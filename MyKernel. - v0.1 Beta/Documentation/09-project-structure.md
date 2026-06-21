# 9. Project Structure

## 9.1 Repository Layout

```
MyKernel - v0.1 Beta/
│
├── config.json                 # Global kernel configuration
├── README.md                   # Root repository guide
│
├── src/                        # MAIN SOURCE CODE
│   ├── mykernel/                # Core Runtime Layer (Python)
│   │   ├── __init__.py
│   │   ├── cli.py                # Runtime CLI parser (run, stop, list, etc.)
│   │   ├── config_reader.py      # Reads config.json
│   │   ├── crypto.py             # Payload I/O + validation, calls into native crypto
│   │   ├── manifest.py           # Reads manifest.json from a .justmko
│   │   ├── scheduler.py          # High-level resource & process manager
│   │   └── packages/             # Built-in package presets
│   │       ├── __init__.py
│   │       ├── work/
│   │       │   ├── work.py        # CMD entry points for the Work package
│   │       │   └── lib/
│   │       │       ├── python/    # Work-specific Python libraries
│   │       │       └── native/    # Work-specific C/C++ libraries
│   │       ├── code/
│   │       │   ├── code.py        # CMD entry points for the Code package
│   │       │   └── lib/
│   │       │       ├── python/    # Code-specific Python libraries
│   │       │       └── native/    # Code-specific C/C++ libraries
│   │       └── home/
│   │           ├── home.py        # CMD entry points for the Home package
│   │           └── lib/
│   │               ├── python/    # Home-specific Python libraries
│   │               └── native/    # Home-specific C/C++ libraries
│   │
│   └── native/                  # Native Module Layer (C++)
│       ├── CMakeLists.txt        # Build system for C++/pybind11
│       ├── bindings.cpp          # pybind11 module registration (mykernel_native)
│       ├── isolation.cpp         # IsolationManager implementation
│       ├── isolation.hpp
│       ├── crypto.cpp            # PayloadCrypto implementation (AES & SHA-256)
│       └── crypto.hpp
│
├── tools/                       # DEVELOPER TOOLS
│   └── mk_bmd/                   # Builder Tool (Cython)
│       ├── setup.py               # Cython build script
│       ├── builder.pyx            # Core mk_bmd.build() logic + encryption call
│       └── templates/
│           └── manifest.json      # Skeleton template used by the `create` command
│
├── examples/                    # USAGE EXAMPLES
│   ├── hello-work-os/             # Minimal example OS built on the Work package
│   ├── hello-code-os/             # Minimal example OS built on the Code package
│   └── hello-home-os/             # Minimal example OS built on the Home package
│
├── Documentation/                # OFFICIAL DOCUMENTATION (this folder)
│   ├── README.md                  # Documentation index (TOC)
│   ├── 01-overview.md
│   ├── 02-architecture.md
│   ├── 03-justmko-format.md
│   ├── 04-packages.md
│   ├── 05-cli-commands.md
│   ├── 06-builder-mk_bmd.md
│   ├── 07-config.md
│   ├── 08-roadmap.md
│   └── 09-project-structure.md    # This document
│
├── tests/                        # UNIT TESTING
│   ├── test_core.py               # Tests for CLI & Python-side logic
│   └── test_native/               # Tests for C++ encryption & isolation directly
│
└── sandbox/                      # DEVELOPMENT TRY-OUT AREA
    └── workspace/                  # Output location for built .justmko files
```

## 9.2 Mapping Folders to Architecture Layers

This maps the repository structure back to the layers described in [02-architecture.md](02-architecture.md):

| Architecture Layer | Repository Location |
|---|---|
| Python Layer (main brain) | `src/mykernel/` |
| C++ Native Module (`mykernel_native`) | `src/native/` |
| Package presets (Work/Code/Home) | `src/mykernel/packages/` |
| Builder tool (`mk_bmd`) | `tools/mk_bmd/` |
| Global kernel config | `config.json` (repository root) |
| `.justmko` build output | `sandbox/workspace/` (during development) |

> **Note on the root folder name:** the repository root is named `MyKernel - v0.1 Beta` (matching the project's version label), rather than a slug-style name like `mykernel-project`. Since it contains spaces, remember to quote the path in shell commands and build scripts (e.g. `cd "MyKernel - v0.1 Beta"`).

## 9.3 `crypto.py` vs `crypto.cpp` — Clarified Responsibility

Earlier documentation described `crypto.py` as a pure bridge to the native module. To be precise about the actual split:

| File | Responsibility |
|---|---|
| `src/mykernel/crypto.py` | Reads/writes the `payload.bin` and `payload.sha256` files from the `.justmko` archive, performs light validation (e.g. file presence, basic format checks), and passes data to/from the native module. |
| `src/native/crypto.cpp` | Owns **all core cryptographic operations** — AES encryption/decryption and SHA-256 hashing. This is the only place actual crypto logic lives. |

In short: `crypto.py` is the **I/O and orchestration layer** around cryptography, not a crypto implementation itself. All AES/SHA logic stays in C++, consistent with the isolation/cryptography responsibilities described in [02-architecture.md §2.2](02-architecture.md#22-division-of-responsibility).

## 9.4 Package `lib/` Structure

As detailed further in [04-packages.md](04-packages.md), each package (Work, Code, Home) owns its own `lib/` folder, split by language:

```
packages/<package_name>/lib/
├── python/   # Pure Python helper libraries specific to this package
└── native/   # C/C++ libraries specific to this package (e.g. compiler
              # tooling for Code, document processing helpers for Work)
```

This split exists because packages like **Code** and **Work** are expected to need more performance-sensitive or system-level libraries than a pure-Python implementation would conveniently provide — while still keeping each package's additions self-contained and easy to extend without touching the Core Lib.

## 9.5 Examples Folder

`examples/` contains one minimal, working example `.justmko` project per built-in package:

- `hello-work-os/`
- `hello-code-os/`
- `hello-home-os/`

These exist to give a concrete, runnable starting point alongside the written documentation — useful for anyone who finds the conceptual docs harder to follow without seeing actual files.

---
Back to [README.md](README.md) for the full documentation index.
