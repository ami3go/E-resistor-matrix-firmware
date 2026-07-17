from __future__ import annotations

import concurrent.futures
import json
import math
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
