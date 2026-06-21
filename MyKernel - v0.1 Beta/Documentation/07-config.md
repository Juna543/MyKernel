# 7. Kernel Config

## 7.1 Purpose

`config.json` holds **global MyKernel settings** — configuration for the kernel itself, not for any individual OS Instance. It lives alongside the MyKernel installation, separate from any `.justmko` file.

This is different from the `manifest.json` found inside a `.justmko` (see [03-justmko-format.md](03-justmko-format.md)), which only describes that one OS Instance.

| File | Scope | Lives where |
|---|---|---|
| `config.json` | Global, applies to MyKernel itself | MyKernel installation |
| `manifest.json` | Local, applies to a single OS Instance | Inside a `.justmko` file |

## 7.2 Why JSON

JSON was chosen over an `.ini`-style format for being more widely understood and easier to parse consistently across the Python layer and tooling, without needing to support two config formats in parallel.

## 7.3 Draft Structure

> Exact keys are still a draft and will be refined as the Python Layer and C++ module take shape.

```json
{
  "kernel_version": "0.1.0-beta",
  "owner": "JustIT",
  "isolation": {
    "default_cpu_limit": null,
    "default_memory_limit": null
  },
  "packages": {
    "enabled": ["Work", "Code", "Home"]
  },
  "logging": {
    "level": "info"
  }
}
```

This structure is illustrative — it reflects the categories of settings expected (versioning, isolation defaults, enabled packages, logging) rather than a finalized schema.

## 7.4 Relationship to the Architecture

`config.json` sits at the top level of MyKernel Core, as shown in [02-architecture.md §2.1](02-architecture.md#21-system-layer-diagram). It's read once at startup by the Python Layer and used to configure both the Python-side behavior and the parameters passed down to the C++ Native Module (e.g. default isolation limits).

---
Continue to [08-roadmap.md](08-roadmap.md) for the full decision log and open questions.
