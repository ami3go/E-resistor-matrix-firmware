from __future__ import annotations

import json
import platform
import sys
from datetime import datetime, timezone
from importlib import metadata
from pathlib import Path
from typing import Any

from . import __version__
from .clients import HttpClient, ScpiClient
from .models import RunConfig
from .parsers import parse_http_state


def _distribution_version(name: str) -> str | None:
    try:
        return metadata.version(name)
    except metadata.PackageNotFoundError:
        return None


def collect_runner_versions(robot_library_version: str) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "captured_utc": datetime.now(timezone.utc).isoformat(),
        "regression_package": {
            "distribution": "e-resistor-regression",
            "version": __version__,
            "robot_library_version": robot_library_version,
        },
        "python": {
            "version": platform.python_version(),
            "full_version": sys.version,
            "executable": sys.executable,
        },
        "packages": {
            "robotframework": _distribution_version("robotframework"),
            "pyserial": _distribution_version("pyserial"),
            "PyVISA": _distribution_version("PyVISA"),
            "pip": _distribution_version("pip"),
            "setuptools": _distribution_version("setuptools"),
        },
        "dut": {
            "capture_status": "NOT_ATTEMPTED",
            "firmware_name": "",
            "firmware_version": "",
            "firmware_serial": "",
            "scpi_identity": "",
            "scpi_version": "",
            "http_error": "",
            "scpi_error": "",
        },
    }


def capture_dut_versions(config: RunConfig, logger: Any) -> dict[str, Any]:
    dut: dict[str, Any] = {
        "capture_status": "FAILED",
        "firmware_name": "",
        "firmware_version": "",
        "firmware_serial": "",
        "scpi_identity": "",
        "scpi_version": "",
        "http_error": "",
        "scpi_error": "",
    }
    try:
        response = HttpClient(config.host, config.http_port, config.timeout_s, logger).request("/state")
        if response.status == 200:
            state = parse_http_state(response.text)
            dut["firmware_name"] = str(state.get("firmware_name", ""))
            dut["firmware_version"] = str(state.get("firmware_version", ""))
            dut["firmware_serial"] = str(state.get("firmware_serial", ""))
        else:
            dut["http_error"] = f"HTTP {response.status}"
    except Exception as exc:
        dut["http_error"] = f"{type(exc).__name__}: {exc}"

    try:
        with ScpiClient(config.host, config.scpi_port, config.timeout_s, logger) as client:
            identity, _ = client.query("*IDN?")
            version, _ = client.query("SYST:VERS?")
        dut["scpi_identity"] = identity
        dut["scpi_version"] = version
    except Exception as exc:
        dut["scpi_error"] = f"{type(exc).__name__}: {exc}"

    if dut["firmware_version"] or dut["scpi_version"] or dut["scpi_identity"]:
        dut["capture_status"] = "CAPTURED"
    elif dut["http_error"] or dut["scpi_error"]:
        dut["capture_status"] = "FAILED"
    return dut


def write_software_versions(output_dir: Path, versions: dict[str, Any]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "software_versions.json").write_text(
        json.dumps(versions, indent=2, sort_keys=True), encoding="utf-8"
    )
    regression = versions.get("regression_package", {})
    python = versions.get("python", {})
    packages = versions.get("packages", {})
    dut = versions.get("dut", {})
    lines = [
        "# Software Versions",
        "",
        f"- Captured UTC: `{versions.get('captured_utc', '')}`",
        f"- Regression package: `{regression.get('version', 'unknown')}`",
        f"- Robot library: `{regression.get('robot_library_version', 'unknown')}`",
        f"- Robot Framework: `{packages.get('robotframework') or 'not installed'}`",
        f"- Python: `{python.get('version', 'unknown')}`",
        f"- PySerial: `{packages.get('pyserial') or 'not installed'}`",
        f"- PyVISA: `{packages.get('PyVISA') or 'not installed'}`",
        "",
        "## E-Resistor DUT",
        "",
        f"- Capture status: **{dut.get('capture_status', 'UNKNOWN')}**",
        f"- Firmware name: `{dut.get('firmware_name') or 'not reported'}`",
        f"- Firmware version: `{dut.get('firmware_version') or 'not reported'}`",
        f"- Firmware serial: `{dut.get('firmware_serial') or 'not reported'}`",
        f"- SCPI identity: `{dut.get('scpi_identity') or 'not reported'}`",
        f"- SCPI version: `{dut.get('scpi_version') or 'not reported'}`",
    ]
    if dut.get("http_error"):
        lines.append(f"- HTTP capture error: `{dut['http_error']}`")
    if dut.get("scpi_error"):
        lines.append(f"- SCPI capture error: `{dut['scpi_error']}`")
    (output_dir / "software_versions.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
