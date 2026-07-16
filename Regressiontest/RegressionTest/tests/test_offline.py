from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from e_resistor_regression.parsers import (
    latency_metrics,
    parse_calibration_compact,
    parse_http_state,
    parse_scpi_state,
)
from e_resistor_regression.source_checks import scan_source


class ParserTests(unittest.TestCase):
    def test_http_state(self) -> None:
        parsed = parse_http_state(
            "firmware_version=0.4.4\n"
            "heap_free_bytes=12345\n\n"
            "ch1=0x0000 resistance=OPEN count=1\n"
            "ch8=00FF resistance=123.000 Ohm count=7\n"
        )
        self.assertEqual(parsed["firmware_version"], "0.4.4")
        self.assertEqual(parsed["channels"][1]["mask"], "0000")
        self.assertEqual(parsed["channels"][8]["count"], 7)

    def test_scpi_state(self) -> None:
        parsed = parse_scpi_state("CH1=0x0000,OPEN;CH2=0001,626.000 Ohm")
        self.assertEqual(parsed[1]["mask"], "0000")
        self.assertEqual(parsed[2]["mask"], "0001")

    def test_http_state_accepts_actual_firmware_v044_channel_lines(self) -> None:
        text = "\n".join(
            f"ch{channel}=0x0000 resistance=OPEN count={1023 + channel}"
            for channel in range(1, 9)
        )
        parsed = parse_http_state(text)
        self.assertEqual(len(parsed["channels"]), 8)
        self.assertTrue(all(item["mask"] == "0000" for item in parsed["channels"].values()))

    def test_scpi_state_accepts_actual_firmware_v044_response(self) -> None:
        text = ";".join(f"CH{channel}=0x0000,OPEN" for channel in range(1, 9))
        parsed = parse_scpi_state(text)
        self.assertEqual(len(parsed), 8)
        self.assertTrue(all(item["mask"] == "0000" for item in parsed.values()))

    def test_calibration(self) -> None:
        text = "CH1:" + ";".join(f"{i},Q{16-i},{1000+i}.000000" for i in range(16))
        parsed = parse_calibration_compact(text)
        self.assertEqual(len(parsed[1]), 16)
        self.assertEqual(parsed[1][0]["mosfet"], "Q16")

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
            (root / "http_handlers.cpp").write_text(
                'server.on("/set", HTTP_GET, handleSet);\n', encoding="utf-8"
            )
            metrics = scan_source(root)
            self.assertEqual(metrics["core1_serial_flush_count"], 1)
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


if __name__ == "__main__":
    unittest.main()
