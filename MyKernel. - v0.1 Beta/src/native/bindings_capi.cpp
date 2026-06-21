// bindings_capi.cpp
//
// Drop-in alternative to bindings.cpp for environments where pybind11
// is not available offline (this container has no network access and
// no pybind11 headers installed — see BUILD.md for the normal pybind11
// path, which remains the supported route on your Alpine WSL machine).
//
// This file uses the raw CPython C-API instead of pybind11, but exposes
// the EXACT SAME Python-level surface as bindings.cpp:
//   mykernel_native.PayloadCrypto   (sha256_hex, verify_sha256, encrypt,
//                                     decrypt, verify_and_decrypt)
//   mykernel_native.IsolationManager (create_instance, run_instance,
//                                      stop_instance, get_instance_info,
//                                      list_instances, remove_instance)
//   mykernel_native.IntegrityError / DecryptionError / IsolationError
//   mykernel_native.InstanceState (CREATED/RUNNING/STOPPED/FAILED)
//
// crypto.cpp and isolation.cpp (the actual logic) are untouched and
// unaware this file even exists — only the binding layer differs.

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#include "crypto.hpp"
#include "isolation.hpp"

#include <new>
#include <vector>
#include <string>

using namespace mykernel;

// ---------------------------------------------------------------------
// Module-level exception objects
// ---------------------------------------------------------------------

static PyObject* IntegrityErrorType = nullptr;
static PyObject* DecryptionErrorType = nullptr;
static PyObject* IsolationErrorType = nullptr;

static Bytes pybytes_to_vector(PyObject* obj) {
    char* buf = nullptr;
    Py_ssize_t len = 0;
    if (PyBytes_AsStringAndSize(obj, &buf, &len) < 0) {
        throw std::invalid_argument("expected a bytes object");
    }
    return Bytes(buf, buf + len);
}

static PyObject* vector_to_pybytes(const Bytes& data) {
    return PyBytes_FromStringAndSize(
        reinterpret_cast<const char*>(data.data()),
        static_cast<Py_ssize_t>(data.size()));
}

// ---------------------------------------------------------------------
// PayloadCrypto wrapper object
// ---------------------------------------------------------------------

struct PyPayloadCrypto {
    PyObject_HEAD
    PayloadCrypto* impl;
};

static int PayloadCrypto_init(PyPayloadCrypto* self, PyObject*, PyObject*) {
    try {
        self->impl = new PayloadCrypto();
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return -1;
    }
    return 0;
}

static void PayloadCrypto_dealloc(PyPayloadCrypto* self) {
    delete self->impl;
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

static PyObject* PayloadCrypto_sha256_hex(PyObject*, PyObject* args) {
    PyObject* data_obj;
    if (!PyArg_ParseTuple(args, "S", &data_obj)) return nullptr;
    try {
        std::string hex = PayloadCrypto::sha256_hex(pybytes_to_vector(data_obj));
        return PyUnicode_FromString(hex.c_str());
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_ValueError, e.what());
        return nullptr;
    }
}

static PyObject* PayloadCrypto_verify_sha256(PyObject*, PyObject* args) {
    PyObject* data_obj;
    const char* expected_hex;
    if (!PyArg_ParseTuple(args, "Ss", &data_obj, &expected_hex)) return nullptr;
    try {
        bool ok = PayloadCrypto::verify_sha256(pybytes_to_vector(data_obj), expected_hex);
        if (ok) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_ValueError, e.what());
        return nullptr;
    }
}

static PyObject* PayloadCrypto_decrypt(PyPayloadCrypto* self, PyObject* args) {
    PyObject* ct_obj;
    if (!PyArg_ParseTuple(args, "S", &ct_obj)) return nullptr;
    try {
        Bytes result = self->impl->decrypt(pybytes_to_vector(ct_obj));
        return vector_to_pybytes(result);
    } catch (const DecryptionError& e) {
        PyErr_SetString(DecryptionErrorType, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* PayloadCrypto_encrypt(PyPayloadCrypto* self, PyObject* args) {
    PyObject* pt_obj;
    if (!PyArg_ParseTuple(args, "S", &pt_obj)) return nullptr;
    try {
        Bytes result = self->impl->encrypt(pybytes_to_vector(pt_obj));
        return vector_to_pybytes(result);
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* PayloadCrypto_verify_and_decrypt(PyPayloadCrypto* self, PyObject* args) {
    PyObject* ct_obj;
    const char* expected_sha256;
    if (!PyArg_ParseTuple(args, "Ss", &ct_obj, &expected_sha256)) return nullptr;
    try {
        Bytes result = self->impl->verify_and_decrypt(pybytes_to_vector(ct_obj), expected_sha256);
        return vector_to_pybytes(result);
    } catch (const IntegrityError& e) {
        PyErr_SetString(IntegrityErrorType, e.what());
        return nullptr;
    } catch (const DecryptionError& e) {
        PyErr_SetString(DecryptionErrorType, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyMethodDef PayloadCrypto_methods[] = {
    {"sha256_hex", (PyCFunction)PayloadCrypto_sha256_hex, METH_VARARGS | METH_STATIC,
     "Compute the SHA-256 hash of data, returned as a lowercase hex string."},
    {"verify_sha256", (PyCFunction)PayloadCrypto_verify_sha256, METH_VARARGS | METH_STATIC,
     "Return True if the SHA-256 of data matches expected_hex."},
    {"decrypt", (PyCFunction)PayloadCrypto_decrypt, METH_VARARGS,
     "Decrypt AES-256-CBC ciphertext (IV-prefixed)."},
    {"encrypt", (PyCFunction)PayloadCrypto_encrypt, METH_VARARGS,
     "Encrypt data with AES-256-CBC. Returns IV-prefixed ciphertext."},
    {"verify_and_decrypt", (PyCFunction)PayloadCrypto_verify_and_decrypt, METH_VARARGS,
     "Verify SHA-256 then decrypt."},
    {nullptr, nullptr, 0, nullptr}
};

static PyTypeObject PayloadCryptoType = []{
    PyTypeObject t{};
    Py_SET_TYPE(&t, nullptr);
    Py_SET_SIZE(&t, 0);
    t.tp_name = "mykernel_native.PayloadCrypto";
    t.tp_basicsize = sizeof(PyPayloadCrypto);
    t.tp_flags = Py_TPFLAGS_DEFAULT;
    t.tp_new = PyType_GenericNew;
    t.tp_init = (initproc)PayloadCrypto_init;
    t.tp_dealloc = (destructor)PayloadCrypto_dealloc;
    t.tp_methods = PayloadCrypto_methods;
    t.tp_doc = "AES-256-CBC + SHA-256 payload cryptography.";
    return t;
}();

// ---------------------------------------------------------------------
// ResourceLimits wrapper object
//
// scheduler.py constructs this as `mykernel_native.ResourceLimits()`
// then assigns attributes directly (limits.max_memory_bytes = ...),
// matching pybind11's def_readwrite style. We mirror that exactly with
// a plain Python-visible struct (members stored as PyObject* so None
// is representable for "no limit").
// ---------------------------------------------------------------------

struct PyResourceLimits {
    PyObject_HEAD
    PyObject* max_memory_bytes; // int or None
    PyObject* max_cpu_seconds;  // int or None
    PyObject* max_open_files;   // int or None
};

static int ResourceLimits_init(PyResourceLimits* self, PyObject*, PyObject*) {
    Py_INCREF(Py_None); self->max_memory_bytes = Py_None;
    Py_INCREF(Py_None); self->max_cpu_seconds = Py_None;
    Py_INCREF(Py_None); self->max_open_files = Py_None;
    return 0;
}

static void ResourceLimits_dealloc(PyResourceLimits* self) {
    Py_XDECREF(self->max_memory_bytes);
    Py_XDECREF(self->max_cpu_seconds);
    Py_XDECREF(self->max_open_files);
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

static PyMemberDef ResourceLimits_members[] = {
    {"max_memory_bytes", T_OBJECT_EX, offsetof(PyResourceLimits, max_memory_bytes), 0, "Max memory in bytes, or None"},
    {"max_cpu_seconds", T_OBJECT_EX, offsetof(PyResourceLimits, max_cpu_seconds), 0, "Max CPU seconds, or None"},
    {"max_open_files", T_OBJECT_EX, offsetof(PyResourceLimits, max_open_files), 0, "Max open files, or None"},
    {nullptr, 0, 0, 0, nullptr}
};

static PyTypeObject ResourceLimitsType = []{
    PyTypeObject t{};
    Py_SET_TYPE(&t, nullptr);
    Py_SET_SIZE(&t, 0);
    t.tp_name = "mykernel_native.ResourceLimits";
    t.tp_basicsize = sizeof(PyResourceLimits);
    t.tp_flags = Py_TPFLAGS_DEFAULT;
    t.tp_new = PyType_GenericNew;
    t.tp_init = (initproc)ResourceLimits_init;
    t.tp_dealloc = (destructor)ResourceLimits_dealloc;
    t.tp_members = ResourceLimits_members;
    t.tp_doc = "Soft resource caps for an OS Instance (None = no explicit limit).";
    return t;
}();

static ResourceLimits pyresourcelimits_to_cpp(PyObject* obj) {
    ResourceLimits limits;
    if (!obj || obj == Py_None) return limits;
    PyObject* v;

    v = PyObject_GetAttrString(obj, "max_memory_bytes");
    if (v) { if (v != Py_None) limits.max_memory_bytes = PyLong_AsUnsignedLongLong(v); Py_DECREF(v); }
    else PyErr_Clear();

    v = PyObject_GetAttrString(obj, "max_cpu_seconds");
    if (v) { if (v != Py_None) limits.max_cpu_seconds = (int)PyLong_AsLong(v); Py_DECREF(v); }
    else PyErr_Clear();

    v = PyObject_GetAttrString(obj, "max_open_files");
    if (v) { if (v != Py_None) limits.max_open_files = (int)PyLong_AsLong(v); Py_DECREF(v); }
    else PyErr_Clear();

    return limits;
}

// ---------------------------------------------------------------------
// IsolationManager wrapper object
// ---------------------------------------------------------------------

struct PyIsolationManager {
    PyObject_HEAD
    IsolationManager* impl;
};

static int IsolationManager_init(PyIsolationManager* self, PyObject*, PyObject*) {
    try {
        self->impl = new IsolationManager();
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return -1;
    }
    return 0;
}

static void IsolationManager_dealloc(PyIsolationManager* self) {
    delete self->impl;
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

// Returns a SimpleNamespace-like object with a `.name` attribute, so
// Python code written for pybind11's py::enum_ (which exposes `.name`
// on enum values, e.g. `info.state.name`) keeps working unchanged.
static PyObject* instance_state_to_py(InstanceState s) {
    const char* name = "CREATED";
    switch (s) {
        case InstanceState::Created: name = "CREATED"; break;
        case InstanceState::Running: name = "RUNNING"; break;
        case InstanceState::Stopped: name = "STOPPED"; break;
        case InstanceState::Failed:  name = "FAILED";  break;
    }
    PyObject* types_mod = PyImport_ImportModule("types");
    if (!types_mod) return PyUnicode_FromString(name); // fallback: plain string
    PyObject* simple_ns_cls = PyObject_GetAttrString(types_mod, "SimpleNamespace");
    Py_DECREF(types_mod);
    if (!simple_ns_cls) return PyUnicode_FromString(name);

    PyObject* kwargs = Py_BuildValue("{s:s}", "name", name);
    PyObject* ns = PyObject_Call(simple_ns_cls, PyTuple_New(0), kwargs);
    Py_DECREF(simple_ns_cls);
    Py_DECREF(kwargs);
    return ns; // has .name == "CREATED"/"RUNNING"/"STOPPED"/"FAILED"
}

static PyObject* instance_info_to_pydict(const InstanceInfo& info) {
    PyObject* d = PyDict_New();
    if (!d) return nullptr;
    PyDict_SetItemString(d, "id", PyUnicode_FromString(info.id.c_str()));
    PyDict_SetItemString(d, "name", PyUnicode_FromString(info.name.c_str()));
    PyDict_SetItemString(d, "pid", PyLong_FromLongLong(info.pid));
    PyDict_SetItemString(d, "state", instance_state_to_py(info.state));
    PyDict_SetItemString(d, "root_path", PyUnicode_FromString(info.root_path.c_str()));
    return d;
}

static PyObject* IsolationManager_create_instance(PyIsolationManager* self, PyObject* args) {
    const char* name;
    const char* root_path;
    if (!PyArg_ParseTuple(args, "ss", &name, &root_path)) return nullptr;
    try {
        std::string id = self->impl->create_instance(name, root_path);
        return PyUnicode_FromString(id.c_str());
    } catch (const IsolationError& e) {
        PyErr_SetString(IsolationErrorType, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* IsolationManager_run_instance(PyIsolationManager* self, PyObject* args, PyObject* kwargs) {
    const char* instance_id;
    const char* executable;
    PyObject* args_list;
    PyObject* limits_obj = nullptr;

    static const char* kwlist[] = {"instance_id", "executable", "args", "limits", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ssO|O", const_cast<char**>(kwlist),
                                      &instance_id, &executable, &args_list, &limits_obj)) {
        return nullptr;
    }

    std::vector<std::string> cpp_args;
    if (args_list && PySequence_Check(args_list)) {
        Py_ssize_t n = PySequence_Size(args_list);
        for (Py_ssize_t i = 0; i < n; ++i) {
            PyObject* item = PySequence_GetItem(args_list, i);
            cpp_args.push_back(PyUnicode_AsUTF8(item));
            Py_DECREF(item);
        }
    }

    // Accepts either a mykernel_native.ResourceLimits instance (the
    // normal case, matching scheduler.py) or a plain dict, for
    // flexibility/testability.
    ResourceLimits limits;
    if (limits_obj && PyDict_Check(limits_obj)) {
        PyObject* v;
        if ((v = PyDict_GetItemString(limits_obj, "max_memory_bytes")) && v != Py_None)
            limits.max_memory_bytes = PyLong_AsUnsignedLongLong(v);
        if ((v = PyDict_GetItemString(limits_obj, "max_cpu_seconds")) && v != Py_None)
            limits.max_cpu_seconds = (int)PyLong_AsLong(v);
        if ((v = PyDict_GetItemString(limits_obj, "max_open_files")) && v != Py_None)
            limits.max_open_files = (int)PyLong_AsLong(v);
    } else if (limits_obj) {
        limits = pyresourcelimits_to_cpp(limits_obj);
    }

    try {
        self->impl->run_instance(instance_id, executable, cpp_args, limits);
        Py_RETURN_NONE;
    } catch (const IsolationError& e) {
        PyErr_SetString(IsolationErrorType, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* IsolationManager_stop_instance(PyIsolationManager* self, PyObject* args, PyObject* kwargs) {
    const char* instance_id;
    int force = 0;
    static const char* kwlist[] = {"instance_id", "force", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|p", const_cast<char**>(kwlist), &instance_id, &force))
        return nullptr;
    try {
        self->impl->stop_instance(instance_id, force != 0);
        Py_RETURN_NONE;
    } catch (const IsolationError& e) {
        PyErr_SetString(IsolationErrorType, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* IsolationManager_get_instance_info(PyIsolationManager* self, PyObject* args) {
    const char* instance_id;
    if (!PyArg_ParseTuple(args, "s", &instance_id)) return nullptr;
    try {
        InstanceInfo info = self->impl->get_instance_info(instance_id);
        return instance_info_to_pydict(info);
    } catch (const IsolationError& e) {
        PyErr_SetString(IsolationErrorType, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* IsolationManager_list_instances(PyIsolationManager* self, PyObject*) {
    try {
        auto instances = self->impl->list_instances();
        PyObject* list = PyList_New(static_cast<Py_ssize_t>(instances.size()));
        for (size_t i = 0; i < instances.size(); ++i) {
            PyList_SET_ITEM(list, i, instance_info_to_pydict(instances[i]));
        }
        return list;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* IsolationManager_remove_instance(PyIsolationManager* self, PyObject* args) {
    const char* instance_id;
    if (!PyArg_ParseTuple(args, "s", &instance_id)) return nullptr;
    try {
        self->impl->remove_instance(instance_id);
        Py_RETURN_NONE;
    } catch (const IsolationError& e) {
        PyErr_SetString(IsolationErrorType, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyMethodDef IsolationManager_methods[] = {
    {"create_instance", (PyCFunction)IsolationManager_create_instance, METH_VARARGS,
     "Register a new OS Instance and prepare its isolated filesystem root."},
    {"run_instance", (PyCFunction)IsolationManager_run_instance, METH_VARARGS | METH_KEYWORDS,
     "Spawn the isolated host process for instance_id."},
    {"stop_instance", (PyCFunction)IsolationManager_stop_instance, METH_VARARGS | METH_KEYWORDS,
     "Stop a running instance."},
    {"get_instance_info", (PyCFunction)IsolationManager_get_instance_info, METH_VARARGS,
     "Return current info/state for a given instance (as a dict)."},
    {"list_instances", (PyCFunction)IsolationManager_list_instances, METH_NOARGS,
     "List all tracked instances (as a list of dicts)."},
    {"remove_instance", (PyCFunction)IsolationManager_remove_instance, METH_VARARGS,
     "Remove a stopped instance's bookkeeping entry."},
    {nullptr, nullptr, 0, nullptr}
};

static PyTypeObject IsolationManagerType = []{
    PyTypeObject t{};
    Py_SET_TYPE(&t, nullptr);
    Py_SET_SIZE(&t, 0);
    t.tp_name = "mykernel_native.IsolationManager";
    t.tp_basicsize = sizeof(PyIsolationManager);
    t.tp_flags = Py_TPFLAGS_DEFAULT;
    t.tp_new = PyType_GenericNew;
    t.tp_init = (initproc)IsolationManager_init;
    t.tp_dealloc = (destructor)IsolationManager_dealloc;
    t.tp_methods = IsolationManager_methods;
    t.tp_doc = "Creates and manages genuinely isolated host processes per OS Instance.";
    return t;
}();

// ---------------------------------------------------------------------
// Module definition
// ---------------------------------------------------------------------

static PyModuleDef mykernel_native_module = {
    PyModuleDef_HEAD_INIT,
    "mykernel_native",
    "MyKernel native module - process isolation (IsolationManager) and "
    "cryptography (PayloadCrypto). Built via raw CPython C-API in this "
    "environment (pybind11 unavailable offline); see Documentation/02-architecture.md.",
    -1,
    nullptr, nullptr, nullptr, nullptr, nullptr
};

PyMODINIT_FUNC PyInit_mykernel_native(void) {
    if (PyType_Ready(&PayloadCryptoType) < 0) return nullptr;
    if (PyType_Ready(&IsolationManagerType) < 0) return nullptr;
    if (PyType_Ready(&ResourceLimitsType) < 0) return nullptr;

    PyObject* m = PyModule_Create(&mykernel_native_module);
    if (!m) return nullptr;

    Py_INCREF(&PayloadCryptoType);
    PyModule_AddObject(m, "PayloadCrypto", (PyObject*)&PayloadCryptoType);

    Py_INCREF(&IsolationManagerType);
    PyModule_AddObject(m, "IsolationManager", (PyObject*)&IsolationManagerType);

    Py_INCREF(&ResourceLimitsType);
    PyModule_AddObject(m, "ResourceLimits", (PyObject*)&ResourceLimitsType);

    IntegrityErrorType = PyErr_NewException("mykernel_native.IntegrityError", nullptr, nullptr);
    Py_INCREF(IntegrityErrorType);
    PyModule_AddObject(m, "IntegrityError", IntegrityErrorType);

    DecryptionErrorType = PyErr_NewException("mykernel_native.DecryptionError", nullptr, nullptr);
    Py_INCREF(DecryptionErrorType);
    PyModule_AddObject(m, "DecryptionError", DecryptionErrorType);

    IsolationErrorType = PyErr_NewException("mykernel_native.IsolationError", nullptr, nullptr);
    Py_INCREF(IsolationErrorType);
    PyModule_AddObject(m, "IsolationError", IsolationErrorType);

    // InstanceState "enum" exposed as simple string constants (CREATED/
    // RUNNING/STOPPED/FAILED), matching the values instance_state_to_py()
    // produces for InstanceInfo.state.
    PyModule_AddStringConstant(m, "STATE_CREATED", "CREATED");
    PyModule_AddStringConstant(m, "STATE_RUNNING", "RUNNING");
    PyModule_AddStringConstant(m, "STATE_STOPPED", "STOPPED");
    PyModule_AddStringConstant(m, "STATE_FAILED", "FAILED");

    return m;
}
