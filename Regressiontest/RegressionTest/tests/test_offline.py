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


class ParserTests(unittest.TestCase):
    def test_http_state_accepts_prefixed_and_bare_masks(self) -> None:
        parsed = parse_http_state(
            "firmware_version=0.6.0\n"
            "core_snapshot_sequence=12\n"
            "core_snapshot_generation=3\n"
            "ch1=0x0000 resistance=OPEN count=1\n"
            "ch8=00FF resistance=123.000 Ohm count=7\n"
        )
        self.assertEqual(parsed["firmware_version"], "0.6.0")
        self.assertEqual(parsed["channels"][1]["mask"], "0000")
        self.assertEqual(parsed["channels"][8]["mask"], "00FF")
        self.assertEqual(parsed["channels"][8]["count"], 7)

    def test_scpi_state_accepts_prefixed_and_bare_masks(self) -> None:
        parsed = parse_scpi_state("CH1=0x0000,OPEN; CH2=0001,626.000 Ohm")
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

    def test_gate3_transport_key_values(self) -> None:
        parsed = parse_key_value_response(
            "generation=3,last_submitted=50,last_completed=50,command_overflows=0,"
            "result_overflows=0,timeouts=0,expired=0,generation_rejects=0,"
            "invalid_commands=0,policy_installs=1,core0_failsafe=0"
        )
        self.assertEqual(int(parsed["generation"]), 3)
        self.assertEqual(int(parsed["last_completed"]), 50)

    def test_gate3_snapshot_key_values(self) -> None:
        parsed = parse_key_value_response(
            "snapshot_sequence=10,last_command_sequence=7,generation=2,flags=11,"
            "ch1_mask=0000,ch8_mask=0000"
        )
        self.assertEqual(parsed["flags"], "11")
        self.assertEqual(parsed["ch8_mask"], "0000")

    def test_latency_metrics(self) -> None:
        metrics = latency_metrics([1.0, 2.0, 3.0, 4.0], "x")
        self.assertEqual(metrics["x_count"], 4)
        self.assertAlmostEqual(metrics["x_avg_ms"], 2.5)


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


class CoverageTests(unittest.TestCase):
    def test_gate3_coverage_items_exist(self) -> None:
        from e_resistor_regression.coverage import evaluate_coverage
        summary = {
            "config": {"gate": "G3"},
            "results": [
                {"test_id": "G3-001", "status": "PASS"},
                {"test_id": "G3-002", "status": "PASS"},
                {"test_id": "G3-003", "status": "PASS"},
            ],
        }
        rows = {row["coverage_id"]: row for row in evaluate_coverage(summary)}
        self.assertEqual(rows["COV-043"]["run_status"], "PASS")
        self.assertEqual(rows["COV-044"]["run_status"], "PASS")
        self.assertEqual(rows["COV-045"]["run_status"], "PASS")
        self.assertEqual(rows["COV-046"]["run_status"], "NOT_RUN")
        self.assertEqual(rows["COV-047"]["run_status"], "NOT_RUN")


class StateRetryTests(unittest.TestCase):
    def test_scpi_state_retry_accepts_second_complete_response(self) -> None:
        from e_resistor_regression.logging_ext import ExtendedLogger
        from e_resistor_regression.models import RunConfig
        from e_resistor_regression.suite import RegressionSuite

        class FakeClient:
            def __init__(self) -> None:
                self.calls = 0
            def query(self, command: str):
                self.calls += 1
                if self.calls == 1:
                    return "CH1=0x0000,OPEN", 1.0
                return ";".join(f"CH{i}=0x0000,OPEN" for i in range(1, 9)), 2.0
            def close(self): pass
            def connect(self): pass

        with tempfile.TemporaryDirectory() as tmp:
            suite = RegressionSuite(RunConfig(host="127.0.0.1", gate="G3"), ExtendedLogger(Path(tmp)))
            raw, parsed, elapsed, attempts = suite._query_scpi_state(FakeClient())
            self.assertEqual(len(parsed), 8)
            self.assertIn("CH8", raw)
            self.assertEqual(attempts, 2)
            self.assertEqual(elapsed, 3.0)

    def test_known_firmware_gate_mapping(self) -> None:
        from e_resistor_regression.suite import RegressionSuite
        self.assertEqual(RegressionSuite._expected_gate_for_firmware("0.6.0"), "G3")
        self.assertEqual(RegressionSuite._expected_gate_for_firmware("0.6.1"), "G3")
        self.assertEqual(RegressionSuite._expected_gate_for_firmware("0.7.0"), "G4")
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
            self.assertTrue(verify_evidence_manifest(root)["verified"])


if __name__ == "__main__":
    unittest.main()
