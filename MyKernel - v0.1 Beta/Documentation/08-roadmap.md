# 8. Roadmap

## 8.1 Status

MyKernel is currently in the **architecture discussion phase** for v0.1 Beta. No code has been written yet — this roadmap exists to keep track of what has been decided, what's still open, and what's intentionally deferred.

## 8.2 Decision Log (v0.1 Beta)

| Area | Decision |
|---|---|
| Project name | MyKernel v0.1 Beta |
| Owner | JustIT |
| Runtime language | Python |
| Native module language | C++ (via pybind11) |
| Process isolation | Real isolation, handled by C++ (`IsolationManager`) |
| OS Instance file format | `.justmko` — ZIP containing `manifest.json` (open) + `payload.bin` (AES-encrypted) + `payload.sha256` |
| Integrity verification | SHA-256, checked before decryption |
| Decryption | AES, handled by C++ (`PayloadCrypto`) |
| Kernel config | `config.json` (global MyKernel settings) |
| Built-in packages | Work, Code, Home — share Core Lib, differ in CMD set, extra Lib, and default config |
| Builder tool | `mk_bmd`, written in Cython, `mk_bmd.CMD()` syntax, compiles a project into `.justmko` |
| Runtime CMD style | No prefix — direct commands (`run`, `stop`, `list`, etc.) |
| User-facing interaction (v0.1) | CLI only |
| Documentation language | English |
| `crypto.py` responsibility | Payload I/O + light validation only — all AES/SHA logic lives in C++ (`crypto.cpp`) |
| Package `lib/` structure | Each package has its own `lib/python/` and `lib/native/` — split by language |
| Examples folder | `examples/` at repo root, one minimal example per package (`hello-work-os/`, `hello-code-os/`, `hello-home-os/`) |
| Repository root folder name | `MyKernel - v0.1 Beta` (not a slug-style name) |

## 8.3 Open Questions

These are areas that have a working draft but haven't been fully nailed down yet:

- **Package-specific CMD/Lib detail** — Work/Code/Home each need a concrete list of their extra commands and library functions (currently only categorized at a high level — see [04-packages.md](04-packages.md)).
- **`mk_bmd` API surface** — the exact set of `mk_bmd.CMD()` functions is still a draft (see [06-builder-mk_bmd.md §6.5](06-builder-mk_bmd.md#65-open-questions)).
- **Key management for AES** — the key currently lives inside MyKernel itself. Whether `mk_bmd` (the builder) shares this key or has a separate signing key hasn't been settled.
- **`config.json` schema** — current structure in [07-config.md](07-config.md) is illustrative, not final.
- **Resource limit specifics** — how CPU/memory/file-access limits are actually enforced per OS Instance by `IsolationManager` needs a dedicated technical pass.

## 8.4 Deferred to Future Versions

Explicitly out of scope for v0.1 Beta, noted here so they aren't forgotten:

| Feature | Target | Reason for deferring |
|---|---|---|
| GUI | v3.0 | v0.1 is CLI-only by design; GUI adds significant scope |
| Online license server | Post-v0.1 | Embedded AES key is considered "good enough" friction for Beta |
| Hardware-bound key | Post-v0.1 | Stronger anti-piracy measure, not needed for initial validation |
| Multi-platform isolation parity | Post-v0.1 | v0.1 focuses on getting isolation working, not on matching behavior across every host OS |

## 8.5 Suggested Next Steps

Once this documentation is reviewed and confirmed, reasonable next steps (not yet started) include:

1. Define the concrete `manifest.json` schema (fields, types, required vs optional).
2. Flesh out the Core Lib function list (file I/O, process, console, resource/kernel info — see [02-architecture.md](02-architecture.md)).
3. Detail the package-specific CMD/Lib/config for Work, Code, and Home, including which functions belong in `lib/python/` vs `lib/native/` for each.
4. Prototype the `mykernel_native` C++ module's public interface (function signatures for `IsolationManager` and `PayloadCrypto`) before writing implementation code.
5. Build out the `examples/` projects (`hello-work-os/`, `hello-code-os/`, `hello-home-os/`) once the Core Lib and `mk_bmd` builder are functional enough to produce a working `.justmko`.

The repository folder/build structure itself is now defined — see [09-project-structure.md](09-project-structure.md).

---
This roadmap will be updated as new decisions are made in future discussions.
