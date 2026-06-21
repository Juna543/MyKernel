# BUILD.md — Building and Testing MyKernel

This guide covers building the native C++ module (`mykernel_native`),
compiling the `mk_bmd` builder tool, and running the test suite.

> This was written for **Alpine Linux under WSL** (your environment),
> since Alpine uses `apk` and `musl libc` rather than `apt`/`glibc`. A
> short MinGW/native-Windows note is included at the end for reference,
> but Alpine WSL is the recommended path — Linux-style tooling is far
> more consistent for CMake/pybind11/OpenSSL than native Windows is.

## 0. Why a Build Step Is Needed at All

Two pieces of this project are **not pure Python** and must be compiled
before they can be imported:

| Module | Language | Compiled into | Required by |
|---|---|---|---|
| `mykernel_native` | C++ (`src/native/`) | A Python extension (`.so` file) | `crypto.py`, `scheduler.py`, `cli.py`'s `run`/`verify`/`stop`/`list` commands |
| `mk_bmd.builder` | Cython (`tools/mk_bmd/builder.pyx`) | A Python extension (`.so` file) | `mk_bmd.build()` / `mk_bmd.scaffold()` |

Everything else (`config_reader.py`, `manifest.py`, `cli.py`'s `inspect`/
`delete` commands) is plain Python and needs no build step — you can
already run those today.

**Build order matters**: `mykernel_native` must be built **first**,
because `builder.pyx` imports it directly (see
Documentation/06-builder-mk_bmd.md — mk_bmd reuses the same crypto
module as the runtime, per the project's decision log).

```
1. Build mykernel_native  (C++/pybind11/CMake)
2. Build mk_bmd.builder   (Cython, imports mykernel_native)
3. Run tests              (pytest)
```

---

## 1. Install Build Tools (Alpine WSL)

Open your Alpine WSL terminal and run:

```sh
sudo apk update
sudo apk add build-base cmake python3 python3-dev py3-pip openssl-dev git
```

What each package is for:
- `build-base` — gcc/g++, make, and friends (Alpine doesn't include these by default)
- `cmake` — the build system used by `src/native/CMakeLists.txt`
- `python3-dev` — Python headers, needed to build any Python extension
- `py3-pip` — to install pybind11, Cython, pytest
- `openssl-dev` — the OpenSSL headers/library used by `crypto.cpp` (`<openssl/evp.h>`, etc.)

Verify versions:

```sh
g++ --version
cmake --version
python3 --version
```

This project targets **C++17** and **CMake ≥ 3.15** — recent Alpine
packages satisfy both comfortably.

## 2. Set Up a Python Virtual Environment

Alpine's `pip` is externally managed by default, so a venv keeps things clean:

```sh
cd "/mnt/d/Documents/MainCodeProjects/MyKernel - JustIT/MyKernel - v0.1 Beta"
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install pybind11 cython pytest
```

> Adjust the `cd` path above to wherever your `MyKernel - v0.1 Beta`
> folder actually lives under `/mnt/d/...` in WSL. Remember the folder
> name has spaces — keep it quoted, as shown.

Keep this venv activated for every step below (`source .venv/bin/activate`
in any new terminal).

## 3. Build `mykernel_native` (C++)

```sh
cd src/native
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

If this succeeds, `CMakeLists.txt` is configured to place the resulting
extension directly into `src/mykernel/`, so Python can find it via
`import mykernel_native` without any extra path setup. Confirm:

```sh
ls ../../mykernel/mykernel_native*.so
```

### Verify it imports correctly

```sh
cd ../../..        # back to project root
source .venv/bin/activate
python3 -c "
import sys; sys.path.insert(0, 'src')
import mykernel_native
print('mykernel_native imported OK')
print(mykernel_native.PayloadCrypto.sha256_hex(b''))
"
```

Expected output:
```
mykernel_native imported OK
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
```
(that hex string is the known SHA-256 of an empty input — a good sanity check that hashing works correctly)

### Common build errors

| Error | Likely cause | Fix |
|---|---|---|
| `Could not find a package configuration file provided by "pybind11"` | pybind11 installed via pip isn't visible to CMake | Run `cmake .. -Dpybind11_DIR=$(python3 -m pybind11 --cmakedir)` instead |
| `fatal error: openssl/evp.h: No such file or directory` | `openssl-dev` not installed | `sudo apk add openssl-dev`, then re-run cmake |
| `undefined reference to EVP_...` at link time | OpenSSL found but not linked | Check `find_package(OpenSSL REQUIRED)` succeeded in the cmake output; re-run `cmake ..` after installing `openssl-dev` |

## 4. Build `mk_bmd` (Cython)

Must be done **after** step 3, since `builder.pyx` imports `mykernel_native`
at module load time.

```sh
cd tools/mk_bmd
python setup.py build_ext --inplace
```

This produces `builder*.so` next to `builder.pyx`. Verify:

```sh
cd ../..   # project root
python3 -c "
import sys; sys.path.insert(0, 'src'); sys.path.insert(0, 'tools')
import mk_bmd
print('mk_bmd available:', mk_bmd.builder_available())
"
```

Expected: `mk_bmd available: True`

## 5. Run the Test Suite

```sh
cd "MyKernel - v0.1 Beta"   # project root, if not already there
source .venv/bin/activate
PYTHONPATH=src pytest tests/ -v
```

With both `mykernel_native` and `mk_bmd` built, all tests in
`tests/test_core.py` should run (none skipped). If only
`mykernel_native` is built so far, tests under `mk_bmd` build/scaffold
flows may still be skipped until you complete step 4.

Tests marked `@pytest.mark.skipif(not native available)` will
automatically switch from SKIPPED to PASSING once the native module is
in place — no test code changes needed.

## 6. End-to-End Smoke Test (Manual)

A full manual run-through, once everything above is built:

```sh
source .venv/bin/activate
export PYTHONPATH=src:tools

# 1. Scaffold a new OS project
python3 -c "
import mk_bmd
path = mk_bmd.scaffold('hello-home-os', output_dir='examples', package='Home')
print('Scaffolded at:', path)
"

# 2. Put some content in the payload (the OS's actual filesystem content)
echo "hello from inside hello-home-os" > "examples/hello-home-os/payload_src/hello.txt"

# 3. Build it into a .justmko
python3 -c "
import mk_bmd
out = mk_bmd.build('examples/hello-home-os')
print('Built:', out)
"

# 4. Inspect the manifest (no native module needed for this step)
python3 -m mykernel.cli inspect examples/hello-home-os.justmko

# 5. Verify payload integrity
python3 -m mykernel.cli verify examples/hello-home-os.justmko

# 6. Run it
python3 -m mykernel.cli run examples/hello-home-os.justmko
```

> Note on step 6: as flagged directly in `cli.py`'s `cmd_run`, staging
> the decrypted payload back onto disk as an actual runnable OS
> Instance filesystem is **not yet implemented** — `run` currently
> verifies, decrypts, and starts a placeholder process
> (`/bin/true`) via `IsolationManager` to prove the pipeline works
> end-to-end. See Documentation/08-roadmap.md "Open Questions" for
> this as a tracked next step.

---

## Appendix: Native Windows (MinGW) Notes

Alpine WSL is recommended over native Windows/MinGW for this project,
mainly because finding pybind11/OpenSSL via CMake tends to be far less
finicky on Linux. If you do want to try MinGW directly on Windows:

- Install [MSYS2](https://www.msys2.org/) (a better-maintained MinGW
  distribution than standalone MinGW).
- In an MSYS2 MinGW64 shell:
  ```sh
  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-openssl mingw-w64-x86_64-python mingw-w64-x86_64-python-pip
  pip install pybind11 cython pytest
  ```
- The `IsolationManager` implementation in `src/native/isolation.cpp`
  uses **POSIX APIs** (`fork`, `execvp`, `waitpid`, `setrlimit`) which
  **are available under MSYS2/MinGW's POSIX layer**, but have not been
  validated on native Windows as part of this v0.1 pass. WSL/Alpine
  remains the supported target for v0.1 — treat MinGW as experimental
  until this is verified.

---
Back to [Documentation/README.md](Documentation/README.md) for the full documentation index.
