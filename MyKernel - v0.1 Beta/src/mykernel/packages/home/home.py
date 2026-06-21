"""
home.py — The "Home" package: general/personal-use OS Instances.

See Documentation/04-packages.md §4.3. Extra Lib for this package lives
under lib/python/ and lib/native/ in this same folder.
"""

from __future__ import annotations

PACKAGE_NAME = "Home"

# Default config.json overrides applied when an OS Instance declares
# package="Home" in its manifest.json. Merged on top of the kernel's
# global config (Documentation/07-config.md).
DEFAULT_CONFIG = {
    "isolation": {
        "default_cpu_limit": None,
        "default_memory_limit": 512 * 1024 * 1024,  # 512 MiB, a reasonable general-purpose default
    },
}


def cmd_status(scheduler, instance_id: str) -> str:
    """
    Example Home-specific command: a friendlier, human-readable status
    line for a single instance. Illustrates the pattern package commands
    follow — real Home commands will be filled in alongside actual Home
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
