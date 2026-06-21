// isolation.cpp
// IsolationManager implementation — Level A isolation (v0.1 scope):
// separate host process via fork/exec, bounded by setrlimit(), confined
// to a per-instance working directory. See isolation.hpp for the
// documented scope and Documentation/08-roadmap.md for what's deferred
// (namespaces/cgroups/full containers).
//
// POSIX-only (Linux/WSL). Not portable to native Windows as-is — see
// BUILD.md for platform notes.

#include "isolation.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <cstring>

#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

namespace mykernel {

namespace {

std::string generate_instance_id() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dist(rng);
    return oss.str();
}

// Applies resource limits in the CHILD process, right after fork(),
// before exec(). Must only call async-signal-safe functions here.
void apply_limits(const ResourceLimits& limits) {
    if (limits.max_memory_bytes.has_value()) {
        struct rlimit rl;
        rl.rlim_cur = *limits.max_memory_bytes;
        rl.rlim_max = *limits.max_memory_bytes;
        setrlimit(RLIMIT_AS, &rl); // address space cap, closest POSIX equiv to a memory cap
    }
    if (limits.max_cpu_seconds.has_value()) {
        struct rlimit rl;
        rl.rlim_cur = static_cast<rlim_t>(*limits.max_cpu_seconds);
        rl.rlim_max = static_cast<rlim_t>(*limits.max_cpu_seconds);
        setrlimit(RLIMIT_CPU, &rl);
    }
    if (limits.max_open_files.has_value()) {
        struct rlimit rl;
        rl.rlim_cur = static_cast<rlim_t>(*limits.max_open_files);
        rl.rlim_max = static_cast<rlim_t>(*limits.max_open_files);
        setrlimit(RLIMIT_NOFILE, &rl);
    }
}

} // namespace

struct IsolationManager::Impl {
    mutable std::mutex mtx;
    std::map<std::string, InstanceInfo> instances;
};

IsolationManager::IsolationManager() : impl_(new Impl()) {}

IsolationManager::~IsolationManager() {
    delete impl_;
}

std::string IsolationManager::create_instance(const std::string& name, const std::string& root_path) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    std::string id = generate_instance_id();

    std::error_code ec;
    fs::create_directories(root_path, ec);
    if (ec) {
        throw IsolationError("IsolationManager: failed to create instance root '" + root_path + "': " + ec.message());
    }

    InstanceInfo info;
    info.id = id;
    info.name = name;
    info.pid = -1;
    info.state = InstanceState::Created;
    info.root_path = root_path;

    impl_->instances[id] = info;
    return id;
}

void IsolationManager::run_instance(const std::string& instance_id,
                                     const std::string& executable,
                                     const std::vector<std::string>& args,
                                     const ResourceLimits& limits) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    auto it = impl_->instances.find(instance_id);
    if (it == impl_->instances.end()) {
        throw IsolationError("IsolationManager: unknown instance id '" + instance_id + "'");
    }
    InstanceInfo& info = it->second;

    pid_t pid = fork();
    if (pid < 0) {
        throw IsolationError(std::string("IsolationManager: fork() failed: ") + std::strerror(errno));
    }

    if (pid == 0) {
        // --- Child process: becomes the isolated OS Instance ---
        apply_limits(limits);

        if (chdir(info.root_path.c_str()) != 0) {
            _exit(127); // root_path missing/inaccessible — fail fast
        }

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(executable.c_str()));
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        execvp(executable.c_str(), argv.data());
        // execvp only returns on failure.
        _exit(127);
    }

    // --- Parent process: MyKernel itself ---
    info.pid = pid;
    info.state = InstanceState::Running;
}

void IsolationManager::stop_instance(const std::string& instance_id, bool force) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    auto it = impl_->instances.find(instance_id);
    if (it == impl_->instances.end()) {
        throw IsolationError("IsolationManager: unknown instance id '" + instance_id + "'");
    }
    InstanceInfo& info = it->second;

    if (info.state != InstanceState::Running || info.pid <= 0) {
        return; // already stopped, nothing to do
    }

    int sig = force ? SIGKILL : SIGTERM;
    if (kill(static_cast<pid_t>(info.pid), sig) != 0 && errno != ESRCH) {
        throw IsolationError(std::string("IsolationManager: failed to signal pid ") +
                              std::to_string(info.pid) + ": " + std::strerror(errno));
    }

    int status = 0;
    waitpid(static_cast<pid_t>(info.pid), &status, 0);

    info.state = InstanceState::Stopped;
    info.pid = -1;
}

InstanceInfo IsolationManager::get_instance_info(const std::string& instance_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    auto it = impl_->instances.find(instance_id);
    if (it == impl_->instances.end()) {
        throw IsolationError("IsolationManager: unknown instance id '" + instance_id + "'");
    }

    // Opportunistically reap if the child already exited on its own,
    // so callers see an up-to-date state without needing a separate poll.
    InstanceInfo info = it->second;
    if (info.state == InstanceState::Running && info.pid > 0) {
        int status = 0;
        pid_t res = waitpid(static_cast<pid_t>(info.pid), &status, WNOHANG);
        if (res == info.pid) {
            const_cast<InstanceInfo&>(it->second).state =
                WIFEXITED(status) && WEXITSTATUS(status) == 0 ? InstanceState::Stopped : InstanceState::Failed;
            const_cast<InstanceInfo&>(it->second).pid = -1;
            info = it->second;
        }
    }
    return info;
}

std::vector<InstanceInfo> IsolationManager::list_instances() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    std::vector<InstanceInfo> result;
    result.reserve(impl_->instances.size());
    for (const auto& [id, info] : impl_->instances) {
        result.push_back(info);
    }
    return result;
}

void IsolationManager::remove_instance(const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    auto it = impl_->instances.find(instance_id);
    if (it == impl_->instances.end()) {
        throw IsolationError("IsolationManager: unknown instance id '" + instance_id + "'");
    }
    if (it->second.state == InstanceState::Running) {
        throw IsolationError("IsolationManager: cannot remove a running instance, stop it first");
    }
    impl_->instances.erase(it);
}

} // namespace mykernel
