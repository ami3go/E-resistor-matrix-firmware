from __future__ import annotations

from dataclasses import dataclass, field, asdict
from typing import Any


@dataclass
class TestResult:
    test_id: str
    name: str
    status: str
    duration_ms: float
    message: str = ""
    metrics: dict[str, float | int | str] = field(default_factory=dict)
    details: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class RunConfig:
    host: str
    http_port: int = 80
    scpi_port: int = 5025
    timeout_s: float = 3.0
    iterations: int = 30
    stress_iterations: int = 100
    heap_drift_limit_bytes: int = 2048
    latency_regression_percent: float = 15.0
    gate: str = "G0"
    profile: str = "read_only"
    output_dir: str = "results"
    source_dir: str | None = None
    baseline: str | None = None
    allow_output_tests: bool = False
    allow_active_output_tests: bool = False
    allow_storage_tests: bool = False
    allow_ota_tests: bool = False
    allow_watchdog_tests: bool = False
    skip_device: bool = False
    serial_port: str = "auto"
    serial_baud: int = 115200
    serial_match: str = ""
    dmm_resource: str = "auto"
    dmm_idn_contains: str = "34401"
    dmm_backend: str = ""
    dmm_init_commands: list[str] = field(default_factory=lambda: ["*CLS", "CONF:RES AUTO", "TRIG:SOUR IMM", "SAMP:COUN 1"])
    dmm_measure_command: str = "READ?"
    hil_channel: int = 1
    hil_bits: str = "0-15"
    hil_combination_masks: str = "0003,0005,0009"
    hil_repeat_cycles: int = 10
    hil_error_limit_percent: float = 1.0
    hil_settle_timeout_s: float = 20.0
    hil_sample_count: int = 5
    hil_sample_interval_s: float = 0.25
    hil_stability_percent: float = 0.20
    hil_minimum_wait_s: float = 0.5
    hil_off_min_ohm: float = 50_000_000.0
    hil_serial_fault_patterns: list[str] = field(default_factory=lambda: ["fatal", "panic", "assert", "hardfault", "queue overflow"])
    fixture_confirmation: str = ""
    required_pages: list[str] = field(default_factory=lambda: [
        "/", "/settings", "/safety", "/files", "/firmware",
        "/scpi", "/log", "/runtime", "/watchdog"
    ])
