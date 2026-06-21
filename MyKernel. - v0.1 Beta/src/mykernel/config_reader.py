"""
config_reader.py — Reads MyKernel's global config.json.

See Documentation/07-config.md for the documented schema. This module
deliberately keeps a permissive, defaulted read: missing keys fall back
to sane defaults rather than raising, since config.json fields are
expected to grow over time.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


DEFAULT_CONFIG_FILENAME = "config.json"


class ConfigError(Exception):
    """Raised when config.json exists but cannot be parsed."""


@dataclass
class IsolationDefaults:
    default_cpu_limit: int | None = None
    default_memory_limit: int | None = None


@dataclass
class KernelConfig:
    kernel_version: str = "0.1.0-beta"
    owner: str = "JustIT"
    isolation: IsolationDefaults = field(default_factory=IsolationDefaults)
    enabled_packages: list[str] = field(default_factory=lambda: ["Work", "Code", "Home"])
    log_level: str = "info"

    # Keeps any extra/unknown keys instead of silently dropping them,
    # so round-tripping (read -> write) doesn't lose data as the schema
    # grows.
    raw: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "KernelConfig":
        isolation_data = data.get("isolation", {}) or {}
        isolation = IsolationDefaults(
            default_cpu_limit=isolation_data.get("default_cpu_limit"),
            default_memory_limit=isolation_data.get("default_memory_limit"),
        )
        packages_data = data.get("packages", {}) or {}

        return cls(
            kernel_version=data.get("kernel_version", cls.kernel_version),
            owner=data.get("owner", cls.owner),
            isolation=isolation,
            enabled_packages=packages_data.get("enabled", ["Work", "Code", "Home"]),
            log_level=(data.get("logging", {}) or {}).get("level", "info"),
            raw=data,
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            **self.raw,
            "kernel_version": self.kernel_version,
            "owner": self.owner,
            "isolation": {
                "default_cpu_limit": self.isolation.default_cpu_limit,
                "default_memory_limit": self.isolation.default_memory_limit,
            },
            "packages": {"enabled": self.enabled_packages},
            "logging": {"level": self.log_level},
        }


def load_config(path: str | Path = DEFAULT_CONFIG_FILENAME) -> KernelConfig:
    """
    Load config.json from `path`. If the file doesn't exist, returns a
    KernelConfig with built-in defaults (does not create the file).
    """
    config_path = Path(path)

    if not config_path.exists():
        return KernelConfig()

    try:
        with config_path.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as exc:
        raise ConfigError(f"config.json at '{config_path}' is not valid JSON: {exc}") from exc

    if not isinstance(data, dict):
        raise ConfigError(f"config.json at '{config_path}' must contain a JSON object at the top level")

    return KernelConfig.from_dict(data)


def save_config(config: KernelConfig, path: str | Path = DEFAULT_CONFIG_FILENAME) -> None:
    """Write `config` back out to `path` as pretty-printed JSON."""
    config_path = Path(path)
    with config_path.open("w", encoding="utf-8") as f:
        json.dump(config.to_dict(), f, indent=2)
        f.write("\n")
