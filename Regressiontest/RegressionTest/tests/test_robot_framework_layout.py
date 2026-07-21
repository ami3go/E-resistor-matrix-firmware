from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RF = ROOT / "robot_framework"


class RobotFrameworkLayoutTests(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        required = [
            RF / "run_robot.py",
            RF / "libraries" / "e_resistor_robot_library.py",
            RF / "listeners" / "evidence_listener.py",
            RF / "resources" / "common.resource",
            RF / "variables" / "bench.py",
            RF / "suites" / "read_only.robot",
            RF / "suites" / "safe_output.robot",
            RF / "suites" / "hil_single_channel.robot",
            RF / "suites" / "gate3_transport_fault.robot",
            RF / "suites" / "gate4_profile.robot",
            RF / "suites" / "gate4_profile_fault.robot",
            ROOT / "requirements-robot.txt",
            ROOT / "setup_robot_environment.bat",
            ROOT / "run_robot_read_only.bat",
            ROOT / "run_robot_safe_output.bat",
            ROOT / "run_robot_hil_single_channel.bat",
            ROOT / "run_robot_gate3_transport_fault.bat",
            ROOT / "run_robot_gate4_profile.bat",
            ROOT / "run_robot_gate4_profile_fault.bat",
            ROOT / "run_robot_all_safe.bat",
            ROOT / "run_robot_custom.bat",
            ROOT / "validate_robot_suites.bat",
            ROOT / "generate_robot_keyword_docs.bat",
            RF / "variables" / "bench_config.example.bat",
            RF / "scripts" / "_windows_common.bat",
        ]
        self.assertEqual([str(p.relative_to(ROOT)) for p in required if not p.exists()], [])

    def test_gate_ids_are_mapped(self) -> None:
        text = "\n".join(p.read_text(encoding="utf-8") for p in (RF / "suites").glob("*.robot"))
        for test_id in ("G3-001", "G3-002", "G3-003", "G3-FI-001", "G3-FI-002", "G3-FI-003", "G4-001", "G4-002", "G4-003", "G4-FI-001"):
            self.assertIn(test_id, text)

    def test_runtime_ids_are_mapped(self) -> None:
        expected = {
            "NET-001", "HTTP-001", "HTTP-002", "HTTP-003",
            "SCPI-001", "SCPI-002", "SCPI-003", "SCPI-004", "SCPI-005", "SCPI-006",
            "PERF-001", "PERF-002", "MEM-001", "STRESS-001", "FILES-001", "LOG-001",
        }
        text = (RF / "suites" / "read_only.robot").read_text(encoding="utf-8")
        self.assertTrue(all(test_id in text for test_id in expected))

    def test_hil_ids_are_mapped(self) -> None:
        text = (RF / "suites" / "hil_single_channel.robot").read_text(encoding="utf-8")
        self.assertTrue(all(f"HIL-{index:03d}" in text for index in range(1, 9)))

    def test_launcher_imports_without_running(self) -> None:
        path = RF / "run_robot.py"
        spec = importlib.util.spec_from_file_location("e_resistor_robot_launcher", path)
        module = importlib.util.module_from_spec(spec)
        assert spec and spec.loader
        spec.loader.exec_module(module)
        args = module.parse_args(["--profile", "read_only"])
        self.assertEqual(args.profile, "read_only")
        self.assertEqual(args.gate, "G4")
        self.assertEqual(args.hil_repeat_cycles, 50)

    def test_windows_bat_stable_root_usage(self) -> None:
        common = (RF / "scripts" / "_windows_common.bat").read_text(encoding="utf-8")
        self.assertIn("REGRESSION_ROOT", common)
        self.assertIn("bench_config.local.bat", common)


if __name__ == "__main__":
    unittest.main()
