# NOTE — This Is the Ubuntu Container Build

This folder (`MyKernel. - v0.1 Beta`, with a period after "MyKernel")
is a **separate build** made inside Anthropic's Ubuntu sandbox
container, kept apart from your original Alpine WSL project
(`MyKernel - v0.1 Beta`, no period). Your original source files were
not modified — this is a copy.

## Why a separate native binding was needed

This container has **no network access** and does not have `cmake`,
`pybind11`, or OpenSSL dev headers installed, and they couldn't be
installed (no internet). So the C++ native module here was built with
a different binding layer than your CMake/pybind11 pipeline:

| File | Role |
|---|---|
| `src/native/bindings_capi.cpp` | **New.** Raw CPython C-API binding — a drop-in replacement for `bindings.cpp` (pybind11). Exposes the exact same Python-level API (`PayloadCrypto`, `IsolationManager`, `ResourceLimits`, exceptions). |
| `src/native/bindings.cpp` | **Untouched**, your original pybind11 version. Still the right file to use back on Alpine WSL where pybind11/CMake are available. |
| `src/native/crypto.cpp`, `isolation.cpp` | **Untouched.** Pure C++, no pybind11 dependency either way — these never needed to change. |

## How it was compiled (no CMake, no pybind11, no Cython install)

```sh
# Native module (mykernel_native)
g++ -std=c++17 -O2 -fPIC -shared \
  $(python3-config --includes) -I/usr/include/node \
  bindings_capi.cpp crypto.cpp isolation.cpp \
  /usr/lib/x86_64-linux-gnu/libcrypto.so.3 \
  -o src/mykernel/mykernel_native.so

# mk_bmd builder — reused the builder.c your Alpine session already
# generated via Cython (no Cython install needed here, just compiled
# the already-generated C file):
g++ -O2 -fPIC -shared \
  $(python3-config --includes) \
  tools/mk_bmd/builder.c \
  -o tools/mk_bmd/builder.so
```

OpenSSL headers came from Node.js's bundled copy
(`/usr/include/node/openssl/`), and `libcrypto` was linked directly
against the system's `libcrypto.so.3` (no `-lcrypto` dev symlink was
present).

## Verified working (smoke test)

```
$ PYTHONPATH=src:src/mykernel:tools python3 -m mykernel.cli run examples/hello-test-os.justmko
Loaded manifest: hello-test-os v0.1.0 (package: Home)
Payload verified and decrypted (10240 bytes).
Started instance '945f865d8769abdc' for 'hello-test-os'.
```

Full pipeline confirmed: `scaffold` → `build` → `inspect` → `verify` → `run`.

## Running this build yourself

```sh
export PYTHONPATH=src:src/mykernel:tools
python3 -m mykernel.cli inspect examples/hello-test-os.justmko
python3 -m mykernel.cli verify examples/hello-test-os.justmko
python3 -m mykernel.cli run examples/hello-test-os.justmko
```

Note the `src/mykernel` entry in `PYTHONPATH` — `mykernel_native.so`
lives there, and needs to be directly on the path for the top-level
`import mykernel_native` statements in `crypto.py`/`scheduler.py` to
resolve.

## Back on your Alpine WSL machine

Nothing here changes your original workflow — `BUILD.md`'s CMake/
pybind11 steps are still the supported path there. This container
build exists only because this sandbox session couldn't reach the
internet to install those tools.
