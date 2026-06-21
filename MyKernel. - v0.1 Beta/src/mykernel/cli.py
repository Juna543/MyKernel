"""
cli.py — Runtime CLI for MyKernel end users.

Per Documentation/05-cli-commands.md §5.2: runtime commands are typed
directly, with NO `mk` prefix (e.g. `run mycoolos.justmko`, not
`mk run mycoolos.justmko`). This is distinct from the developer-only
builder tool, mk_bmd (see tools/mk_bmd/), which is a separate Cython
tool with its own mk_bmd.CMD() API.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import manifest as manifest_mod
from . import crypto as crypto_mod
from . import packages as packages_mod
from .config_reader import load_config

try:
    from .scheduler import Scheduler, NativeModuleUnavailable as SchedulerNativeUnavailable
except ImportError:  # pragma: no cover
    Scheduler = None  # type: ignore
    SchedulerNativeUnavailable = Exception  # type: ignore


# In-process registry so `stop`/`list` can find instances started earlier
# in the SAME process. A real long-running kernel daemon would persist
# this; for v0.1 CLI usage each invocation is its own process, so this
# matters mainly for programmatic/test usage within one Python session.
_scheduler_singleton: "Scheduler | None" = None


def _get_scheduler() -> "Scheduler":
    global _scheduler_singleton
    if Scheduler is None:
        raise SchedulerNativeUnavailable(
            "mykernel_native is not built/importable. Run the native build first — see BUILD.md."
        )
    if _scheduler_singleton is None:
        config = load_config()
        _scheduler_singleton = Scheduler(config=config)
    return _scheduler_singleton


# --- Command implementations -------------------------------------------------

def cmd_create(args: argparse.Namespace) -> int:
    """
    `create <name>` — create a new .justmko skeleton.

    v0.1 scope: delegates to mk_bmd's template (see
    tools/mk_bmd/templates/manifest.json) rather than reimplementing
    project scaffolding in the runtime CLI. This keeps "create a new OS
    project" and "compile an OS project" both owned by the builder tool.
    """
    print(
        f"To create a new OS project named '{args.name}', use the mk_bmd builder tool "
        f"(see Documentation/06-builder-mk_bmd.md). The runtime CLI only runs/manages "
        f"already-built .justmko files."
    )
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    """`run <file.justmko>` — full flow per Documentation/02-architecture.md §2.4."""
    justmko_path = Path(args.file)

    try:
        manifest = manifest_mod.read_manifest(justmko_path)
    except manifest_mod.ManifestError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Loaded manifest: {manifest.name} v{manifest.version} (package: {manifest.package})")

    try:
        decrypted = crypto_mod.load_and_decrypt_payload(justmko_path)
    except crypto_mod.NativeModuleUnavailable as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except crypto_mod.PayloadValidationError as exc:
        print(f"error: invalid payload — {exc}", file=sys.stderr)
        return 1
    except Exception as exc:  # mykernel_native.IntegrityError / DecryptionError
        print(f"error: failed to verify/decrypt payload — {exc}", file=sys.stderr)
        return 1

    print(f"Payload verified and decrypted ({len(decrypted)} bytes).")

    try:
        packages_mod.load_package(manifest.package)
    except packages_mod.UnknownPackageError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    try:
        scheduler = _get_scheduler()
    except SchedulerNativeUnavailable as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    # NOTE: staging `decrypted` onto disk as an executable instance
    # filesystem is intentionally out of scope here — see
    # Documentation/08-roadmap.md "Open Questions". For now `run`
    # demonstrates the verified pipeline up through scheduling.
    instance_id = scheduler.start(
        name=manifest.name,
        package=manifest.package,
        justmko_path=justmko_path,
        executable="/bin/true",  # placeholder until payload->filesystem staging is defined
        args=[],
    )
    print(f"Started instance '{instance_id}' for '{manifest.name}'.")
    return 0


def cmd_stop(args: argparse.Namespace) -> int:
    """`stop <id>` — stop a running OS Instance."""
    try:
        scheduler = _get_scheduler()
        scheduler.stop(args.instance_id, force=args.force)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(f"Stopped instance '{args.instance_id}'.")
    return 0


def cmd_list(_args: argparse.Namespace) -> int:
    """`list` — list OS Instances tracked by this process."""
    try:
        scheduler = _get_scheduler()
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    instances = scheduler.list()
    if not instances:
        print("No instances running.")
        return 0

    for entry in instances:
        print(f"{entry['id']}  {entry['name']:<20} {entry['package']:<6} {entry['state']:<10} pid={entry['pid']}")
    return 0


def cmd_inspect(args: argparse.Namespace) -> int:
    """`inspect <file.justmko>` — show manifest without decrypting."""
    try:
        manifest = manifest_mod.read_manifest(args.file)
    except manifest_mod.ManifestError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"name:        {manifest.name}")
    print(f"version:     {manifest.version}")
    print(f"package:     {manifest.package}")
    print(f"description: {manifest.description}")
    print(f"source:      {manifest.source}")
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    """`verify <file.justmko>` — check SHA-256 without decrypting."""
    try:
        ok = crypto_mod.verify_only(args.file)
    except crypto_mod.NativeModuleUnavailable as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if ok:
        print(f"OK: '{args.file}' payload integrity verified.")
        return 0
    else:
        print(f"FAILED: '{args.file}' payload hash mismatch — file may be corrupted or tampered with.", file=sys.stderr)
        return 1


def cmd_delete(args: argparse.Namespace) -> int:
    """`delete <file.justmko>` — delete an OS Instance file from disk."""
    path = Path(args.file)
    if not path.exists():
        print(f"error: '{path}' not found", file=sys.stderr)
        return 1
    path.unlink()
    print(f"Deleted '{path}'.")
    return 0


# --- Argument parser -----------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="mykernel",
        description="MyKernel runtime CLI — no 'mk' prefix (see Documentation/05-cli-commands.md).",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_create = sub.add_parser("create", help="Create a new OS project skeleton (delegates to mk_bmd)")
    p_create.add_argument("name")
    p_create.set_defaults(func=cmd_create)

    p_run = sub.add_parser("run", help="Run an OS Instance from a .justmko file")
    p_run.add_argument("file")
    p_run.set_defaults(func=cmd_run)

    p_stop = sub.add_parser("stop", help="Stop a running OS Instance")
    p_stop.add_argument("instance_id")
    p_stop.add_argument("--force", action="store_true", help="Send SIGKILL instead of SIGTERM")
    p_stop.set_defaults(func=cmd_stop)

    p_list = sub.add_parser("list", help="List OS Instances")
    p_list.set_defaults(func=cmd_list)

    p_inspect = sub.add_parser("inspect", help="Show manifest.json without decrypting the payload")
    p_inspect.add_argument("file")
    p_inspect.set_defaults(func=cmd_inspect)

    p_verify = sub.add_parser("verify", help="Verify payload SHA-256 without decrypting")
    p_verify.add_argument("file")
    p_verify.set_defaults(func=cmd_verify)

    p_delete = sub.add_parser("delete", help="Delete a .justmko file")
    p_delete.add_argument("file")
    p_delete.set_defaults(func=cmd_delete)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
