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
        self.assertIn('version = "2.7.0"', (ROOT / "pyproject.toml").read_text(encoding="utf-8"))
        self.assertIn('__version__ = "2.7.0"', (ROOT / "e_resistor_regression" / "__init__.py").read_text(encoding="utf-8"))
        self.assertIn('ROBOT_LIBRARY_VERSION = "2.7.0"', (ROOT / "robot_framework" / "libraries" / "e_resistor_robot_library.py").read_text(encoding="utf-8"))

    def test_gate4_bat_defaults_and_crlf(self) -> None:
        for name in ("run_robot_read_only.bat", "run_robot_safe_output.bat", "run_robot_hil_single_channel.bat"):
            path = ROOT / name
            text = path.read_text(encoding="utf-8")
            self.assertIn('set "RUN_GATE=G4"', text, name)
        for path in ROOT.rglob("*.bat"):
            data = path.read_bytes()
            self.assertNotIn(b"\n", data.replace(b"\r\n", b""), str(path.relative_to(ROOT)))

    def test_powershell_wrapper_is_robot_only(self) -> None:
        text = (ROOT / "robot_framework" / "run_robot.ps1").read_text(encoding="utf-8")
        self.assertNotIn("source_build", text)
        self.assertIn("gate3_transport_fault", text)
        self.assertIn("gate4_profile", text)
        self.assertIn("gate4_profile_fault", text)
        self.assertIn('[string]$Gate = "G4"', text)

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


if __name__ == "__main__":
    unittest.main()
