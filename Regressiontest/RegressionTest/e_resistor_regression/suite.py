from __future__ import annotations

import concurrent.futures
import json
import math
import re
import socket
import statistics
import time
from pathlib import Path
from typing import Callable

from .clients import HttpClient, ScpiClient
from .hil import (
    HilMeasurement,
    SerialMonitor,
    VisaDmm,
    equivalent_resistance_ohm,
    error_percent,
    parse_bits_spec,
    parse_masks_spec,
    wait_for_stable_resistance,
    write_hil_measurements,
)
from .logging_ext import ExtendedLogger
from .models import RunConfig, TestResult
from .parsers import (
    latency_metrics,
    parse_calibration_compact,
    parse_http_state,
    parse_key_value_response,
    parse_scpi_state,
    percentile,
    response_excerpt,
)


class RegressionSuite:
    def __init__(self, config: RunConfig, logger: ExtendedLogger) -> None:
        self.config = config
        self.logger = logger
        self.http = HttpClient(config.host, config.http_port, config.timeout_s, logger)
        self.results: list[TestResult] = []
        self.state_before: dict = {}
        self.state_after: dict = {}
        self.serial_monitor: SerialMonitor | None = None
        self.dmm: VisaDmm | None = None
        self.hil_ready = False
        self.hil_start_timestamp = 0.0
        self.hil_calibration: dict[int, float] = {}
        self.calibration_tables: dict[int, dict[int, float]] = {}
        self.hil_measurements: list[HilMeasurement] = []
        self.gate_compatible: bool = False
        self.detected_firmware_version: str = ""

    def add(self, test_id: str, name: str, function: Callable[[], tuple[str, str, dict, dict]]) -> None:
        start = time.perf_counter_ns()
        try:
            status, message, metrics, details = function()
        except Exception as exc:  # intentionally captures a test failure without aborting the suite
            status = "FAIL"
            message = f"{type(exc).__name__}: {exc}"
            metrics = {}
            details = {"exception_type": type(exc).__name__}
            self.logger.log.exception("Test %s failed with exception", test_id)
        duration_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        result = TestResult(test_id, name, status, duration_ms, message, metrics, details)
        self.results.append(result)
        self.logger.event("test_result", **result.to_dict())
        level = self.logger.log.info if status == "PASS" else self.logger.log.warning
        level("%s %-4s %s — %s", test_id, status, name, message)

    def _get_state(self) -> tuple[str, dict, float]:
        response = self.http.request("/state")
        if response.status != 200:
            raise AssertionError(f"/state returned HTTP {response.status}")
        parsed = parse_http_state(response.text)
        return response.text, parsed, response.elapsed_ms

    def _all_masks_zero(self, state: dict) -> bool:
        channels = state.get("channels", {})
        return len(channels) == 8 and all(item.get("mask") == "0000" for item in channels.values())

    @staticmethod
    def _expected_gate_for_firmware(version: str) -> str | None:
        return {
            "0.4.4": "G0",
            "0.4.6": "G1",
            "0.5.0": "G2",
            "0.6.0": "G3",
            "0.6.1": "G3",
            "0.7.0": "G4",
        }.get(version.strip())

    def _query_scpi_state(
        self, client: ScpiClient, retries: int = 3
    ) -> tuple[str, dict[int, dict[str, str]], float, int]:
        """Query and validate all eight SCPI channel states with bounded retry.

        Some firmware/network combinations can return a partial text response when
        the socket idle boundary occurs between TCP segments. A retry is safe because
        ``STATE?`` is read-only. Every failed parse is retained in the transcript.
        """
        attempts: list[dict[str, object]] = []
        total_elapsed = 0.0
        last_response = ""
        for attempt in range(1, max(1, retries) + 1):
            last_response, elapsed = client.query("STATE?")
            total_elapsed += elapsed
            parsed = parse_scpi_state(last_response)
            if len(parsed) == 8:
                return last_response, parsed, total_elapsed, attempt
            diagnostic = {
                "attempt": attempt,
                "parsed_channels": len(parsed),
                "raw_excerpt": response_excerpt(last_response),
            }
            attempts.append(diagnostic)
            self.logger.event("scpi_state_parse_retry", **diagnostic)
            if attempt < retries:
                client.close()
                time.sleep(0.05)
                client.connect()
        raise AssertionError(
            "Incomplete SCPI STATE? response after "
            f"{len(attempts)} attempt(s): {attempts}; "
            f"last_raw={response_excerpt(last_response)!r}"
        )

    def run(self, output_dir: Path) -> list[TestResult]:
        self.add("NET-001", "TCP reachability", self.test_tcp_reachability)
        self.add("HTTP-001", "HTTP ping", self.test_http_ping)
        self.add("HTTP-002", "State schema and readiness", self.test_state_schema)
        self.add("HTTP-003", "Required GUI pages", self.test_pages)
        self.add("SCPI-001", "SCPI identity and greeting", self.test_scpi_identity)
        self.add("SCPI-002", "SCPI status and state consistency", self.test_scpi_state_consistency)
        self.add("SCPI-003", "Calibration table integrity", self.test_calibration)
        self.add("SCPI-004", "Undefined-header error queue", self.test_scpi_error_queue)
        self.add("SCPI-005", "Oversized-line recovery", self.test_scpi_overflow_recovery)
        self.add("SCPI-006", "Read-only target-mask calculation", self.test_scpi_target_calculation)
        self.add("PERF-001", "HTTP latency sample", self.test_http_latency)
        self.add("PERF-002", "SCPI latency sample", self.test_scpi_latency)
        self.add("MEM-001", "Repeated-state heap stability", self.test_heap_stability)
        self.add("STRESS-001", "Concurrent HTTP and SCPI polling", self.test_concurrent_polling)
        self.add("FILES-001", "Calibration bundle download", self.test_calibration_bundle_download)
        self.add("LOG-001", "Log text export", self.test_log_download)

        try:
            if self.config.profile == "safe_output":
                self.add("SAFE-001", "Zero-mask command path", self.test_zero_mask_command_path)
                self.add("SAFE-002", "Repeated all-off command path", self.test_repeated_all_off)
            elif self.config.profile == "hil_single_channel":
                if not self.config.allow_active_output_tests:
                    self.results.append(TestResult(
                        "HIL-000", "Single-channel HIL fixture", "SKIP", 0.0,
                        "requires --allow-active-output-tests",
                    ))
                elif self.config.fixture_confirmation != "E_RESISTOR_SINGLE_CHANNEL_DMM":
                    self.results.append(TestResult(
                        "HIL-000", "Single-channel HIL fixture", "SKIP", 0.0,
                        "requires --fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM",
                    ))
                else:
                    self.add("HIL-001", "COM port and DMM discovery", self.test_hil_fixture_discovery)
                    self.add("HIL-002", "Fixture safe-state precheck", self.test_hil_safe_state_precheck)
                    self.add("HIL-003", "Single-bit physical resistance walk", self.test_hil_single_bit_walk)
                    self.add("HIL-004", "Combination-mask physical resistance", self.test_hil_combination_masks)
                    self.add("HIL-005", "Repeated physical switching", self.test_hil_repeated_switching)
                    self.add("HIL-006", "Serial fault-log inspection", self.test_hil_serial_health)
                    self.add("HIL-008", "Deterministic USB serial diagnostic", self.test_hil_serial_diagnostic)
                    self.add("HIL-007", "Final all-off isolation measurement", self.test_hil_final_all_off)
            elif self.config.profile not in {"read_only", "safe_output"}:
                self.results.append(TestResult(
                    "PROFILE-001", "Requested profile", "SKIP", 0.0,
                    f"Profile {self.config.profile!r} is reserved for a dedicated fixture extension.",
                ))
        finally:
            if (self.config.profile == "safe_output" and self.config.allow_output_tests) or (
                self.config.profile == "hil_single_channel" and self.config.allow_active_output_tests
            ):
                self.best_effort_all_off()

            try:
                state_text, self.state_after, _ = self._get_state()
                (output_dir / "state_after.txt").write_text(state_text, encoding="utf-8")
            except Exception as exc:
                self.logger.log.error("Unable to capture final state: %s", exc)

            if self.hil_measurements:
                write_hil_measurements(output_dir / "hil_measurements.csv", self.hil_measurements)
                hil_summary = {
                    "channel": self.config.hil_channel,
                    "measurement_count": len(self.hil_measurements),
                    "pass_count": sum(item.result == "PASS" for item in self.hil_measurements),
                    "fail_count": sum(item.result != "PASS" for item in self.hil_measurements),
                    "max_abs_error_percent": max(
                        (abs(item.error_percent) for item in self.hil_measurements if math.isfinite(item.error_percent)),
                        default=math.inf,
                    ),
                    "measurements": [item.to_dict() for item in self.hil_measurements],
                }
                (output_dir / "hil_summary.json").write_text(
                    json.dumps(hil_summary, indent=2, sort_keys=True), encoding="utf-8"
                )
            if self.dmm is not None:
                try:
                    self.dmm.close()
                except Exception as exc:
                    self.logger.log.error("DMM close failed: %s", exc)
            if self.serial_monitor is not None:
                try:
                    self.serial_monitor.stop()
                except Exception as exc:
                    self.logger.log.error("Serial monitor stop failed: %s", exc)
        return self.results

    def test_tcp_reachability(self):
        checks = []
        for name, port in (("HTTP", self.config.http_port), ("SCPI", self.config.scpi_port)):
            start = time.perf_counter_ns()
            with socket.create_connection((self.config.host, port), timeout=self.config.timeout_s):
                pass
            elapsed = (time.perf_counter_ns() - start) / 1_000_000.0
            checks.append((name, elapsed))
        return "PASS", "HTTP and SCPI ports reachable", {f"{name.lower()}_connect_ms": value for name, value in checks}, {}

    def test_http_ping(self):
        response = self.http.request("/ping")
        passed = response.status == 200 and response.text.strip() == "pong"
        return ("PASS" if passed else "FAIL", f"HTTP {response.status}, body={response.text.strip()!r}",
                {"http_ping_single_ms": response.elapsed_ms}, {})

    def test_state_schema(self):
        text, state, elapsed = self._get_state()
        self.state_before = state
        Path(self.config.output_dir, "state_before.txt").write_text(text, encoding="utf-8")
        required = [
            "firmware_name", "firmware_version", "firmware_serial", "ip", "littlefs",
            "shift_registers_ready", "outputs_known_safe", "fatal_safe_state", "heap_free_bytes",
            "core1_state", "core1_outputs_ready", "core1_fault", "core1_loop_count",
            "core1_queue_overflow_count",
        ]
        if self.config.gate != "G0":
            required.extend([
                "core1_loop_max_us", "core1_stack_min_free_bytes",
                "core1_event_count", "core1_event_drop_count",
            ])
        if int(self.config.gate[1:]) >= 2:
            required.extend([
                "target_search_last_candidates", "target_search_last_elapsed_us",
                "target_search_timeout_count", "target_search_cancel_count",
            ])
        if int(self.config.gate[1:]) >= 3:
            required.extend([
                "core_snapshot_sequence", "core_snapshot_last_command_sequence",
                "core_snapshot_generation", "core_snapshot_flags",
                "core_transport_generation", "core_transport_last_submitted_sequence",
                "core_transport_last_completed_sequence", "core_transport_command_timeouts",
                "core_transport_expired_rejects", "core_transport_generation_rejects",
                "core_transport_command_overflows", "core_transport_result_overflows",
                "core_transport_policy_installs", "core_transport_core0_failsafe_count",
            ])
        missing = [key for key in required if key not in state]
        channel_count = len(state.get("channels", {}))
        readiness_failures = []
        if state.get("littlefs") != "ready": readiness_failures.append("LittleFS not ready")
        if state.get("shift_registers_ready") != "1": readiness_failures.append("shift registers not ready")
        if state.get("fatal_safe_state") != "0": readiness_failures.append("fatal safe state active")
        if state.get("core1_state") != "ready": readiness_failures.append("Core 1 not ready")

        self.detected_firmware_version = str(state.get("firmware_version", "")).strip()
        expected_gate = self._expected_gate_for_firmware(self.detected_firmware_version)
        gate_mismatch = bool(expected_gate and expected_gate != self.config.gate)
        self.gate_compatible = not gate_mismatch
        if gate_mismatch:
            readiness_failures.append(
                f"firmware {self.detected_firmware_version} belongs to {expected_gate}, "
                f"but run selected {self.config.gate}"
            )

        passed = not missing and channel_count == 8 and not readiness_failures
        details = {
            "missing": missing,
            "channel_count": channel_count,
            "readiness_failures": readiness_failures,
            "firmware_version": self.detected_firmware_version,
            "expected_gate": expected_gate,
            "selected_gate": self.config.gate,
            "raw_excerpt": response_excerpt(text),
        }
        message = "state valid" if passed else json.dumps(details, sort_keys=True)
        return "PASS" if passed else "FAIL", message, {"state_request_ms": elapsed}, details

    def test_pages(self):
        failed: list[str] = []
        latencies: list[float] = []
        sizes: dict[str, int] = {}
        for page in self.config.required_pages:
            response = self.http.request(page)
            latencies.append(response.elapsed_ms)
            sizes[page] = len(response.body)
            if response.status != 200 or len(response.body) < 20:
                failed.append(f"{page}: HTTP {response.status}, {len(response.body)} bytes")
        metrics = latency_metrics(latencies, "gui_page")
        metrics["gui_page_total_bytes"] = sum(sizes.values())
        return ("FAIL" if failed else "PASS", "; ".join(failed) if failed else f"{len(sizes)} pages valid",
                metrics, {"sizes": sizes, "failed": failed})

    def test_scpi_identity(self):
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            identity, elapsed = client.query("*IDN?")
            version, version_elapsed = client.query("SYST:VERS?")
            passed = bool(identity) and "E-Resistor" in identity and bool(version)
            return ("PASS" if passed else "FAIL", identity,
                    {"scpi_idn_ms": elapsed, "scpi_version_ms": version_elapsed},
                    {"greeting": client.greeting, "identity": identity, "version": version})

    def test_scpi_state_consistency(self):
        _, http_state, _ = self._get_state()
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            status, status_ms = client.query("SYST:STAT?")
            state_text, scpi_state, state_ms, state_attempts = self._query_scpi_state(client)
        mismatches = []
        for ch in range(1, 9):
            http_mask = http_state.get("channels", {}).get(ch, {}).get("mask")
            scpi_mask = scpi_state.get(ch, {}).get("mask")
            if http_mask != scpi_mask:
                mismatches.append({"channel": ch, "http": http_mask, "scpi": scpi_mask})
        passed = len(scpi_state) == 8 and not mismatches and "core1_ready=" in status
        return ("PASS" if passed else "FAIL", "state consistent" if passed else f"mismatches={mismatches}",
                {"scpi_status_ms": status_ms, "scpi_state_ms": state_ms},
                {
                    "status": status,
                    "mismatches": mismatches,
                    "state_attempts": state_attempts,
                    "raw_state_excerpt": response_excerpt(state_text),
                })

    def test_calibration(self):
        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 5.0), self.logger) as client:
            response, elapsed = client.query("CAL:RES?", response_timeout_s=max(self.config.timeout_s, 5.0))
        parsed = parse_calibration_compact(response)
        failures = []
        tables: dict[int, dict[int, float]] = {}
        for ch in range(1, 9):
            rows = parsed.get(ch, [])
            if len(rows) != 16:
                failures.append(f"CH{ch} has {len(rows)} rows")
                continue
            bits = sorted(row["bit"] for row in rows)
            if bits != list(range(16)):
                failures.append(f"CH{ch} bit set invalid: {bits}")
            invalid = [row for row in rows if not math.isfinite(row["resistance_ohm"]) or row["resistance_ohm"] <= 0.0]
            if invalid:
                failures.append(f"CH{ch} has {len(invalid)} non-positive/non-finite values")
            tables[ch] = {int(row["bit"]): float(row["resistance_ohm"]) for row in rows}
        passed = not failures and len(parsed) == 8
        if passed:
            self.calibration_tables = tables
        return ("PASS" if passed else "FAIL", "8 x 16 table valid" if passed else "; ".join(failures),
                {"calibration_query_ms": elapsed, "calibration_response_bytes": len(response.encode())},
                {"failures": failures})

    def test_scpi_error_queue(self):
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            client.query("*CLS")
            invalid, _ = client.query("THIS:COMMAND:DOES:NOT:EXIST?")
            error, _ = client.query("SYST:ERR?")
            cleared, _ = client.query("SYST:ERR?")
        passed = invalid.startswith("ERR") and ("Undefined" in error or "-113" in error) and error != cleared
        return "PASS" if passed else "FAIL", f"invalid={invalid!r}, error={error!r}, cleared={cleared!r}", {}, {}

    def test_scpi_overflow_recovery(self):
        oversized = "X" * 220
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            overflow_response, _ = client.query(oversized)
            identity, elapsed = client.query("*IDN?")
        recovered = "E-Resistor" in identity
        single_error = "Input buffer overflow" in overflow_response and "Undefined header" not in overflow_response
        passed = recovered and (self.config.gate == "G0" or single_error)
        return (
            "PASS" if passed else "FAIL",
            f"recovered={recovered}, single_error={single_error}, overflow={overflow_response[:160]!r}",
            {"post_overflow_idn_ms": elapsed},
            {"overflow_response": overflow_response, "recovered": recovered, "single_error": single_error},
        )

    def test_scpi_target_calculation(self):
        if int(self.config.gate[1:]) < 2:
            return "SKIP", "TARGET:CALC? is introduced at Gate 2", {}, {}

        before_text, before_state, _ = self._get_state()
        del before_text
        if not self.calibration_tables:
            with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 5.0), self.logger) as client:
                calibration_text, _ = client.query("CAL:RES?", response_timeout_s=max(self.config.timeout_s, 5.0))
            parsed_calibration = parse_calibration_compact(calibration_text)
            self.calibration_tables = {
                ch: {int(row["bit"]): float(row["resistance_ohm"]) for row in rows}
                for ch, rows in parsed_calibration.items()
            }

        channel = max(1, min(8, self.config.hil_channel))
        calibration = self.calibration_tables.get(channel, {})
        if len(calibration) != 16:
            return "FAIL", f"CH{channel} calibration unavailable", {}, {}

        targets = (626.0, 1000.0, 10000.0, 100000.0, 400000.0)
        response_latencies: list[float] = []
        firmware_elapsed_us: list[float] = []
        candidate_counts: list[int] = []
        failures: list[str] = []
        cases: list[dict] = []

        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 5.0), self.logger) as client:
            for target in targets:
                response, elapsed_ms = client.query(
                    f"CH{channel}:TARGET:CALC? {target:.6f}",
                    response_timeout_s=max(self.config.timeout_s, 5.0),
                )
                response_latencies.append(elapsed_ms)
                if response.startswith("ERR"):
                    failures.append(f"target {target:g}: {response}")
                    continue
                try:
                    fields = parse_key_value_response(response)
                    mask = int(fields["mask"], 16)
                    requested = float(fields["requested_ohm"])
                    calculated = float(fields["calculated_ohm"])
                    absolute_error = float(fields["absolute_error_ohm"])
                    percent_error = float(fields["error_percent"])
                    candidates = int(fields["candidates"])
                    elapsed_us = int(fields["elapsed_us"])
                    expected = equivalent_resistance_ohm(mask, calibration)
                    expected_absolute = abs(expected - target)
                    expected_percent = (expected - target) / target * 100.0
                    tolerance_ohm = max(0.02, abs(expected) * 5.0e-6)
                    checks = {
                        "requested": abs(requested - target) <= max(1.0e-6, target * 1.0e-8),
                        "calculated": abs(calculated - expected) <= tolerance_ohm,
                        "absolute_error": abs(absolute_error - expected_absolute) <= tolerance_ohm,
                        "error_percent": abs(percent_error - expected_percent) <= 0.00002,
                        "candidate_count": 1 <= candidates <= 65535,
                        "deadline": 0 <= elapsed_us <= 2_000_000,
                    }
                    if not all(checks.values()):
                        failures.append(f"target {target:g}: checks={checks}")
                    firmware_elapsed_us.append(float(elapsed_us))
                    candidate_counts.append(candidates)
                    cases.append({
                        "target_ohm": target, "response": response, "mask": f"{mask:04X}",
                        "expected_ohm": expected, "checks": checks,
                    })
                except Exception as exc:
                    failures.append(f"target {target:g}: {type(exc).__name__}: {exc}")

        _, after_state, _ = self._get_state()
        before_masks = {ch: data.get("mask") for ch, data in before_state.get("channels", {}).items()}
        after_masks = {ch: data.get("mask") for ch, data in after_state.get("channels", {}).items()}
        masks_unchanged = before_masks == after_masks
        if not masks_unchanged:
            failures.append(f"output masks changed: before={before_masks}, after={after_masks}")

        metrics: dict[str, float | int] = {}
        metrics.update(latency_metrics(response_latencies, "target_calc_scpi"))
        if firmware_elapsed_us:
            metrics.update({
                "target_search_firmware_count": len(firmware_elapsed_us),
                "target_search_firmware_avg_us": statistics.fmean(firmware_elapsed_us),
                "target_search_firmware_p50_us": percentile(firmware_elapsed_us, 0.50),
                "target_search_firmware_p95_us": percentile(firmware_elapsed_us, 0.95),
                "target_search_firmware_max_us": max(firmware_elapsed_us),
                "target_search_max_candidates": max(candidate_counts),
            })
        return (
            "PASS" if not failures else "FAIL",
            f"{len(cases)}/{len(targets)} targets verified; masks_unchanged={masks_unchanged}; failures={len(failures)}",
            metrics,
            {"channel": channel, "cases": cases, "failures": failures, "masks_unchanged": masks_unchanged},
        )

    def test_calibration_storage_presence(self):
        response = self.http.request("/api/calibration/download_all")
        markers = re.findall(
            r"^#BEGIN\s+CH([1-8])\b[^\n]*\bsaved=([01])\b[^\n]*\bsize=(\d+)",
            response.text,
            flags=re.IGNORECASE | re.MULTILINE,
        )
        by_channel = {
            int(channel): {"saved": int(saved), "size": int(size)}
            for channel, saved, size in markers
        }
        failures: list[str] = []
        if response.status != 200:
            failures.append(f"HTTP status {response.status}")
        if len(by_channel) != 8:
            failures.append(f"expected 8 calibration markers, got {len(by_channel)}")
        missing = [ch for ch in range(1, 9) if by_channel.get(ch, {}).get("saved") != 1]
        empty = [ch for ch in range(1, 9) if by_channel.get(ch, {}).get("saved") == 1 and by_channel[ch]["size"] <= 0]
        if missing:
            failures.append(f"no saved calibration for channels {missing}")
        if empty:
            failures.append(f"empty saved calibration for channels {empty}")
        metrics = {
            "calibration_saved_channel_count": sum(item["saved"] == 1 for item in by_channel.values()),
            "calibration_total_saved_bytes": sum(item["size"] for item in by_channel.values()),
            "calibration_status_http_ms": response.elapsed_ms,
        }
        return (
            "PASS" if not failures else "FAIL",
            "all eight saved calibration files are present" if not failures else "; ".join(failures),
            metrics,
            {"channels": by_channel, "failures": failures},
        )

    def _query_core_transport(self, client: ScpiClient | None = None) -> tuple[dict[str, int], float, str]:
        owns_client = client is None
        if client is None:
            client = ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger)
            client.connect()
        try:
            response, elapsed = client.query("SYST:CORE:TRANSPORT?")
        finally:
            if owns_client:
                client.close()
        fields = parse_key_value_response(response)
        parsed = {key: int(value, 0) for key, value in fields.items()}
        return parsed, elapsed, response

    def _query_core_profile(self, client: ScpiClient | None = None) -> tuple[dict[str, int], float, str]:
        owns_client = client is None
        if client is None:
            client = ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger)
            client.connect()
        try:
            response, elapsed = client.query("SYST:CORE:PROFILE?")
        finally:
            if owns_client:
                client.close()
        fields = parse_key_value_response(response)
        parsed = {key: int(value, 0) for key, value in fields.items()}
        return parsed, elapsed, response

    def test_core_transport_diagnostics(self):
        if int(self.config.gate[1:]) < 3:
            return "SKIP", "Core transport diagnostics are introduced at Gate 3", {}, {}
        values, elapsed, raw = self._query_core_transport()
        required = {
            "generation", "last_submitted", "last_completed", "command_overflows",
            "result_overflows", "timeouts", "expired", "generation_rejects",
            "invalid_commands", "policy_installs", "core0_failsafe",
        }
        missing = sorted(required - values.keys())
        failures: list[str] = []
        if missing:
            failures.append(f"missing={missing}")
        for key in ("command_overflows", "result_overflows", "timeouts", "expired",
                    "generation_rejects", "invalid_commands", "core0_failsafe"):
            if values.get(key, 0) != 0:
                failures.append(f"{key}={values.get(key)}")
        if values.get("generation", 0) < 1:
            failures.append("generation is not positive")
        if values.get("policy_installs", 0) < 1:
            failures.append("no installed Core 1 policy")
        if values.get("last_completed", 0) > values.get("last_submitted", 0):
            failures.append("last_completed exceeds last_submitted")
        metrics = {f"core_transport_{key}": value for key, value in values.items()}
        metrics["core_transport_query_ms"] = elapsed
        return (
            "PASS" if not failures else "FAIL",
            "transport diagnostics healthy" if not failures else "; ".join(failures),
            metrics,
            {"values": values, "raw": raw, "failures": failures},
        )

    def test_core_snapshot_consistency(self):
        if int(self.config.gate[1:]) < 3:
            return "SKIP", "Coherent Core 1 snapshots are introduced at Gate 3", {}, {}
        http_text, http_state, http_ms = self._get_state()
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            raw, scpi_ms = client.query("SYST:CORE:SNAPSHOT?")
        fields = parse_key_value_response(raw)
        failures: list[str] = []
        try:
            snapshot_sequence = int(fields["snapshot_sequence"], 0)
            last_command_sequence = int(fields["last_command_sequence"], 0)
            generation = int(fields["generation"], 0)
            flags = int(fields["flags"], 0)
        except (KeyError, ValueError) as exc:
            return "FAIL", f"invalid snapshot header: {exc}", {}, {"raw": raw}
        http_snapshot_sequence = int(http_state.get("core_snapshot_sequence", "-1"))
        http_last_command = int(http_state.get("core_snapshot_last_command_sequence", "-1"))
        http_generation = int(http_state.get("core_snapshot_generation", "-1"))
        http_flags = int(http_state.get("core_snapshot_flags", "-1"))
        if snapshot_sequence != http_snapshot_sequence:
            failures.append(f"snapshot sequence HTTP={http_snapshot_sequence} SCPI={snapshot_sequence}")
        if last_command_sequence != http_last_command:
            failures.append(f"last command HTTP={http_last_command} SCPI={last_command_sequence}")
        if generation != http_generation:
            failures.append(f"generation HTTP={http_generation} SCPI={generation}")
        if flags != http_flags:
            failures.append(f"flags HTTP={http_flags} SCPI={flags}")
        required_flags = 0x01 | 0x02 | 0x08
        if (flags & required_flags) != required_flags:
            failures.append(f"required safe/ready/policy flags missing: flags={flags}")
        mask_pairs = []
        for ch in range(1, 9):
            scpi_mask = fields.get(f"ch{ch}_mask", "").lower().removeprefix("0x").upper()
            http_mask = http_state.get("channels", {}).get(ch, {}).get("mask")
            mask_pairs.append({"channel": ch, "http": http_mask, "scpi": scpi_mask})
            if scpi_mask != http_mask:
                failures.append(f"CH{ch} HTTP={http_mask} SCPI={scpi_mask}")
        return (
            "PASS" if not failures else "FAIL",
            "HTTP and SCPI use one coherent output snapshot" if not failures else "; ".join(failures),
            {
                "core_snapshot_http_ms": http_ms,
                "core_snapshot_scpi_ms": scpi_ms,
                "core_snapshot_sequence": snapshot_sequence,
                "core_snapshot_generation": generation,
                "core_snapshot_flags": flags,
            },
            {"raw_http_excerpt": response_excerpt(http_text), "raw_scpi": raw,
             "mask_pairs": mask_pairs, "failures": failures},
        )

    def test_core_transport_safe_stress(self):
        if int(self.config.gate[1:]) < 3:
            return "SKIP", "Sequenced Core 1 transport stress is introduced at Gate 3", {}, {}
        if not self.config.allow_output_tests:
            return "SKIP", "requires safe-output authorization", {}, {}
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            before, before_ms, before_raw = self._query_core_transport(client)
            latencies: list[float] = []
            failures: list[str] = []
            iterations = max(50, self.config.stress_iterations)
            for index in range(iterations):
                response, elapsed = client.query("ALL:OFF")
                latencies.append(elapsed)
                if response != "OK":
                    failures.append(f"iteration {index + 1}: {response!r}")
                    break
            state_text, state, state_ms, state_attempts = self._query_scpi_state(client)
            after, after_ms, after_raw = self._query_core_transport(client)
        no_error_keys = ("command_overflows", "result_overflows", "timeouts", "expired",
                         "generation_rejects", "invalid_commands", "core0_failsafe")
        deltas = {key: after.get(key, 0) - before.get(key, 0) for key in no_error_keys}
        nonzero = {key: value for key, value in deltas.items() if value != 0}
        if nonzero:
            failures.append(f"transport error counter deltas={nonzero}")
        sequence_delta = after.get("last_submitted", 0) - before.get("last_submitted", 0)
        if sequence_delta < len(latencies):
            failures.append(f"submitted sequence delta {sequence_delta} < completed iterations {len(latencies)}")
        if after.get("last_completed") != after.get("last_submitted"):
            failures.append(
                f"last completed {after.get('last_completed')} != last submitted {after.get('last_submitted')}"
            )
        if after.get("generation") != before.get("generation"):
            failures.append(
                f"normal ALL:OFF changed generation {before.get('generation')} -> {after.get('generation')}"
            )
        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            failures.append(f"final masks not all zero: {state}")
        metrics: dict[str, float | int] = {
            "core_transport_stress_iterations": len(latencies),
            "core_transport_stress_sequence_delta": sequence_delta,
            "core_transport_before_query_ms": before_ms,
            "core_transport_after_query_ms": after_ms,
            "core_transport_final_state_ms": state_ms,
        }
        metrics.update(latency_metrics(latencies, "core_transport_all_off"))
        for key, value in deltas.items():
            metrics[f"core_transport_delta_{key}"] = value
        return (
            "PASS" if not failures and len(latencies) == iterations else "FAIL",
            f"{len(latencies)}/{iterations} sequenced ALL:OFF commands; failures={len(failures)}",
            metrics,
            {"before": before, "after": after, "deltas": deltas, "failures": failures,
             "before_raw": before_raw, "after_raw": after_raw,
             "state_attempts": state_attempts, "state_raw": response_excerpt(state_text)},
        )

    def test_gate3_profile_zero_baseline(self):
        """Measure the sequential Gate 3 profile path for Gate 4 comparison."""
        if int(self.config.gate[1:]) != 3:
            return "SKIP", "Gate 3 baseline is collected only on G3 firmware", {}, {}
        if not self.config.allow_output_tests:
            return "SKIP", "requires safe-output authorization", {}, {}

        masks = ",".join(["0000"] * 8)
        iterations = max(50, min(250, self.config.stress_iterations))
        latencies: list[float] = []
        internal_us: list[float] = []
        failures: list[str] = []
        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 4.0), self.logger) as client:
            before, before_ms, before_raw = self._query_core_transport(client)
            profile_before, profile_before_ms, profile_before_raw = self._query_core_profile(client)
            for index in range(iterations):
                response, elapsed = client.query(
                    f"ROUT:ALL:MASK {masks}",
                    response_timeout_s=max(self.config.timeout_s, 4.0),
                )
                latencies.append(elapsed)
                if response != "OK":
                    failures.append(f"iteration {index + 1}: {response!r}")
                    break
                profile_now, _profile_query_ms, _profile_raw = self._query_core_profile(client)
                value = profile_now.get("last_us", 0)
                if value <= 0:
                    failures.append(f"iteration {index + 1}: invalid internal duration {value}")
                    break
                internal_us.append(float(value))
            state_text, state, state_ms, state_attempts = self._query_scpi_state(client)
            profile_after, profile_after_ms, profile_after_raw = self._query_core_profile(client)
            after, after_ms, after_raw = self._query_core_transport(client)

        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            failures.append(f"final masks not all zero: {state}")
        if after.get("last_completed") != after.get("last_submitted"):
            failures.append(
                f"last completed {after.get('last_completed')} != last submitted {after.get('last_submitted')}"
            )
        transition_delta = profile_after.get("transitions", 0) - profile_before.get("transitions", 0)
        bbm_delta = profile_after.get("bbm_count", 0) - profile_before.get("bbm_count", 0)
        failure_delta = profile_after.get("failures", 0) - profile_before.get("failures", 0)
        if transition_delta != len(internal_us):
            failures.append(f"profile transition delta {transition_delta} != completed {len(internal_us)}")
        if bbm_delta != len(internal_us) * 8:
            failures.append(f"Gate 3 expected eight BBM operations per profile: delta={bbm_delta}")
        if failure_delta != 0:
            failures.append(f"profile failure delta={failure_delta}")

        error_keys = (
            "command_overflows", "result_overflows", "timeouts", "expired",
            "generation_rejects", "invalid_commands", "core0_failsafe",
        )
        deltas = {key: after.get(key, 0) - before.get(key, 0) for key in error_keys}
        nonzero = {key: value for key, value in deltas.items() if value != 0}
        if nonzero:
            failures.append(f"transport error counter deltas={nonzero}")

        metrics: dict[str, float | int] = {
            "profile_zero_iterations": len(internal_us),
            "profile_zero_before_query_ms": before_ms,
            "profile_zero_after_query_ms": after_ms,
            "profile_zero_profile_before_query_ms": profile_before_ms,
            "profile_zero_profile_after_query_ms": profile_after_ms,
            "profile_zero_final_state_ms": state_ms,
            "profile_zero_transition_delta": transition_delta,
            "profile_zero_bbm_delta": bbm_delta,
            "profile_zero_failure_delta": failure_delta,
        }
        metrics.update(latency_metrics(latencies, "profile_zero"))
        metrics.update(latency_metrics([value / 1000.0 for value in internal_us], "profile_internal"))
        for key, value in deltas.items():
            metrics[f"profile_zero_delta_{key}"] = value

        return (
            "PASS" if not failures and len(internal_us) == iterations else "FAIL",
            f"{len(internal_us)}/{iterations} sequential G3 profiles; BBM={bbm_delta}; failures={len(failures)}",
            metrics,
            {
                "before": before,
                "after": after,
                "profile_before": profile_before,
                "profile_after": profile_after,
                "internal_duration_us": internal_us,
                "deltas": deltas,
                "before_raw": before_raw,
                "after_raw": after_raw,
                "profile_before_raw": profile_before_raw,
                "profile_after_raw": profile_after_raw,
                "state_attempts": state_attempts,
                "state_raw": response_excerpt(state_text),
                "failures": failures,
            },
        )

    def test_gate4_profile_diagnostics(self):
        if int(self.config.gate[1:]) < 4:
            return "SKIP", "Gate 4 profile diagnostics require G4 or later", {}, {}
        profile, profile_ms, profile_raw = self._query_core_profile()
        transport, transport_ms, transport_raw = self._query_core_transport()
        http_text, http_state, http_ms = self._get_state()
        required = {
            "transitions", "failures", "bbm_count", "last_us", "max_us",
            "clear_last_us", "clear_max_us",
        }
        missing = sorted(required - profile.keys())
        failures: list[str] = []
        if missing:
            failures.append(f"missing profile fields {missing}")
        if profile.get("failures", 0) != 0:
            failures.append(f"profile_failures={profile.get('failures')}")
        transport_map = {
            "profile_transitions": "transitions",
            "profile_failures": "failures",
            "profile_bbm_count": "bbm_count",
            "profile_last_us": "last_us",
            "profile_max_us": "max_us",
            "profile_clear_last_us": "clear_last_us",
            "profile_clear_max_us": "clear_max_us",
        }
        mismatches = {}
        for transport_key, profile_key in transport_map.items():
            if transport.get(transport_key) != profile.get(profile_key):
                mismatches[transport_key] = {
                    "transport": transport.get(transport_key),
                    "profile": profile.get(profile_key),
                }
        if mismatches:
            failures.append(f"transport/profile mismatch={mismatches}")
        http_map = {
            "core_profile_transition_count": "transitions",
            "core_profile_failure_count": "failures",
            "core_profile_break_before_make_count": "bbm_count",
            "core_profile_last_duration_us": "last_us",
            "core_profile_max_duration_us": "max_us",
            "core_profile_last_clear_duration_us": "clear_last_us",
            "core_profile_max_clear_duration_us": "clear_max_us",
        }
        http_mismatches = {}
        for http_key, profile_key in http_map.items():
            try:
                http_value = int(http_state.get(http_key, "-1"), 0)
            except (TypeError, ValueError):
                http_value = -1
            if http_value != profile.get(profile_key):
                http_mismatches[http_key] = {
                    "http": http_value,
                    "profile": profile.get(profile_key),
                }
        if http_mismatches:
            failures.append(f"HTTP/profile mismatch={http_mismatches}")
        metrics = {
            "gate4_profile_query_ms": profile_ms,
            "gate4_profile_transport_query_ms": transport_ms,
            "gate4_profile_http_query_ms": http_ms,
        }
        metrics.update({f"gate4_{key}": value for key, value in profile.items()})
        return (
            "PASS" if not failures else "FAIL",
            "Gate 4 profile diagnostics are coherent" if not failures else "; ".join(failures),
            metrics,
            {
                "profile": profile,
                "transport": transport,
                "mismatches": mismatches,
                "http_mismatches": http_mismatches,
                "profile_raw": profile_raw,
                "transport_raw": transport_raw,
                "http_raw": response_excerpt(http_text),
                "failures": failures,
            },
        )

    def _gate3_profile_baseline_p95_ms(self) -> tuple[float | None, str]:
        if not self.config.baseline:
            return None, "no Gate 3 baseline path was supplied"
        path = Path(self.config.baseline).expanduser()
        if not path.exists():
            return None, f"Gate 3 baseline does not exist: {path}"
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except Exception as exc:
            return None, f"unable to read Gate 3 baseline: {type(exc).__name__}: {exc}"
        value = payload.get("flat_metrics", {}).get("G3-004.profile_internal_p95_ms")
        if not isinstance(value, (int, float)) or not math.isfinite(float(value)) or float(value) <= 0.0:
            return None, "baseline is missing G3-004.profile_internal_p95_ms"
        return float(value), str(path)

    def test_gate4_zero_profile_stress(self):
        if int(self.config.gate[1:]) < 4:
            return "SKIP", "Gate 4 profile stress requires G4 or later", {}, {}
        if not self.config.allow_output_tests:
            return "SKIP", "requires safe-output authorization", {}, {}

        baseline_p95_ms, baseline_note = self._gate3_profile_baseline_p95_ms()
        masks = ",".join(["0000"] * 8)
        iterations = max(1000, min(2000, self.config.stress_iterations))
        scpi_ms: list[float] = []
        internal_ms: list[float] = []
        failures: list[str] = []
        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 4.0), self.logger) as client:
            transport_before, transport_before_ms, transport_before_raw = self._query_core_transport(client)
            profile_before, profile_before_ms, profile_before_raw = self._query_core_profile(client)
            for index in range(iterations):
                response, elapsed = client.query(
                    f"ROUT:ALL:MASK {masks}",
                    response_timeout_s=max(self.config.timeout_s, 4.0),
                )
                scpi_ms.append(elapsed)
                if response != "OK":
                    failures.append(f"iteration {index + 1}: {response!r}")
                    break
                current, _query_ms, _raw = self._query_core_profile(client)
                duration_us = current.get("last_us", 0)
                if duration_us <= 0:
                    failures.append(f"iteration {index + 1}: invalid internal duration {duration_us}")
                    break
                internal_ms.append(duration_us / 1000.0)
            state_text, state, state_ms, state_attempts = self._query_scpi_state(client)
            profile_after, profile_after_ms, profile_after_raw = self._query_core_profile(client)
            transport_after, transport_after_ms, transport_after_raw = self._query_core_transport(client)

        completed = len(internal_ms)
        transition_delta = profile_after.get("transitions", 0) - profile_before.get("transitions", 0)
        bbm_delta = profile_after.get("bbm_count", 0) - profile_before.get("bbm_count", 0)
        profile_failure_delta = profile_after.get("failures", 0) - profile_before.get("failures", 0)
        if transition_delta != completed:
            failures.append(f"transition delta {transition_delta} != completed {completed}")
        if bbm_delta != completed:
            failures.append(f"expected one BBM per profile: bbm_delta={bbm_delta}, completed={completed}")
        if profile_failure_delta != 0:
            failures.append(f"profile failure delta={profile_failure_delta}")
        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            failures.append(f"final masks not all zero: {state}")

        error_keys = (
            "command_overflows", "result_overflows", "timeouts", "expired",
            "generation_rejects", "invalid_commands", "core0_failsafe",
        )
        transport_deltas = {
            key: transport_after.get(key, 0) - transport_before.get(key, 0)
            for key in error_keys
        }
        nonzero = {key: value for key, value in transport_deltas.items() if value != 0}
        if nonzero:
            failures.append(f"transport error deltas={nonzero}")

        current_p95_ms = percentile(internal_ms, 0.95) if internal_ms else math.inf
        improvement_percent = math.nan
        if baseline_p95_ms is None:
            failures.append(baseline_note)
        else:
            improvement_percent = (baseline_p95_ms - current_p95_ms) / baseline_p95_ms * 100.0
            if improvement_percent < 30.0:
                failures.append(
                    f"internal profile p95 improvement {improvement_percent:.2f}% < 30.00% "
                    f"(G3={baseline_p95_ms:.6f} ms, G4={current_p95_ms:.6f} ms)"
                )

        metrics: dict[str, float | int] = {
            "gate4_profile_iterations": completed,
            "gate4_profile_transition_delta": transition_delta,
            "gate4_profile_bbm_delta": bbm_delta,
            "gate4_profile_failure_delta": profile_failure_delta,
            "gate4_profile_g3_baseline_p95_ms": baseline_p95_ms if baseline_p95_ms is not None else math.nan,
            "gate4_profile_improvement_percent": improvement_percent,
            "gate4_profile_state_query_ms": state_ms,
            "gate4_profile_before_query_ms": profile_before_ms,
            "gate4_profile_after_query_ms": profile_after_ms,
            "gate4_transport_before_query_ms": transport_before_ms,
            "gate4_transport_after_query_ms": transport_after_ms,
        }
        metrics.update(latency_metrics(scpi_ms, "gate4_profile_scpi"))
        metrics.update(latency_metrics(internal_ms, "gate4_profile_internal"))
        for key, value in transport_deltas.items():
            metrics[f"gate4_transport_delta_{key}"] = value

        return (
            "PASS" if not failures and completed == iterations else "FAIL",
            f"{completed}/{iterations} profiles; BBM={bbm_delta}; improvement={improvement_percent:.2f}%"
            if math.isfinite(improvement_percent)
            else f"{completed}/{iterations} profiles; baseline unavailable",
            metrics,
            {
                "baseline": baseline_note,
                "profile_before": profile_before,
                "profile_after": profile_after,
                "transport_before": transport_before,
                "transport_after": transport_after,
                "transport_deltas": transport_deltas,
                "internal_ms": internal_ms,
                "profile_before_raw": profile_before_raw,
                "profile_after_raw": profile_after_raw,
                "transport_before_raw": transport_before_raw,
                "transport_after_raw": transport_after_raw,
                "state_attempts": state_attempts,
                "state_raw": response_excerpt(state_text),
                "failures": failures,
            },
        )

    def test_gate4_profile_snapshot_observation(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        if int(self.config.gate[1:]) < 4:
            return "SKIP", "Gate 4 profile snapshot observation requires G4 or later", {}, {}
        assert self.dmm is not None
        self._hil_all_off_verified()

        channel = self.config.hil_channel
        target_masks = ["0000"] * 8
        target_masks[channel - 1] = "0001"
        target_command = "ROUT:ALL:MASK " + ",".join(target_masks)
        zero_command = "ROUT:ALL:MASK " + ",".join(["0000"] * 8)
        allowed = {
            tuple(["0000"] * 8),
            tuple(target_masks),
        }

        observations: list[tuple[str, ...]] = []
        observer_errors: list[str] = []
        stop = __import__("threading").Event()

        def observe() -> None:
            local_http = HttpClient(
                self.config.host, self.config.http_port, self.config.timeout_s, self.logger
            )
            while not stop.is_set():
                try:
                    response = local_http.request("/state")
                    if response.status != 200:
                        observer_errors.append(f"HTTP {response.status}")
                        continue
                    parsed = parse_http_state(response.text)
                    channels = parsed.get("channels", {})
                    if len(channels) == 8:
                        observations.append(tuple(channels[ch]["mask"] for ch in range(1, 9)))
                    else:
                        observer_errors.append(f"parsed {len(channels)} channels")
                except Exception as exc:
                    observer_errors.append(f"{type(exc).__name__}: {exc}")
                time.sleep(0.001)

        thread = __import__("threading").Thread(target=observe, daemon=True)
        thread.start()
        command_failures: list[str] = []
        latencies: list[float] = []
        try:
            with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 4.0), self.logger) as client:
                for index in range(20):
                    for command in (target_command, zero_command):
                        response, elapsed = client.query(command, response_timeout_s=4.0)
                        latencies.append(elapsed)
                        if response != "OK":
                            command_failures.append(f"cycle {index + 1}: {command!r} -> {response!r}")
                            break
                    if command_failures:
                        break
        finally:
            stop.set()
            thread.join(timeout=2.0)

        self._hil_all_off_verified()
        samples: list[float] = []
        dmm_ms = 0.0
        for _ in range(max(3, min(5, self.config.hil_sample_count))):
            value, elapsed = self.dmm.read_resistance()
            samples.append(value)
            dmm_ms += elapsed
            if self.config.hil_sample_interval_s:
                time.sleep(self.config.hil_sample_interval_s)
        finite = [value for value in samples if math.isfinite(value)]
        minimum = min(finite) if finite else math.inf

        mixed = [value for value in observations if value not in allowed]
        failures = list(command_failures)
        if observer_errors:
            failures.append(f"observer errors={observer_errors[:5]}")
        if not observations:
            failures.append("no coherent HTTP observations were captured")
        if mixed:
            failures.append(f"mixed published snapshots={mixed[:5]}")
        if minimum < self.config.hil_off_min_ohm:
            failures.append(f"final isolation {minimum:.6g} < {self.config.hil_off_min_ohm:.6g} ohm")

        metrics: dict[str, float | int] = {
            "gate4_snapshot_observation_count": len(observations),
            "gate4_snapshot_mixed_count": len(mixed),
            "gate4_snapshot_observer_error_count": len(observer_errors),
            "gate4_snapshot_final_off_min_ohm": minimum,
            "gate4_snapshot_dmm_ms": dmm_ms,
        }
        metrics.update(latency_metrics(latencies, "gate4_snapshot_profile_scpi"))
        return (
            "PASS" if not failures else "FAIL",
            f"observations={len(observations)}, mixed={len(mixed)}, failures={len(failures)}",
            metrics,
            {
                "allowed": [list(item) for item in allowed],
                "observations": [list(item) for item in observations[:200]],
                "mixed": [list(item) for item in mixed[:20]],
                "observer_errors": observer_errors,
                "command_failures": command_failures,
                "dmm_samples": samples,
                "failures": failures,
            },
        )

    def test_gate4_profile_failure_after_clear(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        if int(self.config.gate[1:]) < 4:
            return "SKIP", "Gate 4 profile fault injection requires G4 or later", {}, {}
        assert self.dmm is not None
        self._hil_all_off_verified()

        channel = self.config.hil_channel
        masks = ["0000"] * 8
        masks[channel - 1] = "0001"
        command = "ROUT:ALL:MASK " + ",".join(masks)

        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 4.0), self.logger) as client:
            mode, mode_ms = client.query("SYST:TEST:MODE?")
            if mode.strip() != "1":
                return "FAIL", "Gate 4 fault-injection firmware is not active", {}, {"mode": mode}
            before, before_ms, before_raw = self._query_core_profile(client)
            armed, arm_ms = client.query("SYST:TEST:PROFILE:FAIL:NEXT")
            if armed.strip() != "OK":
                return "FAIL", f"unable to arm profile failure: {armed!r}", {}, {}
            response, command_ms = client.query(command, response_timeout_s=4.0)
            state_text, state, state_ms, state_attempts = self._query_scpi_state(client)
            after, after_ms, after_raw = self._query_core_profile(client)

        samples: list[float] = []
        dmm_ms = 0.0
        for _ in range(max(3, min(5, self.config.hil_sample_count))):
            value, elapsed = self.dmm.read_resistance()
            samples.append(value)
            dmm_ms += elapsed
            if self.config.hil_sample_interval_s:
                time.sleep(self.config.hil_sample_interval_s)
        finite = [value for value in samples if math.isfinite(value)]
        minimum = min(finite) if finite else math.inf

        transition_delta = after.get("transitions", 0) - before.get("transitions", 0)
        failure_delta = after.get("failures", 0) - before.get("failures", 0)
        bbm_delta = after.get("bbm_count", 0) - before.get("bbm_count", 0)
        failures: list[str] = []
        if not response.startswith("ERR"):
            failures.append(f"injected profile was not rejected: {response!r}")
        if transition_delta != 0:
            failures.append(f"successful transition delta={transition_delta}")
        if failure_delta != 1:
            failures.append(f"profile failure delta={failure_delta}")
        if bbm_delta != 1:
            failures.append(f"BBM delta={bbm_delta}, expected 1")
        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            failures.append(f"software state is not all off: {state}")
        if minimum < self.config.hil_off_min_ohm:
            failures.append(f"physical isolation {minimum:.6g} < {self.config.hil_off_min_ohm:.6g} ohm")

        return (
            "PASS" if not failures else "FAIL",
            f"response={response!r}; failure_delta={failure_delta}; BBM={bbm_delta}; isolation={minimum:.6g}",
            {
                "gate4_fault_mode_query_ms": mode_ms,
                "gate4_fault_before_query_ms": before_ms,
                "gate4_fault_arm_ms": arm_ms,
                "gate4_fault_command_ms": command_ms,
                "gate4_fault_state_ms": state_ms,
                "gate4_fault_after_query_ms": after_ms,
                "gate4_fault_transition_delta": transition_delta,
                "gate4_fault_failure_delta": failure_delta,
                "gate4_fault_bbm_delta": bbm_delta,
                "gate4_fault_off_min_ohm": minimum,
                "gate4_fault_dmm_ms": dmm_ms,
            },
            {
                "before": before,
                "after": after,
                "before_raw": before_raw,
                "after_raw": after_raw,
                "state_attempts": state_attempts,
                "state_raw": response_excerpt(state_text),
                "dmm_samples": samples,
                "failures": failures,
            },
        )

    def test_gate3_test_mode(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        if int(self.config.gate[1:]) < 3:
            return "SKIP", "Gate 3 fault injection requires G3 or later", {}, {}
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            response, elapsed = client.query("SYST:TEST:MODE?")
        passed = response.strip() == "1"
        return (
            "PASS" if passed else "FAIL",
            "Gate 3 fault-injection build detected" if passed else f"test mode response={response!r}",
            {"gate3_test_mode_query_ms": elapsed},
            {"response": response},
        )

    def test_gate3_invalidated_command_no_ghost_actuation(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        assert self.dmm is not None
        self._hil_all_off_verified()
        before, _, _ = self._query_core_transport()
        channel = self.config.hil_channel
        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 4.0), self.logger) as client:
            mode, _ = client.query("SYST:TEST:MODE?")
            if mode.strip() != "1":
                return "FAIL", "fault-injection firmware is not active", {}, {"mode_response": mode}
            armed, arm_ms = client.query("SYST:TEST:CORE1:INVALIDATE:NEXT")
            if armed.strip() != "OK":
                return "FAIL", f"unable to arm queued invalidation: {armed!r}", {}, {}
            response, command_ms = client.query(f"CH{channel}:MASK 0001", response_timeout_s=4.0)
        time.sleep(0.25)
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            state_text, state, state_ms, state_attempts = self._query_scpi_state(client)
            after, after_ms, after_raw = self._query_core_transport(client)
        samples: list[float] = []
        dmm_ms = 0.0
        for _ in range(max(3, min(5, self.config.hil_sample_count))):
            value, elapsed = self.dmm.read_resistance()
            samples.append(value); dmm_ms += elapsed
            if self.config.hil_sample_interval_s:
                time.sleep(self.config.hil_sample_interval_s)
        finite = [value for value in samples if math.isfinite(value)]
        minimum = min(finite) if finite else math.inf
        failures: list[str] = []
        if not response.startswith("ERR"):
            failures.append(f"invalidated command was not rejected: {response!r}")
        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            failures.append(f"ghost output state detected: {state}")
        if minimum < self.config.hil_off_min_ohm:
            failures.append(f"physical isolation {minimum:.6g} < {self.config.hil_off_min_ohm:.6g} ohm")
        timeout_delta = after.get("timeouts", 0) - before.get("timeouts", 0)
        expired_delta = after.get("expired", 0) - before.get("expired", 0)
        reject_delta = after.get("generation_rejects", 0) - before.get("generation_rejects", 0)
        generation_delta = after.get("generation", 0) - before.get("generation", 0)
        if reject_delta < 1:
            failures.append(f"generation rejection did not advance: delta={reject_delta}")
        if timeout_delta != 0:
            failures.append(f"queued invalidation unexpectedly timed out: delta={timeout_delta}")
        if expired_delta != 0:
            failures.append(f"queued invalidation unexpectedly expired: delta={expired_delta}")
        if generation_delta < 1:
            failures.append(f"transport generation did not advance: delta={generation_delta}")
        return (
            "PASS" if not failures else "FAIL",
            "queued invalidation rejected the command before software or physical actuation" if not failures else "; ".join(failures),
            {
                "gate3_invalidate_arm_ms": arm_ms,
                "gate3_invalidate_command_ms": command_ms,
                "gate3_invalidate_state_ms": state_ms,
                "gate3_invalidate_transport_ms": after_ms,
                "gate3_invalidate_generation_delta": generation_delta,
                "gate3_invalidate_generation_reject_delta": reject_delta,
                "gate3_invalidate_timeout_delta": timeout_delta,
                "gate3_invalidate_expired_delta": expired_delta,
                "gate3_invalidate_off_min_ohm": minimum,
                "gate3_invalidate_dmm_ms": dmm_ms,
            },
            {"command_response": response, "samples": samples, "before": before, "after": after,
             "after_raw": after_raw, "state_attempts": state_attempts,
             "state_raw": response_excerpt(state_text), "failures": failures},
        )

    def test_gate3_timeout_no_ghost_actuation(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        assert self.dmm is not None
        self._hil_all_off_verified()
        before, _, _ = self._query_core_transport()
        delay_ms = 1500
        channel = self.config.hil_channel
        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 4.0), self.logger) as client:
            mode, _ = client.query("SYST:TEST:MODE?")
            if mode.strip() != "1":
                return "FAIL", "fault-injection firmware is not active", {}, {"mode_response": mode}
            armed, arm_ms = client.query(f"SYST:TEST:CORE1:DELAY {delay_ms}")
            if armed.strip() != "OK":
                return "FAIL", f"unable to arm delay: {armed!r}", {}, {}
            response, command_ms = client.query(
                f"CH{channel}:MASK 0001", response_timeout_s=max(4.0, delay_ms / 1000.0 + 2.0)
            )
        # Wait beyond the injected delay so a stale command would have reached the hardware.
        time.sleep(delay_ms / 1000.0 + 0.75)
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            state_text, state, state_ms, state_attempts = self._query_scpi_state(client)
            after, after_ms, after_raw = self._query_core_transport(client)
        samples: list[float] = []
        dmm_ms = 0.0
        for _ in range(max(3, min(5, self.config.hil_sample_count))):
            value, elapsed = self.dmm.read_resistance()
            samples.append(value); dmm_ms += elapsed
            if self.config.hil_sample_interval_s:
                time.sleep(self.config.hil_sample_interval_s)
        finite = [value for value in samples if math.isfinite(value)]
        minimum = min(finite) if finite else math.inf
        failures: list[str] = []
        if not response.startswith("ERR"):
            failures.append(f"delayed command did not time out: {response!r}")
        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            failures.append(f"ghost output state detected: {state}")
        if minimum < self.config.hil_off_min_ohm:
            failures.append(f"physical isolation {minimum:.6g} < {self.config.hil_off_min_ohm:.6g} ohm")
        timeout_delta = after.get("timeouts", 0) - before.get("timeouts", 0)
        expired_delta = after.get("expired", 0) - before.get("expired", 0)
        reject_delta = after.get("generation_rejects", 0) - before.get("generation_rejects", 0)
        stale_reject_delta = expired_delta + reject_delta
        if timeout_delta < 1:
            failures.append(f"timeout counter did not advance: delta={timeout_delta}")
        if stale_reject_delta < 1:
            failures.append(
                f"stale command was not rejected after timeout: expired_delta={expired_delta}, "
                f"generation_reject_delta={reject_delta}"
            )
        return (
            "PASS" if not failures else "FAIL",
            "timed-out command produced no software or physical actuation" if not failures else "; ".join(failures),
            {
                "gate3_fault_arm_ms": arm_ms,
                "gate3_fault_command_ms": command_ms,
                "gate3_fault_state_ms": state_ms,
                "gate3_fault_transport_ms": after_ms,
                "gate3_fault_timeout_delta": timeout_delta,
                "gate3_fault_expired_delta": expired_delta,
                "gate3_fault_generation_reject_delta": reject_delta,
                "gate3_fault_stale_reject_delta": stale_reject_delta,
                "gate3_fault_off_min_ohm": minimum,
                "gate3_fault_dmm_ms": dmm_ms,
            },
            {"command_response": response, "samples": samples, "before": before, "after": after,
             "after_raw": after_raw, "state_attempts": state_attempts,
             "state_raw": response_excerpt(state_text), "failures": failures},
        )

    def test_http_latency(self):
        ping = [self.http.request("/ping").elapsed_ms for _ in range(self.config.iterations)]
        state = [self.http.request("/state").elapsed_ms for _ in range(self.config.iterations)]
        metrics = {}
        metrics.update(latency_metrics(ping, "http_ping"))
        metrics.update(latency_metrics(state, "http_state"))
        return "PASS", f"{self.config.iterations} ping and state samples", metrics, {}

    def test_scpi_latency(self):
        values: list[float] = []
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            for _ in range(self.config.iterations):
                response, elapsed = client.query("*IDN?")
                if "E-Resistor" not in response:
                    return "FAIL", f"unexpected identity: {response!r}", {}, {}
                values.append(elapsed)
        return "PASS", f"{len(values)} samples", latency_metrics(values, "scpi_idn"), {}

    def test_heap_stability(self):
        _, before, _ = self._get_state()
        before_heap = int(before.get("heap_free_bytes", "0"))
        latencies = []
        for _ in range(self.config.stress_iterations):
            response = self.http.request("/state")
            if response.status != 200:
                return "FAIL", f"HTTP {response.status} during heap stress", {}, {}
            latencies.append(response.elapsed_ms)
        _, after, _ = self._get_state()
        after_heap = int(after.get("heap_free_bytes", "0"))
        drift = before_heap - after_heap
        passed = drift <= self.config.heap_drift_limit_bytes
        metrics = {"heap_before_bytes": before_heap, "heap_after_bytes": after_heap, "heap_decline_bytes": drift}
        metrics.update(latency_metrics(latencies, "heap_stress_state"))
        return ("PASS" if passed else "FAIL",
                f"heap decline {drift} bytes; limit {self.config.heap_drift_limit_bytes}", metrics, {})

    def test_concurrent_polling(self):
        count = max(10, self.config.stress_iterations // 2)

        def http_worker() -> list[float]:
            return [self.http.request("/state").elapsed_ms for _ in range(count)]

        def scpi_worker() -> list[float]:
            values = []
            with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
                for _ in range(count):
                    _response, _state, elapsed, _attempts = self._query_scpi_state(client)
                    values.append(elapsed)
            return values

        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            http_future = executor.submit(http_worker)
            scpi_future = executor.submit(scpi_worker)
            http_values = http_future.result()
            scpi_values = scpi_future.result()
        metrics = {}
        metrics.update(latency_metrics(http_values, "concurrent_http_state"))
        metrics.update(latency_metrics(scpi_values, "concurrent_scpi_state"))
        return "PASS", f"{count} concurrent requests per protocol", metrics, {}

    def test_calibration_bundle_download(self):
        response = self.http.request("/api/calibration/download_all")
        body = response.text
        passed = response.status == 200 and all(f"BEGIN CH{ch}" in body for ch in range(1, 9))
        return ("PASS" if passed else "FAIL", f"HTTP {response.status}, {len(response.body)} bytes",
                {"calibration_bundle_bytes": len(response.body), "calibration_bundle_ms": response.elapsed_ms}, {})

    def test_log_download(self):
        response = self.http.request("/log_download")
        content_type = response.headers.get("Content-Type", "")
        disposition = response.headers.get("Content-Disposition", "")
        body = response.text
        has_header = body.startswith("E-Resistor event log")
        has_identity = "Firmware version:" in body and "Event history" in body
        text_type = "text/plain" in content_type.lower() or "text/" in content_type.lower()
        not_html = "<html" not in body[:256].lower()
        passed = (
            response.status == 200
            and len(response.body) > 0
            and text_type
            and has_header
            and has_identity
            and not_html
        )
        details = {
            "content_type": content_type,
            "content_disposition": disposition,
            "has_header": has_header,
            "has_identity": has_identity,
            "not_html": not_html,
            "body_excerpt": response_excerpt(body),
        }
        return (
            "PASS" if passed else "FAIL",
            f"HTTP {response.status}, bytes={len(response.body)}, content-type={content_type}",
            {"log_download_bytes": len(response.body), "log_download_ms": response.elapsed_ms},
            details,
        )

    def test_zero_mask_command_path(self):
        if not self.config.allow_output_tests:
            return "SKIP", "requires --allow-output-tests", {}, {}
        masks = ",".join(["0000"] * 8)
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            response, elapsed = client.query(f"ROUT:ALL:MASK {masks}")
            state_text, state, state_elapsed, state_attempts = self._query_scpi_state(client)
        passed = response == "OK" and len(state) == 8 and all(item["mask"] == "0000" for item in state.values())
        return ("PASS" if passed else "FAIL", f"apply={response!r}",
                {"zero_profile_apply_ms": elapsed, "zero_profile_verify_ms": state_elapsed},
                {"state_attempts": state_attempts, "raw_state_excerpt": response_excerpt(state_text)})

    def test_repeated_all_off(self):
        if not self.config.allow_output_tests:
            return "SKIP", "requires --allow-output-tests", {}, {}
        latencies = []
        responses = []
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            for _ in range(min(self.config.iterations, 20)):
                response, elapsed = client.query("ALL:OFF")
                latencies.append(elapsed)
                responses.append(response)
            state_text, state, _state_elapsed, state_attempts = self._query_scpi_state(client)
        passed = all(response == "OK" for response in responses) and len(state) == 8 and all(item["mask"] == "0000" for item in state.values())
        return ("PASS" if passed else "FAIL", f"responses={sorted(set(responses))}",
                latency_metrics(latencies, "all_off"),
                {"state_attempts": state_attempts, "raw_state_excerpt": response_excerpt(state_text)})

    def _hil_skip_unless_ready(self) -> tuple[str, str, dict, dict] | None:
        if not self.hil_ready or self.dmm is None:
            return "SKIP", "HIL fixture was not initialized or failed its safety precheck", {}, {}
        return None

    def _hil_fetch_calibration(self) -> dict[int, float]:
        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 5.0), self.logger) as client:
            response, _ = client.query(
                f"CAL:RES? CH{self.config.hil_channel}",
                response_timeout_s=max(self.config.timeout_s, 5.0),
            )
        parsed = parse_calibration_compact(response)
        rows = parsed.get(self.config.hil_channel, [])
        values = {int(row["bit"]): float(row["resistance_ohm"]) for row in rows}
        if sorted(values) != list(range(16)):
            raise RuntimeError(
                f"CH{self.config.hil_channel} calibration does not contain bits 0..15"
            )
        invalid = {bit: value for bit, value in values.items() if not math.isfinite(value) or value <= 0.0}
        if invalid:
            raise RuntimeError(f"CH{self.config.hil_channel} has invalid calibration values: {invalid}")
        return values

    def _hil_all_off_verified(self) -> tuple[float, float]:
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            response, apply_ms = client.query("ALL:OFF")
            state_text, state, state_ms, _attempts = self._query_scpi_state(client)
        if response != "OK":
            raise RuntimeError(f"ALL:OFF returned {response!r}")
        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            raise RuntimeError(
                f"ALL:OFF verification failed: parsed={state}; raw={response_excerpt(state_text)!r}"
            )
        return apply_ms, state_ms

    def _hil_apply_mask_verified(self, mask: int) -> tuple[float, float]:
        mask_text = f"{mask:04X}"
        channel = self.config.hil_channel
        if self.serial_monitor is not None:
            self.serial_monitor.marker(f"APPLY CH{channel} MASK {mask_text}")
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            response, apply_ms = client.query(f"CH{channel}:MASK {mask_text}")
            state_text, state, verify_ms, _attempts = self._query_scpi_state(client)
        if response != "OK":
            raise RuntimeError(f"CH{channel}:MASK {mask_text} returned {response!r}")
        if len(state) != 8:
            raise RuntimeError(
                f"Incomplete state after mask {mask_text}: parsed={state}; "
                f"raw={response_excerpt(state_text)!r}"
            )
        mismatches = []
        for current_channel in range(1, 9):
            expected = mask_text if current_channel == channel else "0000"
            actual = state.get(current_channel, {}).get("mask")
            if actual != expected:
                mismatches.append({"channel": current_channel, "expected": expected, "actual": actual})
        if mismatches:
            raise RuntimeError(f"Unexpected channel masks after {mask_text}: {mismatches}")
        return apply_ms, verify_ms

    def _hil_measure_mask(self, mask: int, note: str = "") -> HilMeasurement:
        if self.dmm is None:
            raise RuntimeError("DMM is not initialized")
        expected = equivalent_resistance_ohm(mask, self.hil_calibration)
        start = time.perf_counter_ns()
        apply_ms, verify_ms = self._hil_apply_mask_verified(mask)
        measured, samples, stdev, relative_stdev, read_ms, stable = wait_for_stable_resistance(
            self.dmm,
            window=self.config.hil_sample_count,
            interval_s=self.config.hil_sample_interval_s,
            timeout_s=self.config.hil_settle_timeout_s,
            relative_stdev_limit_percent=self.config.hil_stability_percent,
            minimum_wait_s=self.config.hil_minimum_wait_s,
        )
        settle_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        finite_samples = [value for value in samples if math.isfinite(value)]
        selected = finite_samples[-self.config.hil_sample_count:] if finite_samples else []
        sample_mean = statistics.fmean(selected) if selected else math.inf
        delta = measured - expected if math.isfinite(measured) else math.inf
        percent = error_percent(measured, expected)
        passed = stable and math.isfinite(measured) and abs(percent) <= self.config.hil_error_limit_percent
        result = "PASS" if passed else "FAIL"
        measurement = HilMeasurement(
            channel=self.config.hil_channel,
            mask=f"{mask:04X}",
            active_bits=mask.bit_count(),
            expected_ohm=expected,
            measured_ohm=measured,
            error_ohm=delta,
            error_percent=percent,
            sample_count=len(samples),
            sample_mean_ohm=sample_mean,
            sample_stdev_ohm=stdev,
            sample_rel_stdev_percent=relative_stdev,
            stable=stable,
            settle_ms=settle_ms,
            scpi_apply_ms=apply_ms,
            scpi_verify_ms=verify_ms,
            dmm_read_ms=read_ms,
            result=result,
            note=note,
        )
        self.hil_measurements.append(measurement)
        write_hil_measurements(Path(self.config.output_dir) / "hil_measurements.csv", self.hil_measurements)
        self.logger.event("hil_measurement", **measurement.to_dict())
        return measurement

    @staticmethod
    def _measurement_metrics(measurements: list[HilMeasurement], prefix: str) -> dict[str, float | int]:
        if not measurements:
            return {f"{prefix}_count": 0}
        absolute_errors = [abs(item.error_percent) for item in measurements if math.isfinite(item.error_percent)]
        settle = [item.settle_ms for item in measurements]
        apply_values = [item.scpi_apply_ms for item in measurements]
        metrics: dict[str, float | int] = {
            f"{prefix}_count": len(measurements),
            f"{prefix}_pass_count": sum(item.result == "PASS" for item in measurements),
            f"{prefix}_stable_count": sum(item.stable for item in measurements),
            f"{prefix}_max_abs_error_percent": max(absolute_errors) if absolute_errors else math.inf,
            f"{prefix}_avg_abs_error_percent": statistics.fmean(absolute_errors) if absolute_errors else math.inf,
            f"{prefix}_max_settle_ms": max(settle),
        }
        metrics.update(latency_metrics(settle, f"{prefix}_settle"))
        metrics.update(latency_metrics(apply_values, f"{prefix}_apply"))
        return metrics

    def test_hil_fixture_discovery(self):
        # No active output is allowed until the selected gate matches a known
        # firmware version and the board state, COM port, DMM, and calibration
        # have all been identified successfully.
        if not self.gate_compatible:
            return (
                "FAIL",
                f"Selected gate {self.config.gate} does not match firmware "
                f"{self.detected_firmware_version}",
                {},
                {
                    "selected_gate": self.config.gate,
                    "firmware_version": self.detected_firmware_version,
                },
            )
        self._hil_all_off_verified()
        self.hil_start_timestamp = time.time()
        self.serial_monitor = SerialMonitor(
            self.config.serial_port,
            self.config.serial_baud,
            self.logger,
            Path(self.config.output_dir) / "serial_console.log",
            self.config.serial_match,
        )
        self.serial_monitor.start()
        # Opening a USB CDC port can affect some boards; force and verify OFF again.
        time.sleep(0.25)
        self._hil_all_off_verified()

        self.dmm = VisaDmm(
            self.config.dmm_resource,
            self.logger,
            Path(self.config.output_dir) / "dmm_transcript.log",
            idn_contains=self.config.dmm_idn_contains,
            init_commands=self.config.dmm_init_commands,
            measure_command=self.config.dmm_measure_command,
            timeout_s=max(self.config.timeout_s, self.config.hil_settle_timeout_s),
            backend=self.config.dmm_backend,
        )
        self.dmm.open()
        self.hil_calibration = self._hil_fetch_calibration()
        self.hil_ready = True
        details = {
            "serial_port": self.serial_monitor.port,
            "serial_baud": self.config.serial_baud,
            "dmm_resource": self.dmm.resource,
            "dmm_identity": self.dmm.identity,
            "channel": self.config.hil_channel,
            "calibration": self.hil_calibration,
        }
        Path(self.config.output_dir, "hardware_manifest.json").write_text(
            json.dumps(details, indent=2, sort_keys=True), encoding="utf-8"
        )
        return "PASS", (
            f"COM={self.serial_monitor.port}; DMM={self.dmm.identity}; CH{self.config.hil_channel}"
        ), {"hil_calibration_points": len(self.hil_calibration)}, details

    def test_hil_safe_state_precheck(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        assert self.dmm is not None
        all_off_ms, verify_ms = self._hil_all_off_verified()
        samples = []
        read_ms = 0.0
        for _ in range(max(2, min(3, self.config.hil_sample_count))):
            value, elapsed = self.dmm.read_resistance()
            samples.append(value)
            read_ms += elapsed
            if self.config.hil_sample_interval_s:
                time.sleep(self.config.hil_sample_interval_s)
        finite = [value for value in samples if math.isfinite(value)]
        minimum = min(finite) if finite else math.inf
        passed = minimum >= self.config.hil_off_min_ohm
        if not passed:
            self.hil_ready = False
        return (
            "PASS" if passed else "FAIL",
            f"OFF resistance minimum={minimum:.6g} ohm; required >= {self.config.hil_off_min_ohm:.6g}",
            {
                "hil_off_precheck_min_ohm": minimum,
                "hil_off_precheck_dmm_ms": read_ms,
                "hil_all_off_ms": all_off_ms,
                "hil_all_off_verify_ms": verify_ms,
            },
            {"samples": samples},
        )

    def test_hil_single_bit_walk(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        bits = parse_bits_spec(self.config.hil_bits)
        measurements: list[HilMeasurement] = []
        failures: list[str] = []
        for bit in bits:
            mask = 1 << bit
            try:
                measurement = self._hil_measure_mask(mask, note=f"single bit {bit}")
                measurements.append(measurement)
                if measurement.result != "PASS":
                    failures.append(
                        f"bit {bit}: error={measurement.error_percent:.4f}%, "
                        f"stable={measurement.stable}"
                    )
            except Exception as exc:
                failures.append(f"bit {bit}: {type(exc).__name__}: {exc}")
                self.logger.log.exception("HIL single-bit test failed for bit %d", bit)
            finally:
                try:
                    self._hil_all_off_verified()
                except Exception as cleanup_exc:
                    failures.append(f"bit {bit} cleanup: {cleanup_exc}")
                    self.hil_ready = False
                    break
        metrics = self._measurement_metrics(measurements, "hil_single_bit")
        return (
            "PASS" if not failures and len(measurements) == len(bits) else "FAIL",
            f"{len(measurements)}/{len(bits)} measured; failures={len(failures)}",
            metrics,
            {"bits": bits, "failures": failures, "measurements": [item.to_dict() for item in measurements]},
        )

    def test_hil_combination_masks(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        masks = parse_masks_spec(self.config.hil_combination_masks)
        if not masks:
            return "SKIP", "no combination masks configured", {}, {}
        measurements: list[HilMeasurement] = []
        failures: list[str] = []
        for mask in masks:
            try:
                measurement = self._hil_measure_mask(mask, note="combination mask")
                measurements.append(measurement)
                if measurement.result != "PASS":
                    failures.append(
                        f"{mask:04X}: error={measurement.error_percent:.4f}%, stable={measurement.stable}"
                    )
            except Exception as exc:
                failures.append(f"{mask:04X}: {type(exc).__name__}: {exc}")
                self.logger.log.exception("HIL combination test failed for mask %04X", mask)
            finally:
                try:
                    self._hil_all_off_verified()
                except Exception as cleanup_exc:
                    failures.append(f"{mask:04X} cleanup: {cleanup_exc}")
                    self.hil_ready = False
                    break
        metrics = self._measurement_metrics(measurements, "hil_combination")
        return (
            "PASS" if not failures and len(measurements) == len(masks) else "FAIL",
            f"{len(measurements)}/{len(masks)} measured; failures={len(failures)}",
            metrics,
            {"masks": [f"{mask:04X}" for mask in masks], "failures": failures,
             "measurements": [item.to_dict() for item in measurements]},
        )

    def test_hil_repeated_switching(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        selected_bit = parse_bits_spec(self.config.hil_bits)[0]
        mask = 1 << selected_bit
        measurements: list[HilMeasurement] = []
        failures: list[str] = []
        off_latencies: list[float] = []
        for cycle in range(1, self.config.hil_repeat_cycles + 1):
            try:
                measurement = self._hil_measure_mask(mask, note=f"repeat cycle {cycle}")
                measurements.append(measurement)
                if measurement.result != "PASS":
                    failures.append(
                        f"cycle {cycle}: error={measurement.error_percent:.4f}%, stable={measurement.stable}"
                    )
                off_ms, _ = self._hil_all_off_verified()
                off_latencies.append(off_ms)
            except Exception as exc:
                failures.append(f"cycle {cycle}: {type(exc).__name__}: {exc}")
                self.logger.log.exception("HIL repeat cycle %d failed", cycle)
                try:
                    self._hil_all_off_verified()
                except Exception as cleanup_exc:
                    failures.append(f"cycle {cycle} cleanup: {cleanup_exc}")
                    self.hil_ready = False
                    break
        measured_values = [item.measured_ohm for item in measurements if math.isfinite(item.measured_ohm)]
        repeatability_percent = math.inf
        if len(measured_values) >= 2:
            mean = statistics.fmean(measured_values)
            repeatability_percent = statistics.stdev(measured_values) / mean * 100.0 if mean else math.inf
        metrics = self._measurement_metrics(measurements, "hil_repeat")
        metrics["hil_repeat_repeatability_stdev_percent"] = repeatability_percent
        metrics.update(latency_metrics(off_latencies, "hil_repeat_all_off"))
        return (
            "PASS" if not failures and len(measurements) == self.config.hil_repeat_cycles else "FAIL",
            f"{len(measurements)}/{self.config.hil_repeat_cycles} cycles; failures={len(failures)}",
            metrics,
            {"bit": selected_bit, "mask": f"{mask:04X}", "failures": failures,
             "measurements": [item.to_dict() for item in measurements]},
        )

    def test_hil_serial_health(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        if self.serial_monitor is None:
            return "FAIL", "serial monitor not available", {}, {}
        lines = self.serial_monitor.lines_since(self.hil_start_timestamp)
        matches: list[dict[str, str]] = []
        for line in lines:
            lower = line.lower()
            for pattern in self.config.hil_serial_fault_patterns:
                if pattern.lower() in lower:
                    # Do not flag explicit zero-valued status counters.
                    if any(token in lower for token in ("=0", ": 0", "count 0")):
                        continue
                    matches.append({"pattern": pattern, "line": line})
                    break
        if self.serial_monitor.exception is not None:
            matches.append({"pattern": "serial_monitor_exception", "line": repr(self.serial_monitor.exception)})
        structured_events = [line for line in lines if line.startswith("EVT ") and "core=1" in line]
        requires_events = self.config.gate != "G0"
        passed = not matches and (not requires_events or bool(structured_events))
        return (
            "PASS" if passed else "FAIL",
            f"{len(lines)} serial lines; structured_events={len(structured_events)}; fault matches={len(matches)}",
            {
                "hil_serial_line_count": len(lines),
                "hil_serial_structured_event_count": len(structured_events),
                "hil_serial_fault_match_count": len(matches),
            },
            {"matches": matches[:100], "structured_events": structured_events[:100]},
        )

    def test_hil_serial_diagnostic(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        if int(self.config.gate[1:]) < 2:
            return "SKIP", "SYST:DIAG:SERIAL? is introduced at Gate 2", {}, {}
        if self.serial_monitor is None:
            return "FAIL", "serial monitor not available", {}, {}

        started = time.time()
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            response, elapsed_ms = client.query("SYST:DIAG:SERIAL?")
        deadline = time.monotonic() + 3.0
        matched: list[str] = []
        while time.monotonic() < deadline:
            matched = [
                line for line in self.serial_monitor.lines_since(started)
                if line.startswith("EVT ") and "code=SERIAL_TEST" in line and "core=0" in line
            ]
            if matched:
                break
            time.sleep(0.05)
        passed = response.strip() == "OK,SERIAL_TEST" and bool(matched)
        return (
            "PASS" if passed else "FAIL",
            f"response={response!r}; captured_events={len(matched)}",
            {"hil_serial_test_scpi_ms": elapsed_ms, "hil_serial_test_event_count": len(matched)},
            {"matched_events": matched[:20]},
        )

    def test_hil_final_all_off(self):
        skipped = self._hil_skip_unless_ready()
        if skipped:
            return skipped
        assert self.dmm is not None
        all_off_ms, verify_ms = self._hil_all_off_verified()
        samples = []
        read_ms = 0.0
        for _ in range(max(3, self.config.hil_sample_count)):
            value, elapsed = self.dmm.read_resistance()
            samples.append(value)
            read_ms += elapsed
            if self.config.hil_sample_interval_s:
                time.sleep(self.config.hil_sample_interval_s)
        finite = [value for value in samples if math.isfinite(value)]
        minimum = min(finite) if finite else math.inf
        passed = minimum >= self.config.hil_off_min_ohm
        return (
            "PASS" if passed else "FAIL",
            f"final OFF resistance minimum={minimum:.6g} ohm",
            {
                "hil_final_off_min_ohm": minimum,
                "hil_final_off_dmm_ms": read_ms,
                "hil_final_all_off_ms": all_off_ms,
                "hil_final_all_off_verify_ms": verify_ms,
            },
            {"samples": samples},
        )

    def best_effort_all_off(self) -> None:
        try:
            with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
                response, _ = client.query("ALL:OFF")
                self.logger.log.info("Cleanup ALL:OFF response: %s", response)
        except Exception as exc:
            self.logger.log.error("Cleanup ALL:OFF failed: %s", exc)
