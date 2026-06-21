"""
work.py — The "Work" package: productivity/office-style OS Instances.

See Documentation/04-packages.md §4.3. Extra Lib for this package lives
under lib/python/ and lib/native/ in this same folder — Work is one of
the packages expected to lean more on lib/native/ for performance- or
system-level helpers (e.g. document processing), per
Documentation/09-project-structure.md §9.4.
"""

from __future__ import annotations

PACKAGE_NAME = "Work"

# Default config.json overrides applied when an OS Instance declares
# package="Work" in its manifest.json.
DEFAULT_CONFIG = {
    "isolation": {
        "default_cpu_limit": None,
        # Work instances are assumed to handle larger documents/files
        # than a general Home instance, so the default is higher.
        "default_memory_limit": 1024 * 1024 * 1024,  # 1 GiB
    },
}


def cmd_status(scheduler, instance_id: str) -> str:
    """
    Example Work-specific command, following the same pattern as
    home.py's cmd_status. Real Work commands (document handling,
    office-workflow helpers) will be filled in alongside actual Work
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
