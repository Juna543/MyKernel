# 6. Builder — mk_bmd

## 6.1 What is mk_bmd?

`mk_bmd` is a **separate developer tool**, distinct from the MyKernel runtime. Its only job is to compile an OS project into a `.justmko` file. It is **not** something a regular end user installs or runs — it belongs to the development/build side of the workflow.

```
┌────────────────────────────────────────────┐
│  Developer's machine                        │
│                                              │
│  OS project source                          │
│       │                                     │
│       ▼                                     │
│  mk_bmd (Cython)                            │
│       │  mk_bmd.build(...)                  │
│       ▼                                     │
│  mycoolos.justmko                           │
└────────────────────────────────────────────┘
              │
              ▼
   Distributed to end users,
   run via MyKernel's `run` command
```

## 6.2 Why Cython, and Why Separate From the Runtime?

- `mk_bmd` is written in **Cython**, distinct from both the Python runtime layer and the C++ native module of MyKernel itself.
- Keeping it as a separate tool means the build/compile tooling doesn't need to ship inside every MyKernel runtime install — end users only need the runtime, not the builder.

## 6.3 Usage Pattern

The builder is invoked through a `mk_bmd.CMD()`-style API rather than the prefix-free runtime CLI described in [05-cli-commands.md](05-cli-commands.md). For example (illustrative, not finalized):

```python
import mk_bmd

mk_bmd.set_config({
    "name": "mycoolos",
    "package": "Code",
    "version": "0.1.0"
})

mk_bmd.build(source="./mycoolos_project", output="mycoolos.justmko")
```

## 6.4 What the Build Step Produces

Compiling a project with `mk_bmd` results in a `.justmko` file matching the structure defined in [03-justmko-format.md](03-justmko-format.md):

```
mycoolos.justmko
├── manifest.json     (generated from project config, incl. chosen package)
├── payload.bin       (project data, AES-encrypted)
└── payload.sha256    (hash computed over payload.bin)
```

The builder is responsible for:

1. Assembling the project's binary config / filesystem data.
2. Encrypting it into `payload.bin` (AES).
3. Computing and writing `payload.sha256`.
4. Writing the open `manifest.json` (name, version, package, source, description).
5. Packing everything into the final ZIP-based `.justmko` file.

## 6.5 Open Questions

- The exact set of `mk_bmd.CMD()` functions (`build`, `set_config`, etc.) is still a draft and needs a dedicated pass.
- Whether `mk_bmd` shares the AES key/encryption logic with the C++ runtime module, or has its own separate signing path, hasn't been settled yet. See [08-roadmap.md](08-roadmap.md).

---
Continue to [07-config.md](07-config.md) for global kernel configuration.
