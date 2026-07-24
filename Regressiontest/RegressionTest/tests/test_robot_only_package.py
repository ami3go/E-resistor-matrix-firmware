from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RobotOnlyPackageTests(unittest.TestCase):
    def test_no_user_facing_python_or_source_build_launchers(self) -> None:
        forbidden = [
            *ROOT.glob("run_python_*.bat"),
            ROOT / "run_regression.py",
            ROOT / "run_robot_source_build.bat",
            ROOT / "robot_framework" / "suites" / "source_build.robot",
            ROOT / "e_resistor_regression" / "cli.py",
            ROOT / "e_resistor_regression" / "source_checks.py",
            ROOT / "e_resistor_regression" / "build_check.py",
            ROOT / "setup_python_environment.bat",
        ]
        self.assertEqual([str(p.relative_to(ROOT)) for p in forbidden if p.exists()], [])

    def test_package_versions(self) -> None:
        self.assertIn('version = "2.8.1"', (ROOT / "pyproject.toml").read_text(encoding="utf-8"))
        self.assertIn('__version__ = "2.8.1"', (ROOT / "e_resistor_regression" / "__init__.py").read_text(encoding="utf-8"))
        self.assertIn('ROBOT_LIBRARY_VERSION = "2.8.1"', (ROOT / "robot_framework" / "libraries" / "e_resistor_robot_library.py").read_text(encoding="utf-8"))

    def test_gate5_bat_defaults_and_crlf(self) -> None:
        for name in ("run_robot_read_only.bat", "run_robot_safe_output.bat", "run_robot_hil_single_channel.bat"):
            path = ROOT / name
            text = path.read_text(encoding="utf-8")
            self.assertIn('set "RUN_GATE=G5"', text, name)
        ignored_roots = {".venv", ".git", "build", "dist", "__pycache__"}
        for path in ROOT.rglob("*.bat"):
            relative = path.relative_to(ROOT)
            if any(part in ignored_roots for part in relative.parts):
                continue
            data = path.read_bytes()
            self.assertNotIn(b"\n", data.replace(b"\r\n", b""), str(relative))

    def test_windows_setup_script_paths_are_literal_and_clean(self) -> None:
        path = ROOT / "scripts" / "setup_windows_environment.bat"
        data = path.read_bytes()
        self.assertFalse(any(byte < 32 and byte not in (9, 10, 13) for byte in data))
        text = data.decode("utf-8")
        self.assertIn(r"robot_framework\variables\bench_config.example.bat", text)
        self.assertIn(r"robot_framework\variables\bench_config.local.bat", text)
        self.assertIn(r"robot_framework\ci\validate_robot_suites.py", text)

    def test_powershell_wrapper_is_robot_only(self) -> None:
        text = (ROOT / "robot_framework" / "run_robot.ps1").read_text(encoding="utf-8")
        self.assertNotIn("source_build", text)
        self.assertIn("gate3_transport_fault", text)
        self.assertIn("gate4_profile", text)
        self.assertIn("gate4_profile_fault", text)
        self.assertIn('[string]$Gate = "G5"', text)

    def test_fault_launcher_has_interlocks(self) -> None:
        text = (ROOT / "run_robot_gate3_transport_fault.bat").read_text(encoding="utf-8")
        self.assertIn("ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS", text)
        self.assertIn("E_RESISTOR_SINGLE_CHANNEL_DMM", text)
        self.assertIn("build_firmware_gate3_test.bat", text)
        self.assertIn("Reboot and flash the production image", text)

    def test_gate4_fault_launcher_has_interlocks(self) -> None:
        text = (ROOT / "run_robot_gate4_profile_fault.bat").read_text(encoding="utf-8")
        self.assertIn("ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS", text)
        self.assertIn("E_RESISTOR_SINGLE_CHANNEL_DMM", text)
        self.assertIn("build_firmware_gate4_test.bat", text)
        self.assertIn("Reflash production firmware", text)

    def test_bundled_gate3_profile_baseline(self):
        import json
        baseline = ROOT / "baselines" / "G3_v0.6.2_board_503359277A981F9F" / "results.json"
        self.assertTrue(baseline.exists())
        payload = json.loads(baseline.read_text(encoding="utf-8"))
        self.assertAlmostEqual(payload["flat_metrics"]["G3-004.profile_internal_p95_ms"], 22.08815, places=5)

    def test_gate4_runner_defaults_to_bundled_baseline(self):
        text = (ROOT / "run_robot_gate4_profile.bat").read_text(encoding="utf-8")
        self.assertIn("G3_v0.6.2_board_503359277A981F9F", text)

    def test_bundled_gate4_service_baseline(self):
        import json
        baseline = ROOT / "baselines" / "G4_v0.7.2_board_503359277A981F9F" / "results.json"
        self.assertTrue(baseline.exists())
        payload = json.loads(baseline.read_text(encoding="utf-8"))
        self.assertAlmostEqual(payload["flat_metrics"]["PERF-001.http_state_p95_ms"], 98.349025, places=5)

    def test_gate5_runner_defaults_to_bundled_baseline(self):
        text = (ROOT / "run_robot_gate5_service.bat").read_text(encoding="utf-8")
        self.assertIn("G4_v0.7.2_board_503359277A981F9F", text)
        self.assertIn("--stress-iterations 1000", text)
        self.assertIn("--latency-regression-percent 5.0", text)


if __name__ == "__main__":
    unittest.main()
