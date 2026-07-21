from __future__ import annotations

import json
import math
import os
import platform
import sys
import time
import uuid
from dataclasses import asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

# Keep this library usable when Robot is started from any working directory.
PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from e_resistor_regression import __version__ as REGRESSION_PACKAGE_VERSION
from e_resistor_regression.coverage import (
    coverage_counts,
    evaluate_coverage,
    write_coverage_csv,
    write_coverage_json,
    write_coverage_markdown,
)
from e_resistor_regression.evidence import finalize_evidence
from e_resistor_regression.logging_ext import ExtendedLogger
from e_resistor_regression.models import RunConfig, TestResult
from e_resistor_regression.reports import (
    compare_baseline,
    flatten_metrics,
    write_junit,
    write_markdown_report,
    write_metrics_csv,
)
from e_resistor_regression.suite import RegressionSuite

try:
    from robot.api import logger as robot_logger
    from robot.api.deco import keyword, library
    from robot.libraries.BuiltIn import BuiltIn
except ImportError as exc:  # pragma: no cover - reported clearly during Robot import
    raise RuntimeError(
        "Robot Framework is required. Install requirements-robot.txt before running this library."
    ) from exc


class ContextualLogger(ExtendedLogger):
    """Extended logger that adds run/test/operation context to every record."""

    def __init__(self, output_dir: Path, verbose: bool = False) -> None:
        super().__init__(output_dir, verbose=verbose)
        self.context: dict[str, Any] = {}

    def set_context(self, **context: Any) -> None:
        self.context = {key: value for key, value in context.items() if value not in (None, "")}

    def clear_test_context(self) -> None:
        self.context = {
            key: value for key, value in self.context.items()
            if key in {"run_id", "gate", "profile"}
        }

    def event(self, event_type: str, **data: Any) -> None:
        merged = {**self.context, **data}
        super().event(event_type, **merged)

    def transcript(self, channel: str, direction: str, payload: str, **metadata: Any) -> None:
        merged = {**self.context, **metadata}
        super().transcript(channel, direction, payload, **merged)


@library(scope="SUITE", auto_keywords=False)
class EResistorRobotLibrary:
    """Robot Framework adapter for the E-Resistor Python regression harness.

    The adapter deliberately reuses the existing test implementations. Each Robot
    test invokes one named regression method, records the same detailed evidence,
    and then converts PASS/FAIL/SKIP into Robot Framework status.
    """

    ROBOT_LIBRARY_VERSION = "2.7.0"

    def __init__(self) -> None:
        self.initialized = False
        self.finalized = False
        self.run_id = ""
        self.operation_counter = 0
        self.output_dir: Path | None = None
        self.logger: ContextualLogger | None = None
        self.config: RunConfig | None = None
        self.suite: RegressionSuite | None = None
        self.results: list[TestResult] = []
        self.baseline_path: Path | None = None
        self.gate_manifest_path: Path | None = None

    # ------------------------------------------------------------------
    # Suite lifecycle
    # ------------------------------------------------------------------
    @keyword("Initialize E-Resistor Regression Suite")
    def initialize_regression_suite(
        self,
        output_dir: str,
        profile: str = "read_only",
        gate: str = "G3",
        host: str = "192.168.0.55",
        http_port: int = 80,
        scpi_port: int = 5025,
        timeout_s: float = 3.0,
        iterations: int = 30,
        stress_iterations: int = 100,
        heap_drift_limit_bytes: int = 2048,
        latency_regression_percent: float = 15.0,
        baseline: str = "",
        gate_manifest: str = "",
        allow_output_tests: bool = False,
        allow_active_output_tests: bool = False,
        allow_storage_tests: bool = False,
        allow_ota_tests: bool = False,
        allow_watchdog_tests: bool = False,
        serial_port: str = "auto",
        serial_baud: int = 115200,
        serial_match: str = "",
        dmm_resource: str = "auto",
        dmm_idn_contains: str = "34401",
        dmm_backend: str = "",
        dmm_init_commands: Any = None,
        dmm_measure_command: str = "READ?",
        hil_channel: int = 1,
        hil_bits: str = "0-15",
        hil_combination_masks: str = "0003,0005,0009",
        hil_repeat_cycles: int = 50,
        hil_error_limit_percent: float = 1.0,
        hil_settle_timeout_s: float = 20.0,
        hil_sample_count: int = 5,
        hil_sample_interval_s: float = 0.25,
        hil_stability_percent: float = 0.20,
        hil_minimum_wait_s: float = 0.5,
        hil_off_min_ohm: float = 50_000_000.0,
        hil_serial_fault_patterns: Any = None,
        fixture_confirmation: str = "",
        verbose: bool = False,
    ) -> str:
        if self.initialized:
            return self.run_id

        profile = str(profile).strip()
        gate = str(gate).strip().upper()
        if gate not in {f"G{i}" for i in range(10)}:
            raise AssertionError(f"Unsupported optimization gate: {gate!r}")
        if profile not in {
            "read_only", "safe_output", "hil_single_channel", "gate3_transport_fault",
            "gate4_profile", "gate4_profile_fault", "active_output", "storage", "ota", "watchdog",
        }:
            raise AssertionError(f"Unsupported regression profile: {profile!r}")

        if profile in {"safe_output", "gate4_profile"} and not self._to_bool(allow_output_tests):
            raise AssertionError(f"{profile} requires allow_output_tests=True")
        if profile in {"hil_single_channel", "gate3_transport_fault", "gate4_profile_fault"}:
            if not self._to_bool(allow_active_output_tests):
                raise AssertionError(f"{profile} requires allow_active_output_tests=True")
            if fixture_confirmation != "E_RESISTOR_SINGLE_CHANNEL_DMM":
                raise AssertionError(
                    f"{profile} requires fixture_confirmation="
                    "E_RESISTOR_SINGLE_CHANNEL_DMM"
                )

        root = Path(output_dir).expanduser().resolve()
        evidence_dir = root / "e_resistor_evidence" / profile
        evidence_dir.mkdir(parents=True, exist_ok=True)
        self.output_dir = evidence_dir
        self.run_id = self._make_run_id(gate, profile)
        self.logger = ContextualLogger(evidence_dir, verbose=self._to_bool(verbose))
        self.logger.set_context(run_id=self.run_id, gate=gate, profile=profile)

        init_commands = self._to_list(
            dmm_init_commands,
            default=["*CLS", "CONF:RES AUTO", "TRIG:SOUR IMM", "SAMP:COUN 1"],
        )
        fault_patterns = self._to_list(
            hil_serial_fault_patterns,
            default=["fatal", "panic", "assert", "hardfault", "queue overflow"],
        )

        self.config = RunConfig(
            host=str(host),
            http_port=int(http_port),
            scpi_port=int(scpi_port),
            timeout_s=max(0.1, float(timeout_s)),
            iterations=max(1, int(iterations)),
            stress_iterations=max(1, int(stress_iterations)),
            heap_drift_limit_bytes=max(0, int(heap_drift_limit_bytes)),
            latency_regression_percent=max(0.0, float(latency_regression_percent)),
            gate=gate,
            profile=profile,
            output_dir=str(evidence_dir),
            baseline=str(baseline).strip() or None,
            allow_output_tests=self._to_bool(allow_output_tests),
            allow_active_output_tests=self._to_bool(allow_active_output_tests),
            allow_storage_tests=self._to_bool(allow_storage_tests),
            allow_ota_tests=self._to_bool(allow_ota_tests),
            allow_watchdog_tests=self._to_bool(allow_watchdog_tests),
            serial_port=str(serial_port),
            serial_baud=max(1, int(serial_baud)),
            serial_match=str(serial_match),
            dmm_resource=str(dmm_resource),
            dmm_idn_contains=str(dmm_idn_contains),
            dmm_backend=str(dmm_backend),
            dmm_init_commands=init_commands,
            dmm_measure_command=str(dmm_measure_command),
            hil_channel=int(hil_channel),
            hil_bits=str(hil_bits),
            hil_combination_masks=str(hil_combination_masks),
            hil_repeat_cycles=(
                max(50, int(hil_repeat_cycles))
                if int(gate[1:]) >= 3 and profile in {"hil_single_channel", "gate3_transport_fault", "gate4_profile_fault"}
                else max(1, int(hil_repeat_cycles))
            ),
            hil_error_limit_percent=max(0.0, float(hil_error_limit_percent)),
            hil_settle_timeout_s=max(1.0, float(hil_settle_timeout_s)),
            hil_sample_count=max(2, int(hil_sample_count)),
            hil_sample_interval_s=max(0.0, float(hil_sample_interval_s)),
            hil_stability_percent=max(0.0, float(hil_stability_percent)),
            hil_minimum_wait_s=max(0.0, float(hil_minimum_wait_s)),
            hil_off_min_ohm=max(0.0, float(hil_off_min_ohm)),
            hil_serial_fault_patterns=fault_patterns,
            fixture_confirmation=str(fixture_confirmation),
        )
        self.baseline_path = Path(baseline).expanduser().resolve() if str(baseline).strip() else None
        self.gate_manifest_path = (
            Path(gate_manifest).expanduser().resolve() if str(gate_manifest).strip() else None
        )
        self.suite = RegressionSuite(self.config, self.logger)

        environment = {
            "schema_version": 2,
            "run_id": self.run_id,
            "timestamp_utc": datetime.now(timezone.utc).isoformat(),
            "python": sys.version,
            "platform": platform.platform(),
            "machine": platform.machine(),
            "cwd": os.getcwd(),
            "robot_framework": self._robot_version(),
            "regression_package_version": REGRESSION_PACKAGE_VERSION,
            "robot_library_version": self.ROBOT_LIBRARY_VERSION,
            "config": asdict(self.config),
        }
        (evidence_dir / "environment.json").write_text(
            json.dumps(environment, indent=2, sort_keys=True), encoding="utf-8"
        )
        (evidence_dir / "RUN_INCOMPLETE").write_text(
            f"run_id={self.run_id}\nstarted_utc={environment['timestamp_utc']}\n",
            encoding="utf-8",
        )
        self.logger.event("robot_suite_initialized", config=asdict(self.config))
        robot_logger.info(
            f"E-Resistor regression initialized: run_id={self.run_id}, "
            f"profile={profile}, gate={gate}, evidence={evidence_dir}"
        )
        self.initialized = True
        return self.run_id

    @keyword("Finalize E-Resistor Regression Suite")
    def finalize_regression_suite(self) -> str:
        if not self.initialized:
            return "NOT_INITIALIZED"
        if self.finalized:
            return "ALREADY_FINALIZED"
        assert self.output_dir is not None
        assert self.config is not None
        assert self.suite is not None
        assert self.logger is not None

        try:
            self._close_runtime_resources()
            # Enrich the version manifest after the device has been queried.
            environment_path = self.output_dir / "environment.json"
            environment = json.loads(environment_path.read_text(encoding="utf-8"))
            device_state = self.suite.state_before or self.suite.state_after
            if device_state:
                environment["device_identity"] = {
                    "firmware_name": device_state.get("firmware_name", ""),
                    "firmware_version": device_state.get("firmware_version", ""),
                    "firmware_serial": device_state.get("firmware_serial", ""),
                    "ip": device_state.get("ip", self.config.host),
                }
            hardware_path = self.output_dir / "hardware_manifest.json"
            if hardware_path.exists():
                environment["hil_hardware"] = json.loads(hardware_path.read_text(encoding="utf-8"))
            environment_path.write_text(
                json.dumps(environment, indent=2, sort_keys=True), encoding="utf-8"
            )

            all_results = list(self.results)
            counts = {
                status: sum(1 for item in all_results if item.status == status)
                for status in ("PASS", "FAIL", "SKIP")
            }
            summary: dict[str, Any] = {
                "schema_version": 2,
                "runner": "robot_framework",
                "run_id": self.run_id,
                "config": asdict(self.config),
                "counts": counts,
                "overall_status": "FAIL" if counts["FAIL"] else "PASS",
                "results": [item.to_dict() for item in all_results],
                "flat_metrics": flatten_metrics(all_results),
            }
            comparisons = compare_baseline(
                summary,
                self.baseline_path,
                self.config.latency_regression_percent,
            )
            summary["baseline_comparison"] = comparisons
            failed_comparisons = [item for item in comparisons if not item["passed"]]
            if failed_comparisons:
                summary["overall_status"] = "FAIL"
                summary["counts"]["FAIL"] += len(failed_comparisons)

            coverage_rows = evaluate_coverage(summary)
            summary["test_coverage"] = {
                "counts": coverage_counts(coverage_rows),
                "rows": coverage_rows,
            }

            (self.output_dir / "results.json").write_text(
                json.dumps(summary, indent=2, sort_keys=True), encoding="utf-8"
            )
            write_metrics_csv(self.output_dir / "metrics.csv", all_results)
            # Robot already creates output.xml; this file preserves the legacy JUnit format.
            write_junit(self.output_dir / "legacy_junit.xml", all_results)
            write_coverage_csv(self.output_dir / "test_coverage.csv", coverage_rows)
            write_coverage_json(self.output_dir / "test_coverage.json", coverage_rows)
            write_coverage_markdown(self.output_dir / "test_coverage.md", coverage_rows)
            write_markdown_report(self.output_dir / "report.md", summary)

            # Final event must be written before checksums. No evidence file may be
            # modified after finalize_evidence() completes.
            self.logger.event(
                "robot_suite_finalized",
                overall_status=summary["overall_status"],
                counts=summary["counts"],
            )
            self.logger.flush()
            verification = finalize_evidence(
                self.output_dir, self.run_id, summary["overall_status"]
            )
            self.finalized = True
            robot_logger.info(
                f"Regression evidence finalized and verified "
                f"({verification['checked_files']} files): {self.output_dir / 'report.md'}"
            )
            return summary["overall_status"]
        except Exception:
            self.logger.log.exception("Robot suite finalization failed")
            raise

    @keyword("Safety Cleanup After Test")
    def safety_cleanup_after_test(self) -> None:
        if not self.initialized or self.suite is None or self.config is None:
            return
        if self.config.profile in {"safe_output", "hil_single_channel", "gate3_transport_fault", "gate4_profile", "gate4_profile_fault", "active_output", "ota", "watchdog"}:
            self.suite.best_effort_all_off()

    @keyword("Emergency All Off")
    def emergency_all_off(self) -> None:
        if self.suite is None:
            raise AssertionError("Regression suite is not initialized")
        self.suite.best_effort_all_off()

    # ------------------------------------------------------------------
    # Runtime regression adapter
    # ------------------------------------------------------------------
    @keyword("Run E-Resistor Regression Check")
    def run_regression_check(self, test_id: str, name: str, method_name: str) -> dict[str, Any]:
        if not self.initialized or self.suite is None or self.logger is None:
            raise AssertionError("Initialize E-Resistor Regression Suite must run first")
        method = getattr(self.suite, str(method_name), None)
        if method is None or not callable(method):
            raise AssertionError(f"Unknown regression method: {method_name!r}")

        operation_id = self._next_operation_id(str(test_id))
        test_name = BuiltIn().get_variable_value("${TEST_NAME}", default=str(name))
        self.logger.set_context(
            run_id=self.run_id,
            gate=self.config.gate if self.config else "",
            profile=self.config.profile if self.config else "",
            test_id=str(test_id),
            robot_test_name=test_name,
            operation_id=operation_id,
        )
        self.logger.event("test_start", name=name, method=method_name)
        start = time.perf_counter_ns()
        try:
            status, message, metrics, details = method()
        except Exception as exc:
            status = "FAIL"
            message = f"{type(exc).__name__}: {exc}"
            metrics = {}
            details = {"exception_type": type(exc).__name__, "exception": repr(exc)}
            self.logger.log.exception("Robot regression check %s failed", test_id)
        duration_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        result = TestResult(str(test_id), str(name), status, duration_ms, message, metrics, details)
        self.results.append(result)
        # Keep suite.results synchronized because its helpers use the same evidence model.
        self.suite.results = self.results
        self.logger.event("test_result", **result.to_dict())

        robot_logger.info(
            f"{test_id} {status}: {message}\nMetrics: {json.dumps(metrics, sort_keys=True)}",
            html=False,
        )
        self.logger.clear_test_context()

        if status == "SKIP":
            BuiltIn().skip(message or "Regression check skipped")
        if status != "PASS":
            raise AssertionError(f"{test_id} {name}: {message}")
        return result.to_dict()

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------
    def _close_runtime_resources(self) -> None:
        assert self.suite is not None
        assert self.config is not None
        assert self.output_dir is not None
        assert self.logger is not None

        if self.config.profile in {"safe_output", "hil_single_channel", "gate3_transport_fault", "gate4_profile", "gate4_profile_fault", "active_output", "ota", "watchdog"}:
            self.suite.best_effort_all_off()
        try:
            state_text, self.suite.state_after, _ = self.suite._get_state()  # intentional shared implementation
            (self.output_dir / "state_after.txt").write_text(state_text, encoding="utf-8")
        except Exception as exc:
            self.logger.log.error("Unable to capture final state: %s", exc)

        if self.suite.hil_measurements:
            from e_resistor_regression.hil import write_hil_measurements

            write_hil_measurements(self.output_dir / "hil_measurements.csv", self.suite.hil_measurements)
            finite_errors = [
                abs(item.error_percent)
                for item in self.suite.hil_measurements
                if math.isfinite(item.error_percent)
            ]
            hil_summary = {
                "channel": self.config.hil_channel,
                "measurement_count": len(self.suite.hil_measurements),
                "pass_count": sum(item.result == "PASS" for item in self.suite.hil_measurements),
                "fail_count": sum(item.result != "PASS" for item in self.suite.hil_measurements),
                "max_abs_error_percent": max(finite_errors) if finite_errors else math.inf,
                "measurements": [item.to_dict() for item in self.suite.hil_measurements],
            }
            (self.output_dir / "hil_summary.json").write_text(
                json.dumps(hil_summary, indent=2, sort_keys=True), encoding="utf-8"
            )
        if self.suite.dmm is not None:
            try:
                self.suite.dmm.close()
            except Exception as exc:
                self.logger.log.error("DMM close failed: %s", exc)
            self.suite.dmm = None
        if self.suite.serial_monitor is not None:
            try:
                self.suite.serial_monitor.stop()
            except Exception as exc:
                self.logger.log.error("Serial monitor stop failed: %s", exc)
            self.suite.serial_monitor = None

    def _next_operation_id(self, test_id: str) -> str:
        self.operation_counter += 1
        safe_id = "".join(ch if ch.isalnum() else "-" for ch in test_id).strip("-")
        return f"{self.run_id}-{self.operation_counter:04d}-{safe_id}"

    @staticmethod
    def _make_run_id(gate: str, profile: str) -> str:
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        return f"{gate}-{profile}-{timestamp}-{uuid.uuid4().hex[:6]}"

    @staticmethod
    def _to_bool(value: Any) -> bool:
        if isinstance(value, bool):
            return value
        if value is None:
            return False
        return str(value).strip().lower() in {"1", "true", "yes", "on", "enabled"}

    @staticmethod
    def _to_list(value: Any, default: list[str]) -> list[str]:
        if value is None or value == "":
            return list(default)
        if isinstance(value, (list, tuple)):
            return [str(item) for item in value]
        text = str(value).strip()
        if not text:
            return list(default)
        if text.startswith("["):
            parsed = json.loads(text)
            if not isinstance(parsed, list):
                raise ValueError("Expected a JSON list")
            return [str(item) for item in parsed]
        return [item.strip() for item in text.split("|") if item.strip()]

    @staticmethod
    def _robot_version() -> str:
        try:
            import robot

            return str(robot.__version__)
        except Exception:
            return "unknown"
