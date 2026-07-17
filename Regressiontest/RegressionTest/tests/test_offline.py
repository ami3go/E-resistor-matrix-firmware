from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from e_resistor_regression.parsers import (
    latency_metrics,
    parse_calibration_compact,
    parse_http_state,
    parse_key_value_response,
    parse_scpi_state,
    response_excerpt,
)
from e_resistor_regression.source_checks import scan_source


class ParserTests(unittest.TestCase):
    def test_http_state(self) -> None:
        parsed = parse_http_state(
            "firmware_version=0.4.4\n"
            "heap_free_bytes=12345\n\n"
            "ch1=0x0000 resistance=OPEN count=1\n"
            "ch8 = 0x00FF resistance = 123.000 Ohm count = 7\n"
        )
        self.assertEqual(parsed["firmware_version"], "0.4.4")
        self.assertEqual(parsed["channels"][1]["mask"], "0000")
        self.assertEqual(parsed["channels"][8]["count"], 7)

    def test_scpi_state(self) -> None:
        parsed = parse_scpi_state("CH1=0x0000,OPEN; CH2 = 0x0001 , 626.000 Ohm")
        self.assertEqual(parsed[1]["mask"], "0000")
        self.assertEqual(parsed[2]["mask"], "0001")

    def test_response_excerpt_is_bounded(self) -> None:
        excerpt = response_excerpt("a\n  b  " + "x" * 600, limit=40)
        self.assertLessEqual(len(excerpt), 40)
        self.assertNotIn("\n", excerpt)
        self.assertTrue(excerpt.endswith("..."))

    def test_calibration(self) -> None:
        text = "CH1:" + ";".join(f"{i},Q{16-i},{1000+i}.000000" for i in range(16))
        parsed = parse_calibration_compact(text)
        self.assertEqual(len(parsed[1]), 16)
        self.assertEqual(parsed[1][0]["mosfet"], "Q16")

    def test_key_value_response(self) -> None:
        parsed = parse_key_value_response(
            "requested_ohm=1000.000000,mask=0003,calculated_ohm=998.5,candidates=2516,elapsed_us=812"
        )
        self.assertEqual(parsed["mask"], "0003")
        self.assertEqual(int(parsed["candidates"]), 2516)

    def test_latency_metrics(self) -> None:
        metrics = latency_metrics([1.0, 2.0, 3.0, 4.0], "x")
        self.assertEqual(metrics["x_count"], 4)
        self.assertAlmostEqual(metrics["x_avg_ms"], 2.5)


class SourceScanTests(unittest.TestCase):
    def test_source_scan(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "app.h").write_text("String x;\n", encoding="utf-8")
            (root / "shift_registers.cpp").write_text("Serial.flush();\n", encoding="utf-8")
            (root / "core_command.cpp").write_text("Serial.println(\"x\");\n", encoding="utf-8")
            (root / "http_handlers.cpp").write_text(
                'server.on("/set", HTTP_GET, handleSet);\n', encoding="utf-8"
            )
            metrics = scan_source(root)
            self.assertEqual(metrics["core1_serial_flush_count"], 1)
            self.assertEqual(metrics["core1_serial_call_count"], 2)
            self.assertIn("/set", metrics["mutation_get_routes"])


class HilMathTests(unittest.TestCase):
    def test_parse_bits_spec(self) -> None:
        from e_resistor_regression.hil import parse_bits_spec
        self.assertEqual(parse_bits_spec("0,2,4-6"), [0, 2, 4, 5, 6])
        self.assertEqual(parse_bits_spec("all"), list(range(16)))

    def test_parse_masks_spec(self) -> None:
        from e_resistor_regression.hil import parse_masks_spec
        self.assertEqual(parse_masks_spec("0001,0003,0001"), [0x0001, 0x0003])

    def test_equivalent_resistance(self) -> None:
        from e_resistor_regression.hil import equivalent_resistance_ohm
        table = {bit: 1000.0 for bit in range(16)}
        self.assertAlmostEqual(equivalent_resistance_ohm(0x0001, table), 1000.0)
        self.assertAlmostEqual(equivalent_resistance_ohm(0x0003, table), 500.0)

    def test_wait_for_stable_resistance(self) -> None:
        from e_resistor_regression.hil import wait_for_stable_resistance

        class FakeDmm:
            def __init__(self) -> None:
                self.values = iter([1001.0, 1000.5, 1000.2, 1000.1, 1000.0])

            def read_resistance(self):
                return next(self.values), 1.0

        median, samples, stdev, relative, read_ms, stable = wait_for_stable_resistance(
            FakeDmm(), window=3, interval_s=0.0, timeout_s=1.0,
            relative_stdev_limit_percent=0.05, minimum_wait_s=0.0,
        )
        self.assertTrue(stable)
        self.assertGreaterEqual(len(samples), 3)
        self.assertAlmostEqual(median, 1000.5, places=1)
        self.assertLessEqual(relative, 0.05)
        self.assertGreater(read_ms, 0.0)



class CoverageTests(unittest.TestCase):
    def test_coverage_pass_and_not_run(self) -> None:
        from e_resistor_regression.coverage import evaluate_coverage

        summary = {
            "config": {"gate": "G0"},
            "results": [
                {"test_id": "NET-001", "status": "PASS"},
                {"test_id": "HTTP-001", "status": "PASS"},
            ],
        }
        rows = {row["coverage_id"]: row for row in evaluate_coverage(summary)}
        self.assertEqual(rows["COV-001"]["run_status"], "PASS")
        self.assertEqual(rows["COV-002"]["run_status"], "PASS")
        self.assertEqual(rows["COV-003"]["run_status"], "NOT_RUN")

    def test_combined_coverage_requires_all_test_patterns(self) -> None:
        from e_resistor_regression.coverage import evaluate_coverage

        summary = {
            "config": {"gate": "G3"},
            "results": [
                {"test_id": "SCPI-003", "status": "PASS"},
                {"test_id": "HIL-003", "status": "PASS"},
            ],
        }
        rows = {row["coverage_id"]: row for row in evaluate_coverage(summary)}
        self.assertEqual(rows["COV-027"]["run_status"], "PARTIAL")
        self.assertEqual(rows["COV-028"]["run_status"], "PLANNED")

    def test_coverage_files_are_written(self) -> None:
        from e_resistor_regression.coverage import (
            evaluate_coverage,
            write_coverage_csv,
            write_coverage_json,
            write_coverage_markdown,
        )

        rows = evaluate_coverage({
            "config": {"gate": "G0"},
            "results": [{"test_id": "NET-001", "status": "PASS"}],
        })
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_coverage_csv(root / "coverage.csv", rows)
            write_coverage_json(root / "coverage.json", rows)
            write_coverage_markdown(root / "coverage.md", rows)
            self.assertIn("COV-001", (root / "coverage.csv").read_text(encoding="utf-8"))
            self.assertIn('"PASS"', (root / "coverage.json").read_text(encoding="utf-8"))
            self.assertIn("Test Coverage Table", (root / "coverage.md").read_text(encoding="utf-8"))



class StateRetryTests(unittest.TestCase):
    def test_scpi_state_retry_accepts_second_complete_response(self) -> None:
        from e_resistor_regression.logging_ext import ExtendedLogger
        from e_resistor_regression.models import RunConfig
        from e_resistor_regression.suite import RegressionSuite

        class FakeClient:
            def __init__(self) -> None:
                self.calls = 0
                self.closed = 0
                self.connected = 0

            def query(self, command: str):
                self.calls += 1
                if self.calls == 1:
                    return "CH1=0x0000,OPEN", 1.0
                return ";".join(f"CH{i}=0x0000,OPEN" for i in range(1, 9)), 2.0

            def close(self):
                self.closed += 1

            def connect(self):
                self.connected += 1

        with tempfile.TemporaryDirectory() as tmp:
            logger = ExtendedLogger(Path(tmp))
            suite = RegressionSuite(RunConfig(host="127.0.0.1", gate="G2"), logger)
            raw, parsed, elapsed, attempts = suite._query_scpi_state(FakeClient())
            self.assertEqual(len(parsed), 8)
            self.assertIn("CH8", raw)
            self.assertEqual(attempts, 2)
            self.assertEqual(elapsed, 3.0)

    def test_known_firmware_gate_mapping(self) -> None:
        from e_resistor_regression.suite import RegressionSuite
        self.assertEqual(RegressionSuite._expected_gate_for_firmware("0.5.0"), "G2")
        self.assertIsNone(RegressionSuite._expected_gate_for_firmware("9.9.9"))


class EvidenceTests(unittest.TestCase):
    def test_manifest_is_written_after_completion_marker(self) -> None:
        from e_resistor_regression.evidence import finalize_evidence, verify_evidence_manifest

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "RUN_INCOMPLETE").write_text("running", encoding="utf-8")
            (root / "events.jsonl").write_text('{"event":"done"}\n', encoding="utf-8")
            result = finalize_evidence(root, "run-1", "PASS")
            self.assertTrue(result["verified"])
            self.assertFalse((root / "RUN_INCOMPLETE").exists())
            self.assertTrue((root / "RUN_COMPLETE").exists())
            manifest_text = (root / "evidence_manifest.sha256").read_text(encoding="utf-8")
            self.assertIn("RUN_COMPLETE", manifest_text)
            self.assertNotIn("RUN_INCOMPLETE", manifest_text)
            self.assertTrue(verify_evidence_manifest(root)["verified"])


class LogExportValidationTests(unittest.TestCase):
    def test_log_export_rejects_empty_body(self) -> None:
        from e_resistor_regression.clients import HttpResponse
        from e_resistor_regression.logging_ext import ExtendedLogger
        from e_resistor_regression.models import RunConfig
        from e_resistor_regression.suite import RegressionSuite

        class FakeHttp:
            def request(self, path: str):
                return HttpResponse(200, {"Content-Type": "text/plain"}, b"", 1.0)

        with tempfile.TemporaryDirectory() as tmp:
            suite = RegressionSuite(RunConfig(host="127.0.0.1"), ExtendedLogger(Path(tmp)))
            suite.http = FakeHttp()
            status, _message, _metrics, details = suite.test_log_download()
            self.assertEqual(status, "FAIL")
            self.assertFalse(details["has_header"])

    def test_log_export_accepts_expected_format(self) -> None:
        from e_resistor_regression.clients import HttpResponse
        from e_resistor_regression.logging_ext import ExtendedLogger
        from e_resistor_regression.models import RunConfig
        from e_resistor_regression.suite import RegressionSuite

        body = b"E-Resistor event log\nFirmware version: 0.5.0\nEvent history\n"

        class FakeHttp:
            def request(self, path: str):
                return HttpResponse(200, {"Content-Type": "text/plain"}, body, 1.0)

        with tempfile.TemporaryDirectory() as tmp:
            suite = RegressionSuite(RunConfig(host="127.0.0.1"), ExtendedLogger(Path(tmp)))
            suite.http = FakeHttp()
            status, _message, metrics, _details = suite.test_log_download()
            self.assertEqual(status, "PASS")
            self.assertGreater(metrics["log_download_bytes"], 0)

if __name__ == "__main__":
    unittest.main()
