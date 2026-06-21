"""
scheduler.py — High-level resource & process orchestration.

This module owns the LOGIC of deciding what should run and with what
resource limits. The actual isolation/process-spawning mechanics are
delegated to mykernel_native.IsolationManager (see
Documentation/02-architecture.md §2.2 — Python does not implement
isolation directly).
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .config_reader import KernelConfig

try:
    import mykernel_native  # type: ignore
    _NATIVE_AVAILABLE = True
except ImportError:  # pragma: no cover
    _NATIVE_AVAILABLE = False


class SchedulerError(Exception):
    """Raised for scheduler-level failures not already covered by native exceptions."""


class NativeModuleUnavailable(Exception):
    """Raised when an operation needs mykernel_native but it hasn't been built."""


@dataclass
class RunningInstance:
    instance_id: str
    name: str
    package: str
    justmko_path: Path


class Scheduler:
    """
    Tracks OS Instances managed by this MyKernel process and forwards
    isolation work to the native IsolationManager.
    """

    def __init__(self, config: KernelConfig | None = None, sandbox_root: str | Path = "sandbox/workspace"):
        if not _NATIVE_AVAILABLE:
            raise NativeModuleUnavailable(
                "mykernel_native is not built/importable. Run the native build first — see BUILD.md."
            )

        self.config = config or KernelConfig()
        self.sandbox_root = Path(sandbox_root)
        self._isolation = mykernel_native.IsolationManager()  # type: ignore[attr-defined]
        self._instances: dict[str, RunningInstance] = {}

    def _build_limits(self) -> "mykernel_native.ResourceLimits":  # type: ignore[name-defined]
        limits = mykernel_native.ResourceLimits()  # type: ignore[attr-defined]
        limits.max_memory_bytes = self.config.isolation.default_memory_limit
        limits.max_cpu_seconds = self.config.isolation.default_cpu_limit
        limits.max_open_files = None
        return limits

    def start(self, name: str, package: str, justmko_path: str | Path,
               executable: str, args: list[str] | None = None) -> str:
        """
        Registers and starts an OS Instance.

        `executable`/`args` describe what process should actually run
        inside the isolated instance once the payload has been decrypted
        and laid out on disk (that staging step lives in the CLI layer —
        see cli.py's `run` command).
        """
        instance_root = self.sandbox_root / name
        instance_id = self._isolation.create_instance(name, str(instance_root))

        try:
            self._isolation.run_instance(instance_id, executable, args or [], self._build_limits())
        except Exception:
            # Don't leave a half-registered instance behind on failure.
            self._isolation.remove_instance(instance_id)
            raise

        self._instances[instance_id] = RunningInstance(
            instance_id=instance_id,
            name=name,
            package=package,
            justmko_path=Path(justmko_path),
        )
        return instance_id

    def stop(self, instance_id: str, force: bool = False) -> None:
        if instance_id not in self._instances:
            raise SchedulerError(f"unknown instance id '{instance_id}'")
        self._isolation.stop_instance(instance_id, force)

    def list(self) -> list[dict]:
        """Returns a list of dicts merging scheduler + native state, for the `list` CLI command."""
        result = []
        for instance_id, tracked in self._instances.items():
            info = self._isolation.get_instance_info(instance_id)
            result.append(
                {
                    "id": instance_id,
                    "name": tracked.name,
                    "package": tracked.package,
                    "state": info.state.name,
                    "pid": info.pid,
                    "justmko_path": str(tracked.justmko_path),
                }
            )
        return result

    def remove(self, instance_id: str) -> None:
        if instance_id not in self._instances:
            raise SchedulerError(f"unknown instance id '{instance_id}'")
        self._isolation.remove_instance(instance_id)
        del self._instances[instance_id]
