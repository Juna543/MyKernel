# 1. Overview

## 1.1 What is MyKernel?

MyKernel is a **hybrid kernel** — a system that behaves like a kernel (managing processes, resources, and execution) while itself running **on top of Python**, which in turn runs **on top of a host OS** (Linux, Windows, or macOS).

It is called "hybrid" because of this dual position:

- From the host OS's point of view, MyKernel is just a regular Python process.
- From the point of view of an OS Instance running inside it, MyKernel acts as a **real kernel** — providing basic services (file I/O, process management, resources) the way an OS kernel normally would.

## 1.2 The Problem It Solves

MyKernel is designed as the foundation for an **OS Sandbox** — a lightweight Dev VM. Instead of building an OS from scratch (which requires its own kernel, drivers, and toolchain), a user can:

1. **Create** a new OS by reusing the libraries and commands MyKernel already provides.
2. **Run** that OS in a fully isolated space, separate from any other OS instance.

Every OS that gets created has a specific purpose ("what is this OS for") — these are purpose-built OS instances, not a single general-purpose OS repeated everywhere.

## 1.3 Key Terminology

| Term | Definition |
|---|---|
| **MyKernel** | The core kernel that provides libraries, commands, and base services (read-only — an OS Instance can call them, but cannot modify them). |
| **OS Instance** | An "OS" created and run on top of MyKernel, packaged as a `.justmko` file. |
| **.justmko** | *Just MyKernel OS* — the file format for an OS Instance, conceptually similar to a `.iso`. See [03-justmko-format.md](03-justmko-format.md). |
| **Package** | A built-in OS preset: Work, Code, or Home. See [04-packages.md](04-packages.md). |
| **mk_bmd** | A separate builder tool (not part of the runtime) used by developers to compile an OS project into a `.justmko` file. See [06-builder-mk_bmd.md](06-builder-mk_bmd.md). |
| **Isolation Layer** | The C++ layer that guarantees each OS Instance runs in a genuinely separate space, not just a logically separate one. |
| **JustIT** | The owner and developer of the MyKernel project. |

## 1.4 v0.1 Beta Design Principles

1. **Python as the main brain** — all logic, CLI, and orchestration is written in Python.
2. **C++ for anything that needs low-level control** — process isolation and cryptography, called from Python through `pybind11`.
3. **Real isolation, not symbolic isolation** — every OS Instance runs in a genuinely separate host process space.
4. **Core Lib & CMD are read-only** — an OS Instance can call MyKernel's base functions, but cannot modify them.
5. **CLI first, GUI later** — v0.1 interaction is entirely command-line based. A GUI is planned for v3.0.
6. **No rushing** — v0.1 Beta is focused on a solid architectural foundation before adding advanced features (e.g. a full licensing system, GUI, broader platform support).

## 1.5 High-Level Usage Flow

```
Developer                          End User
    │                                  │
    │ mk_bmd.build()                   │
    │ (Cython, compiles the project)   │
    ▼                                  │
mycoolos.justmko  ─────────────────────►│
                                        │
                                        │ run mycoolos.justmko
                                        ▼
                                  MyKernel:
                                  1. Read manifest.json
                                  2. Verify payload SHA-256
                                  3. Decrypt payload (AES, via C++)
                                  4. Boot the OS Instance in an
                                     isolated process (via C++)
```

---
Continue to [02-architecture.md](02-architecture.md) for system architecture details.
