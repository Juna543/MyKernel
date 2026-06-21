"""
crypto.py — Payload I/O and light validation around a .justmko's
encrypted payload.

IMPORTANT (see Documentation/09-project-structure.md §9.3): this module
does NOT implement any cryptography itself. All AES/SHA-256 logic lives
in the native C++ module (src/native/crypto.cpp), exposed here as
`mykernel_native.PayloadCrypto`. This module's job is strictly:

  1. Read payload.bin / payload.sha256 from a .justmko (via manifest.py)
  2. Perform basic format/presence validation
  3. Hand the raw bytes to the native module and return its result

If `mykernel_native` hasn't been built yet (see BUILD.md), importing
this module still works, but calling its functions raises
NativeModuleUnavailable with a clear message — so the rest of the
Python layer can be developed/tested independently of the native build.
"""

from __future__ import annotations

from pathlib import Path

from . import manifest as manifest_mod

try:
    import mykernel_native  # type: ignore
    _NATIVE_AVAILABLE = True
    _NATIVE_IMPORT_ERROR: Exception | None = None
except ImportError as exc:  # pragma: no cover - exercised only when native isn't built
    _NATIVE_AVAILABLE = False
    _NATIVE_IMPORT_ERROR = exc


class NativeModuleUnavailable(Exception):
    """
    Raised when an operation needs mykernel_native but it hasn't been
    built. See BUILD.md for build instructions.
    """


class PayloadValidationError(Exception):
    """Raised for basic, pre-crypto validation failures (e.g. empty payload)."""


def native_module_available() -> bool:
    return _NATIVE_AVAILABLE


def _require_native() -> "mykernel_native.PayloadCrypto":  # type: ignore[name-defined]
    if not _NATIVE_AVAILABLE:
        raise NativeModuleUnavailable(
            "mykernel_native is not built/importable. "
            "Run the native build first — see BUILD.md. "
            f"(import error: {_NATIVE_IMPORT_ERROR})"
        )
    return mykernel_native.PayloadCrypto()


def _validate_payload_bytes(data: bytes) -> None:
    if not data:
        raise PayloadValidationError("payload.bin is empty")
    # AES-CBC payloads produced by this project are always IV (16 bytes)
    # + at least one ciphertext block (16 bytes) = minimum 32 bytes.
    if len(data) < 32:
        raise PayloadValidationError(
            f"payload.bin is too short ({len(data)} bytes) to be a valid AES-CBC payload"
        )


def load_and_decrypt_payload(justmko_path: str | Path) -> bytes:
    """
    Full read path for a .justmko's payload:
      1. Read payload.bin and payload.sha256 (I/O, this module)
      2. Light validation (this module)
      3. Verify SHA-256 + decrypt (native module)

    Raises PayloadValidationError, mykernel_native.IntegrityError, or
    mykernel_native.DecryptionError as appropriate. See
    Documentation/03-justmko-format.md §3.6 for the documented flow.
    """
    payload_bytes = manifest_mod.read_payload_bytes(justmko_path)
    expected_hash = manifest_mod.read_payload_hash(justmko_path)

    _validate_payload_bytes(payload_bytes)

    crypto = _require_native()
    return crypto.verify_and_decrypt(payload_bytes, expected_hash)


def encrypt_payload(plaintext: bytes) -> tuple[bytes, str]:
    """
    Encrypts `plaintext` and returns (ciphertext, sha256_hex_of_ciphertext)
    ready to be written as payload.bin / payload.sha256. Used by mk_bmd
    at build time (see tools/mk_bmd/builder.pyx).
    """
    if not plaintext:
        raise PayloadValidationError("cannot encrypt an empty payload")

    crypto = _require_native()
    ciphertext = crypto.encrypt(plaintext)
    digest = mykernel_native.PayloadCrypto.sha256_hex(ciphertext)  # type: ignore[name-defined]
    return ciphertext, digest


def verify_only(justmko_path: str | Path) -> bool:
    """
    Check payload.bin's integrity against payload.sha256 WITHOUT
    decrypting. Powers the `verify` CLI command
    (see Documentation/05-cli-commands.md).
    """
    payload_bytes = manifest_mod.read_payload_bytes(justmko_path)
    expected_hash = manifest_mod.read_payload_hash(justmko_path)

    if not _NATIVE_AVAILABLE:
        raise NativeModuleUnavailable(
            "mykernel_native is not built/importable. Run the native build first — see BUILD.md."
        )

    return mykernel_native.PayloadCrypto.verify_sha256(payload_bytes, expected_hash)  # type: ignore[name-defined]
