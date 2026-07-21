"""Environment-backed Robot Framework variables for the E-Resistor bench.

All settings can be overridden with Robot ``--variable NAME:value`` options or
with environment variables prefixed by ``ERESISTOR_``.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]


def _env(name: str, default: str = "") -> str:
    return os.environ.get(f"ERESISTOR_{name}", default)


def _bool(name: str, default: bool = False) -> bool:
    value = _env(name, "true" if default else "false")
    return value.strip().lower() in {"1", "true", "yes", "on", "enabled"}


def _int(name: str, default: int) -> int:
    return int(_env(name, str(default)))


def _float(name: str, default: float) -> float:
    return float(_env(name, str(default)))


def _list(name: str, default: list[str]) -> list[str]:
    text = _env(name, "")
    if not text:
        return list(default)
    if text.lstrip().startswith("["):
        value = json.loads(text)
        if not isinstance(value, list):
            raise ValueError(f"ERESISTOR_{name} must be a JSON list")
        return [str(item) for item in value]
    return [part.strip() for part in text.split("|") if part.strip()]


def get_variables() -> dict[str, Any]:
    return {
        "PROJECT_ROOT": str(ROOT),
        "GATE": _env("GATE", "G4"),
        "ERESISTOR_HOST": _env("HOST", "192.168.0.55"),
        "HTTP_PORT": _int("HTTP_PORT", 80),
        "SCPI_PORT": _int("SCPI_PORT", 5025),
        "TIMEOUT_S": _float("TIMEOUT_S", 3.0),
        "ITERATIONS": _int("ITERATIONS", 30),
        "STRESS_ITERATIONS": _int("STRESS_ITERATIONS", 100),
        "HEAP_DRIFT_LIMIT_BYTES": _int("HEAP_DRIFT_LIMIT_BYTES", 2048),
        "LATENCY_REGRESSION_PERCENT": _float("LATENCY_REGRESSION_PERCENT", 15.0),
        "BASELINE": _env("BASELINE", ""),
        "GATE_MANIFEST": _env("GATE_MANIFEST", ""),
        "ALLOW_OUTPUT_TESTS": _bool("ALLOW_OUTPUT_TESTS", False),
        "ALLOW_ACTIVE_OUTPUT_TESTS": _bool("ALLOW_ACTIVE_OUTPUT_TESTS", False),
        "ALLOW_STORAGE_TESTS": _bool("ALLOW_STORAGE_TESTS", False),
        "ALLOW_OTA_TESTS": _bool("ALLOW_OTA_TESTS", False),
        "ALLOW_WATCHDOG_TESTS": _bool("ALLOW_WATCHDOG_TESTS", False),
        "SERIAL_PORT": _env("SERIAL_PORT", "auto"),
        "SERIAL_BAUD": _int("SERIAL_BAUD", 115200),
        "SERIAL_MATCH": _env("SERIAL_MATCH", ""),
        "DMM_RESOURCE": _env("DMM_RESOURCE", "auto"),
        "DMM_IDN_CONTAINS": _env("DMM_IDN_CONTAINS", "34401"),
        "DMM_BACKEND": _env("DMM_BACKEND", ""),
        "DMM_INIT_COMMANDS": _list(
            "DMM_INIT_COMMANDS",
            ["*CLS", "CONF:RES AUTO", "TRIG:SOUR IMM", "SAMP:COUN 1"],
        ),
        "DMM_MEASURE_COMMAND": _env("DMM_MEASURE_COMMAND", "READ?"),
        "HIL_CHANNEL": _int("HIL_CHANNEL", 1),
        "HIL_BITS": _env("HIL_BITS", "0-15"),
        "HIL_COMBINATION_MASKS": _env("HIL_COMBINATION_MASKS", "0003,0005,0009"),
        "HIL_REPEAT_CYCLES": _int("HIL_REPEAT_CYCLES", 50),
        "HIL_ERROR_LIMIT_PERCENT": _float("HIL_ERROR_LIMIT_PERCENT", 1.0),
        "HIL_SETTLE_TIMEOUT_S": _float("HIL_SETTLE_TIMEOUT_S", 20.0),
        "HIL_SAMPLE_COUNT": _int("HIL_SAMPLE_COUNT", 5),
        "HIL_SAMPLE_INTERVAL_S": _float("HIL_SAMPLE_INTERVAL_S", 0.25),
        "HIL_STABILITY_PERCENT": _float("HIL_STABILITY_PERCENT", 0.20),
        "HIL_MINIMUM_WAIT_S": _float("HIL_MINIMUM_WAIT_S", 0.5),
        "HIL_OFF_MIN_OHM": _float("HIL_OFF_MIN_OHM", 50_000_000.0),
        "HIL_SERIAL_FAULT_PATTERNS": _list(
            "HIL_SERIAL_FAULT_PATTERNS",
            ["fatal", "panic", "assert", "hardfault", "queue overflow"],
        ),
        "FIXTURE_CONFIRMATION": _env("FIXTURE_CONFIRMATION", ""),
        "VERBOSE": _bool("VERBOSE", False),
    }
