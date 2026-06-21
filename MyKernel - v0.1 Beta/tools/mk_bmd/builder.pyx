# builder.pyx — mk_bmd's core build logic.
#
# Developer-only tool (NOT part of the MyKernel runtime). Compiles an
# OS project directory into a .justmko file. See
# Documentation/06-builder-mk_bmd.md.
#
# Reuses mykernel_native directly for AES/SHA — same module the runtime
# imports — so encryption is guaranteed consistent with what `run` can
# decrypt, with no duplicated crypto logic (per project decision log,
# Documentation/08-roadmap.md).
#
# This file is Cython (.pyx) per the original design decision that
# mk_bmd is written in Cython, distinct from both the pure-Python
# runtime and the C++ native module. See setup.py for how it's compiled.

import io
import json
import tarfile
import zipfile
from pathlib import Path

import mykernel_native

MANIFEST_FILENAME = "manifest.json"
PAYLOAD_FILENAME = "payload.bin"
PAYLOAD_HASH_FILENAME = "payload.sha256"

VALID_PACKAGES = ("Work", "Code", "Home")

_TEMPLATES_DIR = Path(__file__).parent / "templates"


class BuildError(Exception):
    pass


cpdef dict default_manifest_template():
    """Loads the skeleton manifest.json template shipped with mk_bmd."""
    template_path = _TEMPLATES_DIR / MANIFEST_FILENAME
    with open(template_path, "r", encoding="utf-8") as f:
        return json.load(f)


cpdef str scaffold(str name, str output_dir, str package="Home"):
    """
    create() equivalent: scaffolds a new OS project directory containing
    a manifest.json (from the template, with `name`/`package` filled in)
    and an empty payload_src/ directory the developer fills with their
    actual OS content before calling build().

    Returns the path to the created project directory.
    """
    if package not in VALID_PACKAGES:
        raise BuildError(f"unknown package '{package}', expected one of {VALID_PACKAGES}")

    project_dir = Path(output_dir) / name
    if project_dir.exists():
        raise BuildError(f"project directory already exists: '{project_dir}'")

    payload_src = project_dir / "payload_src"
    payload_src.mkdir(parents=True)

    manifest = default_manifest_template()
    manifest["name"] = name
    manifest["package"] = package

    with open(project_dir / MANIFEST_FILENAME, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    return str(project_dir)


cdef bytes _pack_payload_source(str payload_src_dir):
    """
    Packs the contents of `payload_src_dir` into a single tar byte
    stream — this becomes the plaintext that gets AES-encrypted into
    payload.bin. Using tar (rather than raw concatenation) preserves
    the directory structure of the OS project's filesystem content.
    """
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w") as tar:
        tar.add(payload_src_dir, arcname=".")
    return buf.getvalue()


cpdef str build(str project_dir, str output_path=None):
    """
    Compiles the OS project at `project_dir` into a .justmko file.

    Steps (per Documentation/06-builder-mk_bmd.md §6.4):
      1. Read and validate manifest.json
      2. Pack payload_src/ into plaintext bytes
      3. Encrypt -> payload.bin (via mykernel_native, same module as runtime)
      4. Compute SHA-256 -> payload.sha256
      5. Pack manifest.json + payload.bin + payload.sha256 into a ZIP (.justmko)
    """
    project_path = Path(project_dir)
    manifest_path = project_path / MANIFEST_FILENAME
    payload_src_path = project_path / "payload_src"

    if not manifest_path.exists():
        raise BuildError(f"'{project_path}' has no {MANIFEST_FILENAME} — not a valid mk_bmd project")
    if not payload_src_path.exists():
        raise BuildError(f"'{project_path}' has no payload_src/ directory")

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    missing = [k for k in ("name", "version", "package") if k not in manifest]
    if missing:
        raise BuildError(f"manifest.json is missing required field(s): {', '.join(missing)}")
    if manifest["package"] not in VALID_PACKAGES:
        raise BuildError(f"manifest.json has unknown package '{manifest['package']}'")

    plaintext = _pack_payload_source(str(payload_src_path))
    if not plaintext:
        raise BuildError("payload_src/ is empty — nothing to package")

    crypto = mykernel_native.PayloadCrypto()
    ciphertext = crypto.encrypt(plaintext)
    digest = mykernel_native.PayloadCrypto.sha256_hex(ciphertext)

    if output_path is None:
        output_path = str(project_path.parent / f"{manifest['name']}.justmko")

    with zipfile.ZipFile(output_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(MANIFEST_FILENAME, json.dumps(manifest, indent=2))
        zf.writestr(PAYLOAD_FILENAME, ciphertext)
        zf.writestr(PAYLOAD_HASH_FILENAME, digest)

    return output_path
