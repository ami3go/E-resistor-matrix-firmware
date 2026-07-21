from __future__ import annotations

import unittest
from pathlib import Path
from unittest.mock import patch

from e_resistor_regression.hil import SerialMonitor


class _FakeSerialPort:
    def __init__(self, **kwargs):
        self.kwargs = kwargs
        self.dtr = False
        self.rts = True
        self.closed = False
        self.input_reset = False

    def reset_input_buffer(self):
        self.input_reset = True

    def readline(self):
        return b""

    def close(self):
        self.closed = True


class _FakeSerialModule:
    Serial = _FakeSerialPort


class _FakeLogger:
    def event(self, *args, **kwargs):
        pass

    class log:
        @staticmethod
        def exception(*args, **kwargs):
            pass


class SerialMonitorDtrTest(unittest.TestCase):
    def test_start_asserts_dtr_for_rp2040_usb_cdc(self):
        monitor = SerialMonitor("COM17", 115200, _FakeLogger(), Path("serial.log"), "")
        monitor.resolve_port = lambda: "COM17"
        monitor._import_serial = lambda: (_FakeSerialModule, None)
        with patch("time.sleep", return_value=None):
            monitor.start()
        try:
            self.assertTrue(monitor._serial.dtr)
            self.assertFalse(monitor._serial.rts)
            self.assertTrue(monitor._serial.input_reset)
        finally:
            monitor.stop()


if __name__ == "__main__":
    unittest.main()
