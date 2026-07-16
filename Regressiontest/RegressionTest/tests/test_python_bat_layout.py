from __future__ import annotations

import unittest
from pathlib import Path

from e_resistor_regression.cli import parse_args, _split_pipe_values

ROOT = Path(__file__).resolve().parents[1]


class PythonBatLayoutTests(unittest.TestCase):
    def test_required_python_bat_files_exist(self) -> None:
        required = [
            "setup_python_environment.bat",
            "run_python_read_only.bat",
            "run_python_safe_output.bat",
            "run_python_hil_single_channel.bat",
            "run_python_source_build.bat",
            "run_python_all_safe.bat",
            "run_python_custom.bat",
            "validate_python_harness.bat",
            "WINDOWS_PYTHON_BAT_RUNNERS.md",
            "scripts/setup_windows_environment.bat",
        ]
        missing = [name for name in required if not (ROOT / name).exists()]
        self.assertEqual(missing, [])

    def test_hil_bat_has_safety_interlocks(self) -> None:
        text = (ROOT / "run_python_hil_single_channel.bat").read_text(encoding="utf-8")
        self.assertIn("ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS", text)
        self.assertIn("E_RESISTOR_SINGLE_CHANNEL_DMM", text)
        self.assertIn("--allow-active-output-tests", text)

    def test_pipe_delimited_cli_aliases(self) -> None:
        args = parse_args([
            "--output", "out",
            "--dmm-init-commands", "*CLS|CONF:RES AUTO",
            "--hil-serial-fault-patterns", "panic|hardfault",
        ])
        self.assertEqual(_split_pipe_values(args.dmm_init_commands_pipe), ["*CLS", "CONF:RES AUTO"])
        self.assertEqual(_split_pipe_values(args.hil_serial_fault_patterns_pipe), ["panic", "hardfault"])

    def test_setup_repairs_missing_pip(self) -> None:
        text = (ROOT / "scripts" / "setup_windows_environment.bat").read_text(encoding="utf-8")
        self.assertIn("ensurepip", text)
        self.assertIn("-m pip --version", text)


    def test_launchers_request_automatic_environment_repair(self) -> None:
        common = (ROOT / "robot_framework" / "scripts" / "_windows_common.bat").read_text(encoding="utf-8")
        self.assertIn("setup_windows_environment.bat", common)
        self.assertIn("ensure", common)
        for name in [
            "run_python_read_only.bat",
            "run_python_safe_output.bat",
            "run_python_hil_single_channel.bat",
            "run_python_source_build.bat",
            "run_python_all_safe.bat",
            "run_python_custom.bat",
            "validate_python_harness.bat",
        ]:
            text = (ROOT / name).read_text(encoding="utf-8")
            self.assertIn('_windows_common.bat" python', text, name)

    def test_setup_avoids_stale_errorlevel_exit_inside_blocks(self) -> None:
        text = (ROOT / "scripts" / "setup_windows_environment.bat").read_text(encoding="utf-8")
        self.assertNotIn("exit /b %errorlevel%", text.lower())
        self.assertIn("Existing .venv is invalid", text)
        self.assertIn('rmdir /s /q ".venv"', text)
        self.assertIn("last_setup_diagnostics.txt", text)


    def test_example_source_directory_is_disabled_by_default(self) -> None:
        text = (ROOT / "robot_framework" / "variables" / "bench_config.example.bat").read_text(encoding="utf-8")
        active_lines = [line.strip() for line in text.splitlines() if not line.strip().lower().startswith("rem ")]
        self.assertIn('set "ERESISTOR_SOURCE_DIR="', active_lines)
        self.assertFalse(any(line.startswith('set "ERESISTOR_SOURCE_DIR=C:') for line in active_lines))

    def test_bat_files_use_crlf(self) -> None:
        for path in ROOT.glob("*.bat"):
            data = path.read_bytes()
            self.assertNotIn(b"\n", data.replace(b"\r\n", b""), path.name)


if __name__ == "__main__":
    unittest.main()
