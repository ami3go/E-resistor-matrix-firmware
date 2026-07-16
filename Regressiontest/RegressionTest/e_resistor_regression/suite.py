from __future__ import annotations

import concurrent.futures
import json
import math
import socket
import statistics
import time
import traceback
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
from .parsers import latency_metrics, parse_calibration_compact, parse_http_state, parse_scpi_state


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
        self.hil_measurements: list[HilMeasurement] = []

    def add(self, test_id: str, name: str, function: Callable[[], tuple[str, str, dict, dict]]) -> None:
        start = time.perf_counter_ns()
        try:
            status, message, metrics, details = function()
        except Exception as exc:  # intentionally captures a test failure without aborting the suite
            status = "FAIL"
            message = f"{type(exc).__name__}: {exc}"
            metrics = {}
            details = {"exception_type": type(exc).__name__, "exception": repr(exc), "traceback": traceback.format_exc()}
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

    def _query_scpi_state(
        self,
        client: ScpiClient | None = None,
        attempts: int = 3,
    ) -> tuple[str, dict[int, dict[str, str]], float]:
        """Read and validate a complete eight-channel SCPI state response.

        A retry reconnects the existing client because an empty read normally
        means the peer closed the connection.  Read-only STATE? retries are
        safe and keep transient TCP fragmentation from becoming false hardware
        failures.
        """
        errors: list[str] = []
        total_elapsed_ms = 0.0
        last_text = ""
        last_state: dict[int, dict[str, str]] = {}
        max_attempts = max(1, attempts)
        for attempt in range(1, max_attempts + 1):
            try:
                if client is None:
                    with ScpiClient(
                        self.config.host,
                        self.config.scpi_port,
                        self.config.timeout_s,
                        self.logger,
                    ) as temporary_client:
                        last_text, elapsed = temporary_client.query("STATE?")
                else:
                    if attempt > 1:
                        client.connect()
                    last_text, elapsed = client.query("STATE?")
                total_elapsed_ms += elapsed
                last_state = parse_scpi_state(last_text)
                if len(last_state) == 8:
                    if attempt > 1:
                        self.logger.event(
                            "scpi_state_retry_recovered",
                            attempts=attempt,
                            raw_state=last_text,
                        )
                    return last_text, last_state, total_elapsed_ms
                errors.append(
                    f"attempt {attempt}: parsed={len(last_state)}, raw={last_text!r}"
                )
            except Exception as exc:
                errors.append(f"attempt {attempt}: {type(exc).__name__}: {exc}")
            if attempt < max_attempts:
                time.sleep(0.05)
        raise AssertionError(
            "Incomplete SCPI state after "
            f"{max_attempts} attempt(s): {'; '.join(errors)}"
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
                    self.add("HIL-007", "Final all-off isolation measurement", self.test_hil_final_all_off)
            elif self.config.profile not in {"read_only", "safe_output"}:
                self.results.append(TestResult(
                    "PROFILE-001", "Requested profile", "SKIP", 0.0,
                    f"Profile {self.config.profile!r} is reserved for a dedicated fixture extension.",
                ))
        finally:
            cleanup_required = (self.config.profile == "safe_output" and self.config.allow_output_tests) or (
                self.config.profile == "hil_single_channel" and self.config.allow_active_output_tests
            )
            if cleanup_required:
                cleanup_ok, cleanup_details = self.best_effort_all_off()
                cleanup_result = TestResult(
                    "SAFE-999",
                    "Verified final all-off cleanup",
                    "PASS" if cleanup_ok else "FAIL",
                    float(cleanup_details.get("duration_ms", 0.0)),
                    cleanup_details.get("message", ""),
                    {
                        "cleanup_attempts": int(cleanup_details.get("attempts", 0)),
                        "cleanup_duration_ms": float(cleanup_details.get("duration_ms", 0.0)),
                    },
                    cleanup_details,
                )
                self.results.append(cleanup_result)
                self.logger.event("test_result", **cleanup_result.to_dict())

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
        missing = [key for key in required if key not in state]
        channel_count = len(state.get("channels", {}))
        readiness_failures = []
        if state.get("littlefs") != "ready": readiness_failures.append("LittleFS not ready")
        if state.get("shift_registers_ready") != "1": readiness_failures.append("shift registers not ready")
        if state.get("fatal_safe_state") != "0": readiness_failures.append("fatal safe state active")
        if state.get("core1_state") != "ready": readiness_failures.append("Core 1 not ready")
        passed = not missing and channel_count == 8 and not readiness_failures
        details = {
            "missing": missing,
            "channel_count": channel_count,
            "readiness_failures": readiness_failures,
            "parsed_channels": state.get("channels", {}),
            "raw_channel_lines": [
                line.strip() for line in text.splitlines()
                if line.strip().lower().startswith("ch")
            ],
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
            state_text, scpi_state, state_ms = self._query_scpi_state(client)
        mismatches = []
        for ch in range(1, 9):
            http_mask = http_state.get("channels", {}).get(ch, {}).get("mask")
            scpi_mask = scpi_state.get(ch, {}).get("mask")
            if http_mask != scpi_mask:
                mismatches.append({"channel": ch, "http": http_mask, "scpi": scpi_mask})
        passed = len(scpi_state) == 8 and not mismatches and "core1_ready=" in status
        message = (
            "state consistent" if passed
            else f"channel_count={len(scpi_state)}, mismatches={mismatches}"
        )
        return ("PASS" if passed else "FAIL", message,
                {"scpi_status_ms": status_ms, "scpi_state_ms": state_ms},
                {
                    "status": status,
                    "channel_count": len(scpi_state),
                    "parsed_state": scpi_state,
                    "raw_state": state_text,
                    "mismatches": mismatches,
                })

    def test_calibration(self):
        with ScpiClient(self.config.host, self.config.scpi_port, max(self.config.timeout_s, 5.0), self.logger) as client:
            response, elapsed = client.query("CAL:RES?", response_timeout_s=max(self.config.timeout_s, 5.0))
        parsed = parse_calibration_compact(response)
        failures = []
        for ch in range(1, 9):
            rows = parsed.get(ch, [])
            if len(rows) != 16:
                failures.append(f"CH{ch} has {len(rows)} rows")
                continue
            bits = sorted(row["bit"] for row in rows)
            if bits != list(range(16)):
                failures.append(f"CH{ch} bit set invalid: {bits}")
        passed = not failures and len(parsed) == 8
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
        # v0.4.4 may emit more than one error; recovery is the essential requirement.
        passed = "E-Resistor" in identity
        return ("PASS" if passed else "FAIL", f"recovered={passed}, overflow={overflow_response[:120]!r}",
                {"post_overflow_idn_ms": elapsed}, {})

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
                    _response, _parsed, elapsed = self._query_scpi_state(client)
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
        passed = response.status == 200 and ("text" in content_type.lower() or len(response.body) >= 0)
        return ("PASS" if passed else "FAIL", f"HTTP {response.status}, content-type={content_type}",
                {"log_download_bytes": len(response.body), "log_download_ms": response.elapsed_ms}, {})

    def test_zero_mask_command_path(self):
        if not self.config.allow_output_tests:
            return "SKIP", "requires --allow-output-tests", {}, {}
        masks = ",".join(["0000"] * 8)
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            response, elapsed = client.query(f"ROUT:ALL:MASK {masks}")
            state_text, state, state_elapsed = self._query_scpi_state(client)
        passed = response == "OK" and len(state) == 8 and all(item["mask"] == "0000" for item in state.values())
        return ("PASS" if passed else "FAIL", f"apply={response!r}, channels={len(state)}",
                {"zero_profile_apply_ms": elapsed, "zero_profile_verify_ms": state_elapsed},
                {"raw_state": state_text, "parsed_state": state})

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
            state_text, state, _ = self._query_scpi_state(client)
        passed = all(response == "OK" for response in responses) and len(state) == 8 and all(item["mask"] == "0000" for item in state.values())
        return ("PASS" if passed else "FAIL",
                f"responses={sorted(set(responses))}, channels={len(state)}",
                latency_metrics(latencies, "all_off"),
                {"raw_state": state_text, "parsed_state": state})

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
            state_text, state, state_ms = self._query_scpi_state(client)
        if response != "OK":
            raise RuntimeError(f"ALL:OFF returned {response!r}")
        if len(state) != 8 or any(item.get("mask") != "0000" for item in state.values()):
            raise RuntimeError(f"ALL:OFF verification failed: {state}")
        return apply_ms, state_ms

    def _hil_verify_mask_state(self, mask: int) -> float:
        mask_text = f"{mask:04X}"
        channel = self.config.hil_channel
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            state_text, state, verify_ms = self._query_scpi_state(client)
        if len(state) != 8:
            raise RuntimeError(f"Incomplete state while verifying mask {mask_text}: {state}")
        mismatches = []
        for current_channel in range(1, 9):
            expected = mask_text if current_channel == channel else "0000"
            actual = state.get(current_channel, {}).get("mask")
            if actual != expected:
                mismatches.append({"channel": current_channel, "expected": expected, "actual": actual})
        if mismatches:
            raise RuntimeError(f"Unexpected channel masks while verifying {mask_text}: {mismatches}")
        return verify_ms

    def _hil_apply_mask_verified(self, mask: int) -> tuple[float, float]:
        mask_text = f"{mask:04X}"
        channel = self.config.hil_channel
        if self.serial_monitor is not None:
            self.serial_monitor.marker(f"APPLY CH{channel} MASK {mask_text}")
        with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
            response, apply_ms = client.query(f"CH{channel}:MASK {mask_text}")
        if response != "OK":
            raise RuntimeError(f"CH{channel}:MASK {mask_text} returned {response!r}")
        verify_ms = self._hil_verify_mask_state(mask)
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
        post_verify_ms = self._hil_verify_mask_state(mask)
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
            scpi_post_verify_ms=post_verify_ms,
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
        # No active output is allowed until the board state, COM port, DMM, and
        # selected-channel calibration have all been identified successfully.
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
        return (
            "PASS" if not matches else "FAIL",
            f"{len(lines)} serial lines; fault matches={len(matches)}",
            {"hil_serial_line_count": len(lines), "hil_serial_fault_match_count": len(matches)},
            {"matches": matches[:100]},
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

    def best_effort_all_off(self, attempts: int = 3) -> tuple[bool, dict]:
        """Force all outputs OFF and verify all eight SCPI masks are zero.

        Cleanup retries because it often runs immediately after a protocol or
        hardware failure. The method never raises, preserving the original test
        failure while still producing explicit cleanup evidence.
        """
        started = time.perf_counter_ns()
        errors: list[str] = []
        last_response = ""
        last_state: dict = {}
        last_state_text = ""
        used_attempts = 0
        for attempt in range(1, max(1, attempts) + 1):
            used_attempts = attempt
            try:
                with ScpiClient(self.config.host, self.config.scpi_port, self.config.timeout_s, self.logger) as client:
                    last_response, _ = client.query("ALL:OFF")
                    state_text, last_state, _ = self._query_scpi_state(client)
                last_state_text = state_text
                verified = (
                    last_response == "OK"
                    and len(last_state) == 8
                    and all(item.get("mask") == "0000" for item in last_state.values())
                )
                self.logger.event(
                    "cleanup_all_off_attempt", attempt=attempt, response=last_response,
                    verified=verified, state=last_state, raw_state=last_state_text,
                )
                if verified:
                    duration_ms = (time.perf_counter_ns() - started) / 1_000_000.0
                    message = f"ALL:OFF verified on attempt {attempt}"
                    self.logger.log.info(message)
                    return True, {
                        "message": message,
                        "attempts": used_attempts,
                        "duration_ms": duration_ms,
                        "response": last_response,
                        "state": last_state,
                        "raw_state": last_state_text,
                        "errors": errors,
                    }
                errors.append(f"attempt {attempt}: response={last_response!r}, state={last_state!r}")
            except Exception as exc:
                errors.append(f"attempt {attempt}: {type(exc).__name__}: {exc}")
                self.logger.log.error("Cleanup ALL:OFF attempt %d failed: %s", attempt, exc)
            if attempt < max(1, attempts):
                time.sleep(0.20)

        duration_ms = (time.perf_counter_ns() - started) / 1_000_000.0
        message = f"Unable to verify ALL:OFF after {used_attempts} attempt(s)"
        self.logger.log.critical("%s: %s", message, errors)
        return False, {
            "message": message,
            "attempts": used_attempts,
            "duration_ms": duration_ms,
            "response": last_response,
            "state": last_state,
            "raw_state": last_state_text,
            "errors": errors,
        }
