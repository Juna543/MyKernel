"""
manifest.py — Reads the open manifest.json portion of a .justmko file,
without touching the encrypted payload.

See Documentation/03-justmko-format.md for the full .justmko structure:

    mycoolos.justmko   (ZIP)
    ├── manifest.json    <- this module reads this
    ├── payload.bin       <- handled by crypto.py + the native module
    └── payload.sha256
"""

from __future__ import annotations

import json
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

MANIFEST_FILENAME = "manifest.json"
PAYLOAD_FILENAME = "payload.bin"
PAYLOAD_HASH_FILENAME = "payload.sha256"

VALID_PACKAGES = ("Work", "Code", "Home")


class ManifestError(Exception):
    """Raised when a .justmko's manifest.json is missing or invalid."""


@dataclass
class Manifest:
    name: str
    version: str
    package: str
    description: str = ""
    source: str = ""  # "load from where" per the original design discussion
    raw: dict[str, Any] | None = None

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Manifest":
        missing = [k for k in ("name", "version", "package") if k not in data]
        if missing:
            raise ManifestError(f"manifest.json is missing required field(s): {', '.join(missing)}")

        package = data["package"]
        if package not in VALID_PACKAGES:
            raise ManifestError(
                f"manifest.json has unknown package '{package}', expected one of {VALID_PACKAGES}"
            )

        return cls(
            name=data["name"],
            version=data["version"],
            package=package,
            description=data.get("description", ""),
            source=data.get("source", ""),
            raw=data,
        )

    def to_dict(self) -> dict[str, Any]:
        base = dict(self.raw or {})
        base.update(
            {
                "name": self.name,
                "version": self.version,
                "package": self.package,
                "description": self.description,
                "source": self.source,
            }
        )
        return base


def read_manifest(justmko_path: str | Path) -> Manifest:
    """
    Open `justmko_path` as a ZIP archive and read just manifest.json,
    WITHOUT decrypting payload.bin. This is what powers the `inspect`
    CLI command (see Documentation/05-cli-commands.md).
    """
    path = Path(justmko_path)
    if not path.exists():
        raise ManifestError(f".justmko file not found: '{path}'")

    try:
        with zipfile.ZipFile(path, "r") as zf:
            if MANIFEST_FILENAME not in zf.namelist():
                raise ManifestError(f"'{path}' does not contain {MANIFEST_FILENAME}")
            with zf.open(MANIFEST_FILENAME) as f:
                data = json.load(f)
    except zipfile.BadZipFile as exc:
        raise ManifestError(f"'{path}' is not a valid .justmko (not a valid ZIP archive)") from exc
    except json.JSONDecodeError as exc:
        raise ManifestError(f"'{path}' has an invalid {MANIFEST_FILENAME}: {exc}") from exc

    if not isinstance(data, dict):
        raise ManifestError(f"'{path}': {MANIFEST_FILENAME} must contain a JSON object")

    return Manifest.from_dict(data)


def read_payload_bytes(justmko_path: str | Path) -> bytes:
    """Read the raw (still-encrypted) payload.bin bytes from a .justmko."""
    path = Path(justmko_path)
    with zipfile.ZipFile(path, "r") as zf:
        if PAYLOAD_FILENAME not in zf.namelist():
            raise ManifestError(f"'{path}' does not contain {PAYLOAD_FILENAME}")
        return zf.read(PAYLOAD_FILENAME)


def read_payload_hash(justmko_path: str | Path) -> str:
    """Read the expected SHA-256 hex digest from payload.sha256."""
    path = Path(justmko_path)
    with zipfile.ZipFile(path, "r") as zf:
        if PAYLOAD_HASH_FILENAME not in zf.namelist():
            raise ManifestError(f"'{path}' does not contain {PAYLOAD_HASH_FILENAME}")
        return zf.read(PAYLOAD_HASH_FILENAME).decode("ascii").strip()
