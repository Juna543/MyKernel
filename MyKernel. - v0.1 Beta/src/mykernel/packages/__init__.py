"""
packages/__init__.py — Registry for MyKernel's built-in packages.

See Documentation/04-packages.md: a package is NOT a separate kernel —
all packages share the same Core Lib. What differs is the CMD set, a
small amount of extra Lib, and default config (per
Documentation/04-packages.md §4.2).

Each package module (work.py / code.py / home.py) exposes:
  - PACKAGE_NAME: str
  - COMMANDS: dict[str, Callable]   (extra CMD on top of Core CMD)
  - DEFAULT_CONFIG: dict            (default config for this package)
"""

from __future__ import annotations

from importlib import import_module
from types import ModuleType

_PACKAGE_MODULES = {
    "Work": "mykernel.packages.work.work",
    "Code": "mykernel.packages.code.code",
    "Home": "mykernel.packages.home.home",
}


class UnknownPackageError(Exception):
    pass


def load_package(name: str) -> ModuleType:
    """
    Imports and returns the package module for `name` (one of
    "Work", "Code", "Home"), as recorded in a .justmko's manifest.json.
    """
    if name not in _PACKAGE_MODULES:
        raise UnknownPackageError(
            f"unknown package '{name}', expected one of {list(_PACKAGE_MODULES)}"
        )
    return import_module(_PACKAGE_MODULES[name])


def available_packages() -> list[str]:
    return list(_PACKAGE_MODULES)
