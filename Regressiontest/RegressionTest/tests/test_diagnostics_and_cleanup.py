from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from e_resistor_regression.diagnostics import collect_diagnostics, write_diagnostics
from e_resistor_regression.logging_ext import ExtendedLogger
from e_resistor_regression.models import RunConfig
from e_resistor_regression.suite import RegressionSuite


class DiagnosticsTests(unittest.TestCase):
    def test_diagnostics_files_are_written_without_hardware_inventory(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            config = RunConfig(host="127.0.0.1", output_dir=str(output))
            data = collect_diagnostics(config, include_hardware_inventory=False)
            write_diagnostics(output, data)
            self.assertTrue((output / "diagnostics.json").exists())
            self.assertTrue((output / "diagnostics.md").exists())
            parsed = json.loads((output / "diagnostics.json").read_text(encoding="utf-8"))
            self.assertEqual(parsed["network"]["host"], "127.0.0.1")
            self.assertFalse(parsed["hardware_inventory_included"])


class CleanupTests(unittest.TestCase):
    def test_verified_cleanup_retries_and_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = RunConfig(host="127.0.0.1", output_dir=directory)
            logger = ExtendedLogger(Path(directory))
            suite = RegressionSuite(config, logger)

            class FakeClient:
                attempt = 0

                def __init__(self, *args, **kwargs):
                    pass

                def __enter__(self):
                    type(self).attempt += 1
                    return self

                def __exit__(self, exc_type, exc, tb):
                    return None

                def query(self, command):
                    if command == "ALL:OFF":
                        return ("ERR" if self.attempt == 1 else "OK"), 1.0
                    state = ";".join(f"CH{channel}=0000,OPEN" for channel in range(1, 9))
                    return state, 1.0

            with patch("e_resistor_regression.suite.ScpiClient", FakeClient), patch("time.sleep", return_value=None):
                passed, details = suite.best_effort_all_off(attempts=3)
            self.assertTrue(passed)
            self.assertEqual(details["attempts"], 2)


class HilSafetyTests(unittest.TestCase):
    def test_measurement_reverifies_all_channel_masks_after_dmm_settling(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = RunConfig(
                host="127.0.0.1",
                output_dir=directory,
                profile="hil_single_channel",
                hil_channel=3,
                hil_sample_count=3,
            )
            logger = ExtendedLogger(Path(directory))
            suite = RegressionSuite(config, logger)
            suite.dmm = Mock()
            suite.hil_calibration = {bit: 1000.0 for bit in range(16)}

            with (
                patch.object(suite, "_hil_apply_mask_verified", return_value=(1.0, 2.0)) as apply_verified,
                patch.object(suite, "_hil_verify_mask_state", return_value=3.0) as post_verify,
                patch(
                    "e_resistor_regression.suite.wait_for_stable_resistance",
                    return_value=(1000.0, [1000.0, 1000.0, 1000.0], 0.0, 0.0, 4.0, True),
                ),
            ):
                measurement = suite._hil_measure_mask(0x0001, note="unit test")

            apply_verified.assert_called_once_with(0x0001)
            post_verify.assert_called_once_with(0x0001)
            self.assertEqual(measurement.scpi_post_verify_ms, 3.0)
            self.assertEqual(measurement.result, "PASS")

    def test_dmm_auto_probe_closes_failed_and_successful_probe_handles(self) -> None:
        from e_resistor_regression.hil import VisaDmm

        class FakeInstrument:
            def __init__(self, identity=None, error=None):
                self.identity = identity
                self.error = error
                self.closed = False
                self.timeout = 0

            def query(self, command):
                if self.error:
                    raise self.error
                return self.identity

            def close(self):
                self.closed = True

        failed = FakeInstrument(error=RuntimeError("not a DMM"))
        selected = FakeInstrument(identity="HEWLETT-PACKARD,34401A,0,1")

        class FakeResourceManager:
            def list_resources(self):
                return ("USB0::FAILED::INSTR", "USB0::DMM::INSTR")

            def open_resource(self, resource):
                return failed if "FAILED" in resource else selected

        with tempfile.TemporaryDirectory() as directory:
            logger = ExtendedLogger(Path(directory))
            dmm = VisaDmm(
                "auto", logger, Path(directory) / "dmm.jsonl", idn_contains="34401"
            )
            dmm.rm = FakeResourceManager()
            resource = dmm.resolve_resource()

        self.assertEqual(resource, "USB0::DMM::INSTR")
        self.assertTrue(failed.closed)
        self.assertTrue(selected.closed)


if __name__ == "__main__":
    unittest.main()


class EvidenceManifestTests(unittest.TestCase):
    def test_manifest_matches_final_files_and_excludes_itself(self) -> None:
        from e_resistor_regression.evidence import (
            verify_sha256_manifest,
            write_sha256_manifest,
        )

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "events.jsonl").write_text('{"event":"finalized"}\n', encoding="utf-8")
            (output / "RUN_COMPLETE").write_text("status=PASS\n", encoding="utf-8")
            manifest = write_sha256_manifest(output)

            self.assertTrue(manifest.exists())
            self.assertNotIn("evidence_manifest.sha256", manifest.read_text(encoding="utf-8"))
            self.assertEqual(verify_sha256_manifest(output), [])

            (output / "events.jsonl").write_text('{"event":"changed"}\n', encoding="utf-8")
            self.assertEqual(
                verify_sha256_manifest(output),
                ["hash mismatch: events.jsonl"],
            )


class ScpiTransportTests(unittest.TestCase):
    def test_fragmented_response_uses_longer_pre_terminator_idle_window(self) -> None:
        import socket
        from e_resistor_regression.clients import ScpiClient

        class FakeSocket:
            def __init__(self) -> None:
                self.responses = iter([
                    b"CH",
                    b"1=0x0000,OPEN;CH2=0x0000,OPEN\n",
                    socket.timeout(),
                ])
                self.timeouts: list[float] = []

            def settimeout(self, value: float) -> None:
                self.timeouts.append(value)

            def recv(self, _size: int) -> bytes:
                value = next(self.responses)
                if isinstance(value, BaseException):
                    raise value
                return value

        with tempfile.TemporaryDirectory() as directory:
            logger = ExtendedLogger(Path(directory))
            client = ScpiClient("127.0.0.1", 5025, 3.0, logger)
            fake = FakeSocket()
            client.sock = fake  # type: ignore[assignment]
            response = client._read_until_idle()

        self.assertIn("CH1=0x0000", response)
        self.assertGreaterEqual(fake.timeouts[1], 0.70)
        self.assertLessEqual(fake.timeouts[-1], 0.13)

    def test_state_query_reconnects_until_complete(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = RunConfig(host="127.0.0.1", output_dir=directory)
            logger = ExtendedLogger(Path(directory))
            suite = RegressionSuite(config, logger)

            class FakeClient:
                def __init__(self) -> None:
                    self.responses = iter([
                        "",
                        "CH",
                        ";".join(
                            f"CH{channel}=0x0000,OPEN" for channel in range(1, 9)
                        ),
                    ])
                    self.reconnect_count = 0

                def connect(self) -> None:
                    self.reconnect_count += 1

                def query(self, command: str):
                    self.assert_state_command(command)
                    return next(self.responses), 1.0

                @staticmethod
                def assert_state_command(command: str) -> None:
                    if command != "STATE?":
                        raise AssertionError(command)

            client = FakeClient()
            with patch("time.sleep", return_value=None):
                raw, state, elapsed = suite._query_scpi_state(client, attempts=3)  # type: ignore[arg-type]

            self.assertEqual(len(state), 8)
            self.assertIn("CH8=0x0000", raw)
            self.assertEqual(elapsed, 3.0)
            self.assertEqual(client.reconnect_count, 2)
