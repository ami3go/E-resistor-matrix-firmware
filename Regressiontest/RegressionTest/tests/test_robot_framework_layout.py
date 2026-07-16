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
            RF / "suites" / "source_build.robot",
            ROOT / "requirements-robot.txt",
            ROOT / "setup_robot_environment.bat",
            ROOT / "run_robot_read_only.bat",
            ROOT / "run_robot_safe_output.bat",
            ROOT / "run_robot_hil_single_channel.bat",
            ROOT / "run_robot_source_build.bat",
            ROOT / "run_robot_all_safe.bat",
            ROOT / "run_robot_custom.bat",
            ROOT / "validate_robot_suites.bat",
            ROOT / "generate_robot_keyword_docs.bat",
            RF / "variables" / "bench_config.example.bat",
            RF / "scripts" / "_windows_common.bat",
        ]
        missing = [str(path.relative_to(ROOT)) for path in required if not path.exists()]
        self.assertEqual(missing, [])

    def test_runtime_ids_are_mapped(self) -> None:
        expected = {
            "NET-001", "HTTP-001", "HTTP-002", "HTTP-003",
            "SCPI-001", "SCPI-002", "SCPI-003", "SCPI-004", "SCPI-005",
            "PERF-001", "PERF-002", "MEM-001", "STRESS-001",
            "FILES-001", "LOG-001",
        }
        text = (RF / "suites" / "read_only.robot").read_text(encoding="utf-8")
        self.assertTrue(all(test_id in text for test_id in expected))

    def test_hil_ids_are_mapped(self) -> None:
        text = (RF / "suites" / "hil_single_channel.robot").read_text(encoding="utf-8")
        self.assertTrue(all(f"HIL-{index:03d}" in text for index in range(1, 8)))

    def test_launcher_imports_without_running(self) -> None:
        path = RF / "run_robot.py"
        spec = importlib.util.spec_from_file_location("e_resistor_robot_launcher", path)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader if spec else None)
        module = importlib.util.module_from_spec(spec)
        assert spec and spec.loader
        spec.loader.exec_module(module)
        args = module.parse_args(["--profile", "read_only"])
        self.assertEqual(args.profile, "read_only")

    def test_windows_bat_safety_guards(self) -> None:
        hil = (ROOT / "run_robot_hil_single_channel.bat").read_text(encoding="utf-8")
        self.assertIn("ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS", hil)
        self.assertIn("E_RESISTOR_SINGLE_CHANNEL_DMM", hil)
        self.assertIn("--allow-active-output-tests", hil)

    def test_robot_library_collects_run_diagnostics(self) -> None:
        text = (RF / "libraries" / "e_resistor_robot_library.py").read_text(encoding="utf-8")
        self.assertIn("collect_diagnostics", text)
        self.assertIn("write_diagnostics", text)
        self.assertIn('profile == "hil_single_channel"', text)

    def test_windows_bat_stable_root_usage(self) -> None:
        common = (RF / "scripts" / "_windows_common.bat").read_text(encoding="utf-8")
        self.assertIn("REGRESSION_ROOT", common)
        self.assertIn("bench_config.local.bat", common)


if __name__ == "__main__":
    unittest.main()
