# 2. Architecture

## 2.1 System Layer Diagram

```
┌──────────────────────────────────────────────────────────┐
│  Host OS (Linux / Windows / macOS)                        │
├──────────────────────────────────────────────────────────┤
│  MyKernel Core                                             │
│                                                             │
│  config.json  ← global kernel configuration                │
│                                                             │
│  ┌────────────────────────────────────────────────────┐  │
│  │ Python Layer (main brain, runtime)                   │  │
│  │  - Core Lib (shared by all packages)                  │  │
│  │  - .justmko Manifest Reader                            │  │
│  │  - CLI Interface (direct commands, no prefix)           │  │
│  │  - Scheduler / Resource Manager                          │  │
│  │  - import mykernel_native (C++ module)                    │  │
│  └────────────────────────────────────────────────────┘  │
│                     ▲ pybind11 (binding)                    │
│  ┌────────────────────────────────────────────────────┐  │
│  │ C++ Native Module (mykernel_native)                   │  │
│  │  - IsolationManager  → isolates OS Instance processes   │  │
│  │  - PayloadCrypto     → AES decrypt + SHA verification     │  │
│  └────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────┤
│  OS Instances grouped by Package:                            │
│                                                                │
│  [Work]            [Code]            [Home]                   │
│  - Core Lib         - Core Lib        - Core Lib               │
│  - Work-specific    - Code-specific   - Home-specific            │
│    CMD                CMD               CMD                      │
│  - Extra Lib        - Extra Lib       - Extra Lib                  │
│  - Default config   - Default config  - Default config              │
│  [ISOLATED]         [ISOLATED]        [ISOLATED]                     │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│  SEPARATE: mk_bmd (Builder Tool, developer-only)              │
│  - Written in Cython                                          │
│  - Syntax: mk_bmd.CMD()                                         │
│  - Job: compile an OS project → a .justmko file                  │
│  - NOT used by regular end users                                  │
└──────────────────────────────────────────────────────────┘
```

## 2.2 Division of Responsibility

### Python Layer (main brain)

Python owns all **logic and orchestration**:

- Reads `manifest.json` inside a `.justmko` file (the open, unencrypted part).
- Provides the CLI interface for end users.
- Handles high-level scheduling/resource management (logic only, not the actual isolation).
- Provides the Core Lib plus package-specific libraries (Work/Code/Home).
- Calls into the native C++ module for anything that needs low-level control.

On the cryptography side specifically, the Python-side `crypto.py` module handles **payload I/O and light validation** — reading/writing `payload.bin` and `payload.sha256` from the `.justmko` archive, and basic format checks — while **all actual cryptographic operations (AES encryption/decryption, SHA-256 hashing) remain entirely in the C++ module**. See [09-project-structure.md §9.3](09-project-structure.md#93-cryptopy-vs-cryptocpp--clarified-responsibility) for the exact split.

Python was chosen as the main layer because it's flexible and fast to iterate on — well suited for logic that will keep changing during this early Beta stage.

### C++ Native Module (`mykernel_native`)

Written in C++, wrapped as a Python module via **pybind11**, and imported from Python like any normal module (`import mykernel_native`).

Two core responsibilities:

1. **`IsolationManager`** — creates and manages OS Instance processes at the host level, ensuring each instance is genuinely isolated (not merely separated at the code-structure level).
2. **`PayloadCrypto`** — verifies the SHA-256 hash and decrypts the `.justmko` payload using AES.

C++ was chosen specifically for these two tasks because:

- Process isolation needs more direct access to host OS facilities than Python conveniently offers.
- Cryptographic operations are safer in a native layer — faster in practice, and the decryption logic isn't directly readable the way plain Python source would be.

### Why pybind11?

Three C++-to-Python binding approaches were considered:

| Option | Pros | Cons |
|---|---|---|
| `ctypes` | Built into Python, simplest to start with | Requires `extern "C"` style functions, manual type matching, doesn't scale well |
| **pybind11** ✅ | Natively supports C++ classes/structs/exceptions, scales well as the project grows | Heavier build setup (needs a C++ compiler & CMake) |
| Cython | Syntax close to Python | Better suited for writing new accelerated logic than for binding an existing C++ library |

**pybind11** was chosen because MyKernel is expected to grow a fair number of functions and structures (`IsolationManager`, `PayloadCrypto`, and more to come) — pybind11 gives a cleaner long-term foundation than `ctypes`.

## 2.3 Isolation Between OS Instances

Isolation requirement: **real**, not merely logical — no running OS Instance should be able to interfere with another.

- Every OS Instance runs as a host process managed by `IsolationManager` (C++).
- Resources (file access, memory, CPU) are bounded per instance.
- Python does not handle isolation directly — it only instructs the C++ module to create and manage the isolated space.

## 2.4 `run` Execution Flow

```
1. Python  : read manifest.json from the .justmko (ZIP, open part)
2. Python  : pass payload.bin to the C++ module
3. C++     : compute SHA-256 of payload.bin, compare against payload.sha256
4. C++     : if valid → decrypt payload.bin using AES
5. C++     : return the decrypted result to Python
6. Python  : prepare the execution space via IsolationManager (C++)
7. C++     : spawn an isolated host process, boot the OS Instance inside it
```

If SHA-256 verification fails at step 3, execution stops — the file is treated as corrupted or tampered with, and decryption never happens.

---
Continue to [03-justmko-format.md](03-justmko-format.md) for the `.justmko` file format details.
