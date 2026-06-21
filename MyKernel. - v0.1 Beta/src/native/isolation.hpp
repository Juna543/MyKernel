// isolation.hpp
// IsolationManager — creates and manages a genuinely separate host
// process for each OS Instance (per Documentation/02-architecture.md §2.3:
// "real isolation, not symbolic isolation").
//
// v0.1 scope (Linux/WSL target): isolation is implemented using POSIX
// fork/exec plus resource limits (rlimit) and a restricted working
// directory. This is NOT full container-grade isolation (no namespaces/
// cgroups yet) — it guarantees separate process memory space, a bounded
// resource budget, and a confined filesystem root, which is the
// documented v0.1 bar. See Documentation/08-roadmap.md for what's
// deferred to later versions.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <optional>

namespace mykernel {

class IsolationError : public std::runtime_error {
public:
    explicit IsolationError(const std::string& msg) : std::runtime_error(msg) {}
};

enum class InstanceState {
    Created,
    Running,
    Stopped,
    Failed
};

struct ResourceLimits {
    // Soft caps for v0.1. std::nullopt means "no explicit limit applied,
    // inherit host default".
    std::optional<uint64_t> max_memory_bytes;
    std::optional<int> max_cpu_seconds;
    std::optional<int> max_open_files;
};

struct InstanceInfo {
    std::string id;            // unique id assigned by IsolationManager
    std::string name;          // OS Instance name, from manifest.json
    int64_t pid = -1;          // host process id once running
    InstanceState state = InstanceState::Created;
    std::string root_path;     // confined working directory for this instance
};

class IsolationManager {
public:
    IsolationManager();
    ~IsolationManager();

    // Registers a new OS Instance and prepares its isolated filesystem
    // root (under the kernel's sandbox/workspace area). Does not start
    // the process yet. Returns the generated instance id.
    std::string create_instance(const std::string& name, const std::string& root_path);

    // Spawns the isolated host process for `instance_id`, running
    // `executable` with `args`, confined to its root_path and bounded by
    // `limits`. Throws IsolationError on failure.
    void run_instance(const std::string& instance_id,
                       const std::string& executable,
                       const std::vector<std::string>& args,
                       const ResourceLimits& limits);

    // Requests termination of a running instance. `force` = true sends
    // SIGKILL instead of SIGTERM.
    void stop_instance(const std::string& instance_id, bool force = false);

    // Returns current info/state for a given instance.
    InstanceInfo get_instance_info(const std::string& instance_id) const;

    // Lists all instances currently tracked (any state).
    std::vector<InstanceInfo> list_instances() const;

    // Removes a stopped instance's bookkeeping entry. Throws if the
    // instance is still running.
    void remove_instance(const std::string& instance_id);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace mykernel
