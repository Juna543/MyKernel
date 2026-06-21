# MyKernel — v0.1 Beta

A Python-based hybrid kernel for running isolated OS Instances (`.justmko`
files), backed by a native C++ module for process isolation and
cryptography.

Owned and developed by **JustIT**.

> **Status: v0.1 Beta, under active development.** The architecture is
> documented and the first implementation pass is complete; native
> components still need to be compiled in your own environment before
> the full pipeline runs end-to-end. See [BUILD.md](BUILD.md).

## Start Here

- **New to this project?** Read [Documentation/README.md](Documentation/README.md) for the full concept, architecture, and file format documentation.
- **Want to build and run it?** Read [BUILD.md](BUILD.md) for setup, compilation, and test instructions (written for Alpine WSL).
- **Want to see the file layout?** See [Documentation/09-project-structure.md](Documentation/09-project-structure.md).

## What's Implemented So Far

| Component | Status |
|---|---|
| Documentation (architecture, format, roadmap) | ✅ Complete |
| `src/native/` — C++ (`PayloadCrypto`, `IsolationManager`, pybind11 bindings) | ✅ Written, not yet compiled in this environment — see BUILD.md |
| `src/mykernel/` — Python core (CLI, config, manifest, crypto bridge, scheduler) | ✅ Written and tested (Python-only parts) |
| `src/mykernel/packages/home/` | ✅ Written (reference implementation) |
| `src/mykernel/packages/work/`, `.../code/` | ⏳ Not yet implemented — follow the `home/` pattern |
| `tools/mk_bmd/` — Cython builder tool | ✅ Written, not yet compiled |
| `tests/test_core.py` | ✅ Written, Python-only tests verified manually; native-dependent tests pending compilation |
| `tests/test_native/` | ⏳ Not yet written (C++-level unit tests) |
| `examples/` | ⏳ Scaffolding pending a working build |

## Quick Orientation

```
MyKernel - v0.1 Beta/
├── src/mykernel/      ← Python runtime (the "brain")
├── src/native/         ← C++ module: isolation + cryptography
├── tools/mk_bmd/        ← Developer-only builder (Cython)
├── examples/             ← Sample .justmko projects
├── Documentation/         ← Full written documentation
├── tests/                  ← pytest suite
└── sandbox/workspace/       ← Where built/running OS Instances live
```

For the full annotated tree, see [Documentation/09-project-structure.md](Documentation/09-project-structure.md).

## License & Ownership

This project and the `.justmko` format are owned by **JustIT**. License terms TBD.
