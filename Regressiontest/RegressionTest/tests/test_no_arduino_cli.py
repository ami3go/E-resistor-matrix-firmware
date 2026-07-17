from __future__ import annotations

import importlib.util
import unittest
from dataclasses import fields
from pathlib import Path

from e_resistor_regression.cli import parse_args
from e_resistor_regression.models import RunConfig

ROOT = Path(__file__).resolve().parents[1]


class ArduinoCliRemovalTests(unittest.TestCase):
    def test_build_module_and_legacy_launchers_are_absent(self) -> None:
        absent = [
            ROOT / "e_resistor_regression" / "build_check.py",
            ROOT / "run_python_source_build.bat",
            ROOT / "run_robot_source_build.bat",
            ROOT / "robot_framework" / "suites" / "source_build.robot",
            ROOT / "robot_framework" / "resources" / "source_build.resource",
            ROOT / "robot_framework" / "profiles" / "source_build.args",
        ]
        self.assertEqual([str(path.relative_to(ROOT)) for path in absent if path.exists()], [])

    def test_pure_python_cli_has_no_build_tool_arguments(self) -> None:
        args = parse_args(["--output", "out"])
        self.assertFalse(hasattr(args, "arduino_cli"))
        self.assertFalse(hasattr(args, "fqbn"))
        config_fields = {item.name for item in fields(RunConfig)}
        self.assertNotIn("arduino_cli", config_fields)
        self.assertNotIn("fqbn", config_fields)

    def test_robot_launcher_exposes_source_check_only(self) -> None:
        path = ROOT / "robot_framework" / "run_robot.py"
        spec = importlib.util.spec_from_file_location("e_resistor_robot_launcher_no_build", path)
        assert spec and spec.loader
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        args = module.parse_args(["--profile", "source_check", "--source-dir", str(ROOT)])
        self.assertEqual(args.profile, "source_check")
        self.assertFalse(hasattr(args, "arduino_cli"))
        self.assertFalse(hasattr(args, "fqbn"))
        self.assertNotIn("source_build", module.SUITES)

    def test_source_check_suite_has_no_compile_test(self) -> None:
        suite = (ROOT / "robot_framework" / "suites" / "source_check.robot").read_text(encoding="utf-8")
        self.assertIn("SRC Gate Structure Checks", suite)
        self.assertNotIn("BUILD-001", suite)
        self.assertNotIn("compilation", suite.lower())


if __name__ == "__main__":
    unittest.main()
