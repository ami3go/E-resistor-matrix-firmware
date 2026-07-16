from __future__ import annotations

import json
import os
import platform
import socket
import sys
from dataclasses import asdict
from datetime import datetime, timezone
from importlib import metadata
from pathlib import Path
from typing import Any

from .models import RunConfig


def _distribution_version(name: str) -> str | None:
    try:
        return metadata.version(name)
    except metadata.PackageNotFoundError:
        return None


def _serial_inventory() -> dict[str, Any]:
    try:
        from serial.tools import list_ports  # type: ignore
    except Exception as exc:
        return {"available": False, "error": f"{type(exc).__name__}: {exc}", "ports": []}

    ports = []
    try:
        for item in list_ports.comports():
            ports.append(
                {
                    "device": str(item.device or ""),
                    "description": str(item.description or ""),
                    "manufacturer": str(item.manufacturer or ""),
                    "product": str(item.product or ""),
                    "serial_number": str(item.serial_number or ""),
                    "vid": item.vid,
                    "pid": item.pid,
                    "hwid": str(item.hwid or ""),
                }
            )
        return {"available": True, "error": "", "ports": ports}
    except Exception as exc:
        return {"available": False, "error": f"{type(exc).__name__}: {exc}", "ports": ports}


def _visa_inventory(backend: str) -> dict[str, Any]:
    try:
        import pyvisa  # type: ignore
    except Exception as exc:
        return {"available": False, "error": f"{type(exc).__name__}: {exc}", "resources": []}

    rm = None
    try:
        rm = pyvisa.ResourceManager(backend) if backend else pyvisa.ResourceManager()
        resources = [str(item) for item in rm.list_resources()]
        return {
            "available": True,
            "error": "",
            "backend": backend or "default",
            "resource_manager": str(getattr(rm, "visalib", "")),
            "resources": resources,
        }
    except Exception as exc:
        return {
            "available": False,
            "error": f"{type(exc).__name__}: {exc}",
            "backend": backend or "default",
            "resources": [],
        }
    finally:
        if rm is not None:
            try:
                rm.close()
            except Exception:
                pass


def collect_diagnostics(config: RunConfig, *, include_hardware_inventory: bool) -> dict[str, Any]:
    try:
        addresses = sorted({item[4][0] for item in socket.getaddrinfo(config.host, None)})
        dns_error = ""
    except Exception as exc:
        addresses = []
        dns_error = f"{type(exc).__name__}: {exc}"

    result: dict[str, Any] = {
        "schema_version": 1,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "python": {
            "version": sys.version,
            "executable": sys.executable,
            "prefix": sys.prefix,
            "base_prefix": sys.base_prefix,
            "in_virtual_environment": sys.prefix != sys.base_prefix,
        },
        "platform": {
            "platform": platform.platform(),
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
        },
        "process": {
            "cwd": os.getcwd(),
            "virtual_env": os.environ.get("VIRTUAL_ENV", ""),
        },
        "packages": {
            "e-resistor-regression": _distribution_version("e-resistor-regression"),
            "pyserial": _distribution_version("pyserial"),
            "PyVISA": _distribution_version("PyVISA"),
            "robotframework": _distribution_version("robotframework"),
            "pip": _distribution_version("pip"),
            "setuptools": _distribution_version("setuptools"),
        },
        "network": {
            "host": config.host,
            "resolved_addresses": addresses,
            "resolution_error": dns_error,
            "http_port": config.http_port,
            "scpi_port": config.scpi_port,
        },
        "config": asdict(config),
        "hardware_inventory_included": include_hardware_inventory,
    }
    if include_hardware_inventory:
        result["serial"] = _serial_inventory()
        result["visa"] = _visa_inventory(config.dmm_backend)
    return result


def write_diagnostics(output_dir: Path, diagnostics: dict[str, Any]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "diagnostics.json").write_text(
        json.dumps(diagnostics, indent=2, sort_keys=True), encoding="utf-8"
    )

    lines = [
        "# Regression Diagnostics",
        "",
        f"- Timestamp (UTC): `{diagnostics.get('timestamp_utc', '')}`",
        f"- Python: `{diagnostics.get('python', {}).get('version', '').splitlines()[0]}`",
        f"- Executable: `{diagnostics.get('python', {}).get('executable', '')}`",
        f"- Platform: `{diagnostics.get('platform', {}).get('platform', '')}`",
        f"- Target host: `{diagnostics.get('network', {}).get('host', '')}`",
        "",
        "## Installed packages",
        "",
    ]
    for name, version in diagnostics.get("packages", {}).items():
        lines.append(f"- `{name}`: `{version or 'not installed'}`")

    if diagnostics.get("hardware_inventory_included"):
        lines.extend(["", "## Serial ports", ""])
        serial = diagnostics.get("serial", {})
        if serial.get("ports"):
            for item in serial["ports"]:
                lines.append(
                    f"- `{item.get('device', '')}` — {item.get('description', '')} "
                    f"({item.get('manufacturer', '')})"
                )
        else:
            lines.append(f"- None reported. {serial.get('error', '')}".rstrip())

        lines.extend(["", "## VISA resources", ""])
        visa = diagnostics.get("visa", {})
        if visa.get("resources"):
            for resource in visa["resources"]:
                lines.append(f"- `{resource}`")
        else:
            lines.append(f"- None reported. {visa.get('error', '')}".rstrip())

    (output_dir / "diagnostics.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
