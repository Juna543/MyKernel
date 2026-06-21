"""
test_core.py — Unit tests for MyKernel's pure-Python core layer.

Run with: pytest tests/test_core.py -v
(from the project root, with src/ on PYTHONPATH — see BUILD.md or
pytest.ini / conftest.py for path setup.)

Tests that require mykernel_native (crypto, scheduler, full `run` flow)
are marked with @pytest.mark.requires_native and SKIPPED automatically
if the native module hasn't been built yet, so this file is runnable
in pure-Python environments too.
"""

from __future__ import annotations

import json
import zipfile
from pathlib import Path

import pytest

from mykernel import cli
from mykernel.config_reader import KernelConfig, load_config, save_config
from mykernel.manifest import Manifest, ManifestError, read_manifest, read_payload_bytes, read_payload_hash
from mykernel import crypto as crypto_mod
from mykernel import packages as packages_mod


# --- Fixtures -----------------------------------------------------------

@pytest.fixture
def fake_justmko(tmp_path) -> Path:
    """A syntactically valid .justmko with a fake (non-AES) payload, for
    tests that only exercise manifest reading / structure, not real
    crypto."""
    path = tmp_path / "hello-home-os.justmko"
    with zipfile.ZipFile(path, "w") as zf:
        zf.writestr(
            "manifest.json",
            json.dumps(
                {
                    "name": "hello-home-os",
                    "version": "0.1.0",
                    "package": "Home",
                    "description": "Minimal test instance",
                    "source": "test-fixture",
                }
            ),
        )
        zf.writestr("payload.bin", b"\x00" * 48)  # fake ciphertext-shaped bytes
        zf.writestr("payload.sha256", "0" * 64)
    return path


def _native_available() -> bool:
    return crypto_mod.native_module_available()


requires_native = pytest.mark.skipif(
    not _native_available(),
    reason="mykernel_native is not built — see BUILD.md",
)


# --- manifest.py ----------------------------------------------------------

class TestManifest:
    def test_read_valid_manifest(self, fake_justmko):
        m = read_manifest(fake_justmko)
        assert m.name == "hello-home-os"
        assert m.version == "0.1.0"
        assert m.package == "Home"
        assert m.description == "Minimal test instance"

    def test_read_missing_file_raises(self, tmp_path):
        missing = tmp_path / "does-not-exist.justmko"
        with pytest.raises(ManifestError):
            read_manifest(missing)

    def test_read_non_zip_raises(self, tmp_path):
        not_a_zip = tmp_path / "broken.justmko"
        not_a_zip.write_text("this is not a zip file")
        with pytest.raises(ManifestError):
            read_manifest(not_a_zip)

    def test_missing_required_field_raises(self, tmp_path):
        path = tmp_path / "bad.justmko"
        with zipfile.ZipFile(path, "w") as zf:
            zf.writestr("manifest.json", json.dumps({"name": "x"}))  # missing version/package
        with pytest.raises(ManifestError):
            read_manifest(path)

    def test_invalid_package_raises(self, tmp_path):
        path = tmp_path / "bad_package.justmko"
        with zipfile.ZipFile(path, "w") as zf:
            zf.writestr(
                "manifest.json",
                json.dumps({"name": "x", "version": "1.0", "package": "NotReal"}),
            )
        with pytest.raises(ManifestError):
            read_manifest(path)

    def test_read_payload_bytes_and_hash(self, fake_justmko):
        payload = read_payload_bytes(fake_justmko)
        assert payload == b"\x00" * 48

        digest = read_payload_hash(fake_justmko)
        assert digest == "0" * 64

    def test_manifest_to_dict_roundtrip(self):
        m = Manifest(name="x", version="1.0", package="Code", description="d", source="s")
        d = m.to_dict()
        assert d["name"] == "x"
        assert d["package"] == "Code"
        m2 = Manifest.from_dict(d)
        assert m2 == m or (m2.name, m2.version, m2.package) == (m.name, m.version, m.package)


# --- config_reader.py -------------------------------------------------------

class TestConfigReader:
    def test_load_missing_file_returns_defaults(self, tmp_path):
        cfg = load_config(tmp_path / "does-not-exist.json")
        assert cfg.kernel_version == "0.1.0-beta"
        assert cfg.owner == "JustIT"
        assert set(cfg.enabled_packages) == {"Work", "Code", "Home"}

    def test_save_and_load_roundtrip(self, tmp_path):
        path = tmp_path / "config.json"
        original = KernelConfig(kernel_version="0.1.0-beta", owner="JustIT")
        original.isolation.default_memory_limit = 256 * 1024 * 1024

        save_config(original, path)
        loaded = load_config(path)

        assert loaded.kernel_version == original.kernel_version
        assert loaded.owner == original.owner
        assert loaded.isolation.default_memory_limit == 256 * 1024 * 1024

    def test_load_invalid_json_raises(self, tmp_path):
        path = tmp_path / "config.json"
        path.write_text("{not valid json")
        from mykernel.config_reader import ConfigError

        with pytest.raises(ConfigError):
            load_config(path)

    def test_load_non_object_json_raises(self, tmp_path):
        path = tmp_path / "config.json"
        path.write_text("[1, 2, 3]")
        from mykernel.config_reader import ConfigError

        with pytest.raises(ConfigError):
            load_config(path)


# --- packages registry --------------------------------------------------

class TestPackagesRegistry:
    def test_available_packages(self):
        assert set(packages_mod.available_packages()) == {"Work", "Code", "Home"}

    def test_load_home_package(self):
        mod = packages_mod.load_package("Home")
        assert mod.PACKAGE_NAME == "Home"
        assert "status" in mod.COMMANDS

    def test_load_unknown_package_raises(self):
        with pytest.raises(packages_mod.UnknownPackageError):
            packages_mod.load_package("NotAPackage")


# --- cli.py: commands that don't require native ----------------------------

class TestCliInspect:
    def test_inspect_valid_file(self, fake_justmko, capsys):
        exit_code = cli.main(["inspect", str(fake_justmko)])
        captured = capsys.readouterr()

        assert exit_code == 0
        assert "hello-home-os" in captured.out
        assert "Home" in captured.out

    def test_inspect_missing_file(self, tmp_path, capsys):
        missing = tmp_path / "nope.justmko"
        exit_code = cli.main(["inspect", str(missing)])
        captured = capsys.readouterr()

        assert exit_code == 1
        assert "error" in captured.err.lower()

    def test_delete_command(self, fake_justmko, capsys):
        assert fake_justmko.exists()
        exit_code = cli.main(["delete", str(fake_justmko)])
        captured = capsys.readouterr()

        assert exit_code == 0
        assert not fake_justmko.exists()
        assert "Deleted" in captured.out


class TestCliWithoutNative:
    """
    These commands SHOULD fail gracefully (clear error, non-zero exit)
    when mykernel_native isn't built — they must never crash with a
    raw traceback.
    """

    @pytest.mark.skipif(_native_available(), reason="only meaningful when native is NOT built")
    def test_verify_without_native_fails_gracefully(self, fake_justmko, capsys):
        exit_code = cli.main(["verify", str(fake_justmko)])
        captured = capsys.readouterr()
        assert exit_code == 1
        assert "mykernel_native" in captured.err

    @pytest.mark.skipif(_native_available(), reason="only meaningful when native is NOT built")
    def test_run_without_native_fails_gracefully(self, fake_justmko, capsys):
        exit_code = cli.main(["run", str(fake_justmko)])
        captured = capsys.readouterr()
        assert exit_code == 1
        # manifest should still have been read and printed before the
        # native-dependent step failed
        assert "hello-home-os" in captured.out


# --- Tests that DO require the native module --------------------------------

@requires_native
class TestCryptoWithNative:
    def test_sha256_known_vector(self):
        import mykernel_native

        # SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
        digest = mykernel_native.PayloadCrypto.sha256_hex(b"")
        assert digest == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

    def test_encrypt_decrypt_roundtrip(self):
        import mykernel_native

        crypto = mykernel_native.PayloadCrypto()
        plaintext = b"hello mykernel, this is a test payload"

        ciphertext = crypto.encrypt(plaintext)
        assert ciphertext != plaintext

        decrypted = crypto.decrypt(ciphertext)
        assert decrypted == plaintext

    def test_verify_and_decrypt_detects_tampering(self):
        import mykernel_native

        crypto = mykernel_native.PayloadCrypto()
        ciphertext = crypto.encrypt(b"some payload data")
        real_hash = mykernel_native.PayloadCrypto.sha256_hex(ciphertext)

        # Correct hash: should succeed
        result = crypto.verify_and_decrypt(ciphertext, real_hash)
        assert result == b"some payload data"

        # Tampered hash: should raise IntegrityError, not silently decrypt
        with pytest.raises(mykernel_native.IntegrityError):
            crypto.verify_and_decrypt(ciphertext, "0" * 64)

    def test_load_and_decrypt_payload_end_to_end(self, tmp_path):
        """Full .justmko round-trip: encrypt -> package -> read -> verify -> decrypt."""
        import mykernel_native

        crypto = mykernel_native.PayloadCrypto()
        plaintext = b"this is the OS instance's binary config content"
        ciphertext = crypto.encrypt(plaintext)
        digest = mykernel_native.PayloadCrypto.sha256_hex(ciphertext)

        path = tmp_path / "real.justmko"
        with zipfile.ZipFile(path, "w") as zf:
            zf.writestr(
                "manifest.json",
                json.dumps({"name": "real-test-os", "version": "0.1.0", "package": "Home"}),
            )
            zf.writestr("payload.bin", ciphertext)
            zf.writestr("payload.sha256", digest)

        result = crypto_mod.load_and_decrypt_payload(path)
        assert result == plaintext


@requires_native
class TestSchedulerWithNative:
    def test_start_and_list_instance(self, tmp_path):
        from mykernel.scheduler import Scheduler

        scheduler = Scheduler(sandbox_root=tmp_path / "workspace")
        instance_id = scheduler.start(
            name="test-instance",
            package="Home",
            justmko_path=tmp_path / "fake.justmko",
            executable="/bin/true",
        )

        instances = scheduler.list()
        assert any(i["id"] == instance_id for i in instances)

        scheduler.stop(instance_id)
