// bindings.cpp
// pybind11 module registration. Exposes PayloadCrypto and IsolationManager
// to Python as `mykernel_native` — this is the only file that knows about
// Python at all; crypto.cpp/isolation.cpp are plain C++ with no pybind11
// dependency, which keeps them independently unit-testable in C++
// (see tests/test_native/).

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "crypto.hpp"
#include "isolation.hpp"

namespace py = pybind11;
using namespace mykernel;

namespace {

// Bytes (std::vector<uint8_t>) <-> Python bytes conversion helpers.
// pybind11/stl.h would otherwise convert this to a Python list of ints,
// which is wasteful for payload-sized data — so these are bound
// explicitly as py::bytes at the function boundary instead.

Bytes py_bytes_to_vector(const py::bytes& data) {
    std::string s = data; // pybind11 implicit conversion, copies once
    return Bytes(s.begin(), s.end());
}

py::bytes vector_to_py_bytes(const Bytes& data) {
    return py::bytes(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace

PYBIND11_MODULE(mykernel_native, m) {
    m.doc() = "MyKernel native module — process isolation (IsolationManager) "
               "and cryptography (PayloadCrypto). See "
               "Documentation/02-architecture.md for the architectural role "
               "of this module.";

    // --- Exceptions ---
    py::register_exception<IntegrityError>(m, "IntegrityError");
    py::register_exception<DecryptionError>(m, "DecryptionError");
    py::register_exception<IsolationError>(m, "IsolationError");

    // --- PayloadCrypto ---
    py::class_<PayloadCrypto>(m, "PayloadCrypto")
        .def(py::init<>())
        .def_static("sha256_hex",
            [](const py::bytes& data) {
                return PayloadCrypto::sha256_hex(py_bytes_to_vector(data));
            },
            py::arg("data"),
            "Compute the SHA-256 hash of `data`, returned as a lowercase hex string.")
        .def_static("verify_sha256",
            [](const py::bytes& data, const std::string& expected_hex) {
                return PayloadCrypto::verify_sha256(py_bytes_to_vector(data), expected_hex);
            },
            py::arg("data"), py::arg("expected_hex"),
            "Return True if the SHA-256 of `data` matches `expected_hex`.")
        .def("decrypt",
            [](const PayloadCrypto& self, const py::bytes& ciphertext) {
                return vector_to_py_bytes(self.decrypt(py_bytes_to_vector(ciphertext)));
            },
            py::arg("ciphertext"),
            "Decrypt AES-256-CBC ciphertext (IV-prefixed). Raises DecryptionError on failure.")
        .def("encrypt",
            [](const PayloadCrypto& self, const py::bytes& plaintext) {
                return vector_to_py_bytes(self.encrypt(py_bytes_to_vector(plaintext)));
            },
            py::arg("plaintext"),
            "Encrypt data with AES-256-CBC. Returns IV-prefixed ciphertext.")
        .def("verify_and_decrypt",
            [](const PayloadCrypto& self, const py::bytes& ciphertext, const std::string& expected_sha256) {
                return vector_to_py_bytes(self.verify_and_decrypt(py_bytes_to_vector(ciphertext), expected_sha256));
            },
            py::arg("ciphertext"), py::arg("expected_sha256"),
            "Verify SHA-256 then decrypt. Raises IntegrityError if the hash doesn't match, "
            "before ever attempting decryption.");

    // --- IsolationManager ---
    py::enum_<InstanceState>(m, "InstanceState")
        .value("CREATED", InstanceState::Created)
        .value("RUNNING", InstanceState::Running)
        .value("STOPPED", InstanceState::Stopped)
        .value("FAILED", InstanceState::Failed);

    py::class_<ResourceLimits>(m, "ResourceLimits")
        .def(py::init<>())
        .def_readwrite("max_memory_bytes", &ResourceLimits::max_memory_bytes)
        .def_readwrite("max_cpu_seconds", &ResourceLimits::max_cpu_seconds)
        .def_readwrite("max_open_files", &ResourceLimits::max_open_files);

    py::class_<InstanceInfo>(m, "InstanceInfo")
        .def_readonly("id", &InstanceInfo::id)
        .def_readonly("name", &InstanceInfo::name)
        .def_readonly("pid", &InstanceInfo::pid)
        .def_readonly("state", &InstanceInfo::state)
        .def_readonly("root_path", &InstanceInfo::root_path);

    py::class_<IsolationManager>(m, "IsolationManager")
        .def(py::init<>())
        .def("create_instance", &IsolationManager::create_instance,
            py::arg("name"), py::arg("root_path"),
            "Register a new OS Instance and prepare its isolated filesystem root. "
            "Returns the generated instance id.")
        .def("run_instance", &IsolationManager::run_instance,
            py::arg("instance_id"), py::arg("executable"), py::arg("args"), py::arg("limits"),
            "Spawn the isolated host process for `instance_id`.")
        .def("stop_instance", &IsolationManager::stop_instance,
            py::arg("instance_id"), py::arg("force") = false,
            "Stop a running instance (SIGTERM, or SIGKILL if force=True).")
        .def("get_instance_info", &IsolationManager::get_instance_info,
            py::arg("instance_id"))
        .def("list_instances", &IsolationManager::list_instances)
        .def("remove_instance", &IsolationManager::remove_instance,
            py::arg("instance_id"));
}
