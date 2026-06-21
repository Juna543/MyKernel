# 5. CLI Commands

## 5.1 Two Separate Command Worlds

MyKernel has two distinct command surfaces that should not be confused with each other:

| World | Used by | Form | Example |
|---|---|---|---|
| **Runtime CMD** | Regular end users | Direct CLI commands, no `mk` prefix | `run mycoolos.justmko` |
| **Builder CMD** | Developers building an OS | Cython API, `mk_bmd.CMD()` syntax | `mk_bmd.build()` |

This document covers **Runtime CMD** only. For the builder commands, see [06-builder-mk_bmd.md](06-builder-mk_bmd.md).

## 5.2 Design Decision: No `mk` Prefix

Earlier drafts considered prefixing every runtime command with `mk` (e.g. `mk run`, `mk list`). The current decision is that runtime commands are typed **directly**, without a prefix — keeping everyday usage shorter.

```bash
# Not this:
mk run mycoolos.justmko

# This:
run mycoolos.justmko
```

## 5.3 Core Runtime Commands (v0.1 Draft)

| Command | Purpose |
|---|---|
| `create <name>` | Create a new `.justmko` skeleton/instance |
| `run <file.justmko>` | Run an OS Instance (decrypt payload, isolate, boot) |
| `stop <id\|name>` | Stop a currently running OS Instance |
| `list` | List all OS Instances (available and/or running) |
| `inspect <file.justmko>` | Show the manifest (metadata) without decrypting the payload |
| `delete <file.justmko>` | Delete an OS Instance file |
| `verify <file.justmko>` | Check the SHA-256 hash — confirms the file isn't corrupted or tampered with |

> This list reflects the commands discussed so far. It is expected to grow as package-specific commands (Work/Code/Home — see [04-packages.md](04-packages.md)) are defined in more detail.

## 5.4 Execution Flow Behind `run`

`run` is the most involved command — it triggers the full flow described in [02-architecture.md §2.4](02-architecture.md#24-run-execution-flow):

```
run mycoolos.justmko
  → Python reads manifest.json
  → Python hands payload.bin to the C++ module
  → C++ verifies SHA-256
  → C++ decrypts payload.bin with AES
  → C++ spawns an isolated host process
  → OS Instance boots inside that isolated process
```

## 5.5 Command Scope Per Package

Because each package (Work/Code/Home) exposes its own extra commands on top of the shared Core CMD set (see [04-packages.md §4.2](04-packages.md#42-what-actually-differs-between-packages)), the exact list of package-specific commands will be documented separately once finalized.

---
Continue to [06-builder-mk_bmd.md](06-builder-mk_bmd.md) for the developer-side builder tool.
