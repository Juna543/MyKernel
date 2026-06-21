"""
mk_bmd/__init__.py — Public API entry point for the mk_bmd builder tool.

Per Documentation/06-builder-mk_bmd.md §6.3, usage looks like:

    import mk_bmd
    mk_bmd.scaffold("mycoolos", output_dir=".", package="Code")
    mk_bmd.build("./mycoolos")

This module is plain Python; the actual build logic lives in
builder.pyx (Cython), compiled separately via setup.py. Importing this
package requires the compiled `builder` extension to be present — see
BUILD.md.
"""

from __future__ import annotations

try:
    from . import builder as _builder  # compiled Cython extension
    _BUILDER_AVAILABLE = True
    _BUILDER_IMPORT_ERROR: Exception | None = None
except ImportError as exc:  # pragma: no cover - exercised only when not yet compiled
    _builder = None
    _BUILDER_AVAILABLE = False
    _BUILDER_IMPORT_ERROR = exc


class BuilderUnavailable(Exception):
    """Raised when builder.pyx hasn't been compiled yet. See BUILD.md."""


def _require_builder():
    if not _BUILDER_AVAILABLE:
        raise BuilderUnavailable(
            "mk_bmd's compiled builder extension is not available. "
            "Run `python setup.py build_ext --inplace` inside tools/mk_bmd/ first — see BUILD.md. "
            f"(import error: {_BUILDER_IMPORT_ERROR})"
        )
    return _builder


def scaffold(name: str, output_dir: str = ".", package: str = "Home") -> str:
    """Create a new OS project skeleton. Returns the project directory path."""
    return _require_builder().scaffold(name, output_dir, package)


def build(project_dir: str, output_path: str | None = None) -> str:
    """Compile an OS project directory into a .justmko file. Returns the output path."""
    return _require_builder().build(project_dir, output_path)


def builder_available() -> bool:
    return _BUILDER_AVAILABLE
