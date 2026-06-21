"""
code.py — The "Code" package: development-workflow OS Instances.

See Documentation/04-packages.md §4.3. Extra Lib for this package lives
under lib/python/ and lib/native/ in this same folder — Code is one of
the packages expected to lean more on lib/native/ for performance- or
system-level helpers (e.g. compiler/build tooling), per
Documentation/09-project-structure.md §9.4.
"""

from __future__ import annotations

PACKAGE_NAME = "Code"

# Default config.json overrides applied when an OS Instance declares
# package="Code" in its manifest.json.
DEFAULT_CONFIG = {
    "isolation": {
        # Code instances may run builds/compilers, which can be
        # CPU-bound for a while — no default CPU cap, but a higher
        # memory ceiling than Home.
        "default_cpu_limit": None,
        "default_memory_limit": 1024 * 1024 * 1024,  # 1 GiB
    },
}


def cmd_status(scheduler, instance_id: str) -> str:
    """
    Example Code-specific command, following the same pattern as
    home.py's cmd_status. Real Code commands (build tooling,
    compiler-related helpers) will be filled in alongside actual Code
    use cases.
    """
    for entry in scheduler.list():
        if entry["id"] == instance_id:
            return f"{entry['name']} ({entry['package']}) is {entry['state'].lower()}"
    return f"no instance found with id '{instance_id}'"


# Extra CMD exposed by this package, on top of Core CMD (see
# Documentation/05-cli-commands.md §5.5).
COMMANDS = {
    "status": cmd_status,
}
