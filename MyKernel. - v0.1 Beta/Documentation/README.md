# MyKernel Documentation

Official documentation for **MyKernel** — a Python-based kernel that runs on top of a host OS, backed by a native C++ module for process isolation and cryptography.

> Status: **v0.1 Beta — Architecture Draft.** This is the result of the design/discussion phase. No implementation has started yet. These documents serve as the shared reference before development begins, and will keep evolving as decisions are made.

Owned and developed by **JustIT**.

---

## Table of Contents

| # | Document | Covers |
|---|---|---|
| 1 | [Overview](01-overview.md) | Project vision, core concept, key terminology |
| 2 | [Architecture](02-architecture.md) | System layers, role of Python & C++ |
| 3 | [.justmko Format](03-justmko-format.md) | OS instance file structure, encryption, integrity |
| 4 | [Packages](04-packages.md) | Built-in packages: Work, Code, Home |
| 5 | [CLI Commands](05-cli-commands.md) | Runtime commands for end users |
| 6 | [Builder — mk_bmd](06-builder-mk_bmd.md) | Developer tool for building `.justmko` files |
| 7 | [Kernel Config](07-config.md) | Global MyKernel configuration (`config.json`) |
| 8 | [Roadmap](08-roadmap.md) | v0.1 decision log, open questions, future versions |
| 9 | [Project Structure](09-project-structure.md) | Repository file tree, mapped to the architecture |

---

## Quick Summary

MyKernel is a **hybrid kernel**: the core logic is written in **Python**, while operations that require low-level control — process isolation and cryptography — are handled by a **native C++ module**, bound to Python through **pybind11**.

On top of MyKernel, users can create and run **OS Instances** packaged as `.justmko` files (*Just MyKernel OS*) — an OS image format that relies on MyKernel's built-in libraries and commands instead of shipping its own kernel. Every OS Instance runs **fully isolated** from every other instance.

```
Host OS (Linux / Windows / macOS)
   └─ Python Runtime
        └─ MyKernel
             ├─ Python Layer (logic, CLI, libraries)
             ├─ C++ Native Module (isolation, cryptography)
             └─ OS Instances (.justmko) — isolated
```

Continue to [01-overview.md](01-overview.md) for the full concept walkthrough.
