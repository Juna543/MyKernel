# 4. Packages

## 4.1 What a Package Is

A **Package** is a built-in preset that an OS Instance is built on top of. MyKernel ships with three packages:

- **Work**
- **Code**
- **Home**

A package is **not** a separate kernel — every package shares the same Core Lib and the same isolation/cryptography foundation described in [02-architecture.md](02-architecture.md). What differs between packages is narrower than that.

## 4.2 What Actually Differs Between Packages

| Layer | Shared across all packages? | Notes |
|---|---|---|
| Core Lib | ✅ Yes | File I/O, process control, console output, resource/kernel info — identical everywhere |
| Isolation & cryptography (C++ module) | ✅ Yes | Same `IsolationManager` / `PayloadCrypto` regardless of package |
| CMD set | ❌ No | Each package exposes its own command set on top of the shared base |
| Extra Lib | ❌ Partially | A small set of additional library functions specific to the package's purpose |
| Default config | ❌ No | Each package ships its own default `config.json` values |

In short: **same engine, different toolbox and default settings.**

## 4.3 Package Overview

> The exact command names and extra libraries below are placeholders pending a future, more detailed pass — what's fixed at this stage is the *category* of what each package focuses on.

### Work
Oriented toward productivity-style use cases (documents, office-like workflows).

### Code
Oriented toward development workflows (build/test tooling, code-related utilities).

### Home
Oriented toward general/personal use cases.

## 4.4 Choosing a Package

The package is selected at **build time**, not at runtime — it's set when a developer compiles a project into a `.justmko` file using `mk_bmd` (see [06-builder-mk_bmd.md](06-builder-mk_bmd.md)), and recorded in `manifest.json` (see [03-justmko-format.md](03-justmko-format.md)).

```json
{
  "name": "mycoolos",
  "package": "Code",
  "version": "0.1.0"
}
```

When MyKernel runs a `.justmko` file, it reads the `package` field from the manifest to determine which CMD set, extra Lib, and default config to load alongside the Core Lib.

## 4.5 Package `lib/` Structure

Each package owns its own `lib/` folder, split by language — see [09-project-structure.md §9.4](09-project-structure.md#94-package-lib-structure):

```
packages/<package_name>/lib/
├── python/   # Pure Python helper libraries
└── native/   # C/C++ helper libraries
```

Splitting `lib/` this way exists because **Code** and **Work** in particular tend to need more performance-sensitive or system-level functionality (e.g. compiler-related tooling, document processing) than pure Python conveniently provides — while **Home**, being more general-purpose, is expected to lean more heavily on `lib/python/`.

Having a dedicated `lib/` folder per package (rather than a single file like `work.py`) also makes it easier to extend a package over time — new library functions can be dropped in as new files instead of growing one large module.

---
Continue to [05-cli-commands.md](05-cli-commands.md) for the end-user CLI command reference.
