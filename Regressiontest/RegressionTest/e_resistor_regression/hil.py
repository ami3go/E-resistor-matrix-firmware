from __future__ import annotations

import csv
import math
import re
import statistics
import threading
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable

from .logging_ext import ExtendedLogger


@dataclass
class HilMeasurement:
    channel: int
    mask: str
    active_bits: int
    expected_ohm: float
    measured_ohm: float
    error_ohm: float
    error_percent: float
    sample_count: int
    sample_mean_ohm: float
    sample_stdev_ohm: float
    sample_rel_stdev_percent: float
    stable: bool
    settle_ms: float
    scpi_apply_ms: float
    scpi_verify_ms: float
    dmm_read_ms: float
    result: str
    note: str = ""

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def parse_bits_spec(spec: str) -> list[int]:
    """Parse bit selections such as ``0-15``, ``0,1,4-7``, or ``all``."""
    text = spec.strip().lower()
    if text in {"all", "*"}:
        return list(range(16))
    bits: set[int] = set()
    for token in text.split(","):
        token = token.strip()
        if not token:
            continue
        if "-" in token:
            start_text, end_text = token.split("-", 1)
            start = int(start_text, 10)
            end = int(end_text, 10)
            if start > end:
                start, end = end, start
            for bit in range(start, end + 1):
                if not 0 <= bit < 16:
                    raise ValueError(f"Bit {bit} is outside 0..15")
                bits.add(bit)
        else:
            bit = int(token, 10)
            if not 0 <= bit < 16:
                raise ValueError(f"Bit {bit} is outside 0..15")
            bits.add(bit)
    if not bits:
        raise ValueError("No HIL bits selected")
    return sorted(bits)


def parse_masks_spec(spec: str) -> list[int]:
    masks: list[int] = []
    for token in spec.split(","):
        token = token.strip()
        if not token:
            continue
        value = int(token, 16)
        if not 0 <= value <= 0xFFFF:
            raise ValueError(f"Mask {token!r} is outside 0000..FFFF")
        if value and value not in masks:
            masks.append(value)
    return masks


def equivalent_resistance_ohm(mask: int, resistance_by_bit: dict[int, float]) -> float:
    conductance = 0.0
    for bit in range(16):
        if not (mask & (1 << bit)):
            continue
        resistance = float(resistance_by_bit[bit])
        if not math.isfinite(resistance) or resistance <= 0.0:
            raise ValueError(f"Invalid resistance for bit {bit}: {resistance!r}")
        conductance += 1.0 / resistance
    if conductance <= 0.0:
        return math.inf
    return 1.0 / conductance


def error_percent(measured_ohm: float, expected_ohm: float) -> float:
    if not math.isfinite(expected_ohm) or expected_ohm == 0.0:
        return math.nan
    return (measured_ohm - expected_ohm) / expected_ohm * 100.0


class SerialMonitor:
    """Background reader for the RP2040 USB CDC/COM diagnostic stream."""

    DEFAULT_MATCH = re.compile(r"(rp2040|raspberry pi pico|pico|usb serial|cdc|arduino)", re.IGNORECASE)

    def __init__(self, port: str, baudrate: int, logger: ExtendedLogger,
                 output_path: Path, match_text: str = "") -> None:
        self.requested_port = port
        self.baudrate = baudrate
        self.logger = logger
        self.output_path = output_path
        self.match_text = match_text
        self.port = ""
        self._serial: Any = None
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._lines: list[tuple[float, str]] = []
        self._exception: Exception | None = None

    @staticmethod
    def _import_serial():
        try:
            import serial  # type: ignore
            from serial.tools import list_ports  # type: ignore
        except ImportError as exc:  # pragma: no cover - depends on optional package
            raise RuntimeError("pyserial is required for HIL COM-port capture") from exc
        return serial, list_ports

    def resolve_port(self) -> str:
        serial, list_ports = self._import_serial()
        del serial
        if self.requested_port and self.requested_port.lower() != "auto":
            return self.requested_port

        ports = list(list_ports.comports())
        if not ports:
            raise RuntimeError("No serial/COM ports were detected")

        custom = self.match_text.strip().lower()
        candidates = []
        for item in ports:
            combined = " ".join(str(value or "") for value in (
                item.device, item.description, item.manufacturer, item.product,
                item.hwid, item.vid, item.pid,
            ))
            if custom and custom in combined.lower():
                candidates.append(item)
            elif not custom and self.DEFAULT_MATCH.search(combined):
                candidates.append(item)

        if len(candidates) == 1:
            return str(candidates[0].device)
        if not candidates and len(ports) == 1:
            return str(ports[0].device)

        listed = [
            {
                "device": item.device,
                "description": item.description,
                "manufacturer": item.manufacturer,
                "hwid": item.hwid,
            }
            for item in (candidates or ports)
        ]
        raise RuntimeError(
            "Unable to select one board COM port automatically. "
            f"Use --serial-port. Candidates: {listed}"
        )

    def start(self) -> None:
        serial, _ = self._import_serial()
        self.port = self.resolve_port()
        self.output_path.parent.mkdir(parents=True, exist_ok=True)
        self._serial = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            timeout=0.2,
            write_timeout=1.0,
        )
        # RP2040 Arduino-Pico USB CDC suppresses transmit data while the host has
        # not asserted DTR. Keep the requested baud far from the 1200-baud
        # bootloader trigger, assert DTR for normal CDC traffic, and leave RTS low.
        try:
            self._serial.dtr = True
            self._serial.rts = False
        except Exception:
            pass
        time.sleep(0.20)
        try:
            self._serial.reset_input_buffer()
        except Exception:
            pass
        self._stop.clear()
        self._thread = threading.Thread(target=self._reader, name="e-resistor-serial-monitor", daemon=True)
        self._thread.start()
        self.logger.event("serial_monitor_started", port=self.port, baudrate=self.baudrate)

    def _reader(self) -> None:
        assert self._serial is not None
        try:
            with self.output_path.open("a", encoding="utf-8") as handle:
                while not self._stop.is_set():
                    raw = self._serial.readline()
                    if not raw:
                        continue
                    timestamp = time.time()
                    text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                    with self._lock:
                        self._lines.append((timestamp, text))
                    handle.write(f"{timestamp:.6f}\t{text}\n")
                    handle.flush()
                    self.logger.event("serial_line", port=self.port, text=text)
        except Exception as exc:  # pragma: no cover - depends on hardware
            self._exception = exc
            self.logger.log.exception("Serial monitor failed")

    def marker(self, text: str) -> None:
        timestamp = time.time()
        with self.output_path.open("a", encoding="utf-8") as handle:
            handle.write(f"{timestamp:.6f}\t--- {text} ---\n")
        self.logger.event("serial_marker", text=text)

    def lines_since(self, timestamp: float) -> list[str]:
        with self._lock:
            return [line for line_timestamp, line in self._lines if line_timestamp >= timestamp]

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        if self._serial is not None:
            try:
                self._serial.close()
            finally:
                self._serial = None
        self.logger.event("serial_monitor_stopped", port=self.port, exception=repr(self._exception) if self._exception else "")

    @property
    def exception(self) -> Exception | None:
        return self._exception


class VisaDmm:
    """Small configurable PyVISA DMM adapter suitable for HP/Keysight 34401A."""

    def __init__(self, resource: str, logger: ExtendedLogger, output_path: Path,
                 idn_contains: str = "", init_commands: Iterable[str] = (),
                 measure_command: str = "READ?", timeout_s: float = 10.0,
                 backend: str = "") -> None:
        self.requested_resource = resource
        self.logger = logger
        self.output_path = output_path
        self.idn_contains = idn_contains
        self.init_commands = [command.strip() for command in init_commands if command.strip()]
        self.measure_command = measure_command.strip() or "READ?"
        self.timeout_s = timeout_s
        self.backend = backend
        self.rm: Any = None
        self.instrument: Any = None
        self.resource = ""
        self.identity = ""

    @staticmethod
    def _import_pyvisa():
        try:
            import pyvisa  # type: ignore
        except ImportError as exc:  # pragma: no cover - depends on optional package
            raise RuntimeError("PyVISA is required for USB DMM HIL tests") from exc
        return pyvisa

    def _transcript(self, direction: str, payload: str, **metadata: Any) -> None:
        self.logger.transcript("dmm", direction, payload, **metadata)

    def resolve_resource(self) -> str:
        if self.requested_resource and self.requested_resource.lower() != "auto":
            return self.requested_resource
        assert self.rm is not None
        all_resources = list(self.rm.list_resources())
        if not all_resources:
            raise RuntimeError("PyVISA found no instrument resources")
        # Do not probe serial resources during automatic DMM selection because
        # the RP2040 USB CDC diagnostic port may also be visible through VISA.
        # An ASRL DMM remains supported when its resource is supplied explicitly.
        resources = [resource for resource in all_resources if not str(resource).upper().startswith("ASRL")]
        if not resources:
            resources = all_resources

        matches: list[str] = []
        probes: list[dict[str, str]] = []
        for resource in resources:
            identity = ""
            try:
                instrument = self.rm.open_resource(resource)
                instrument.timeout = max(1000, int(self.timeout_s * 1000))
                identity = str(instrument.query("*IDN?")).strip()
                instrument.close()
            except Exception as exc:
                identity = f"<query failed: {type(exc).__name__}: {exc}>"
            probes.append({"resource": str(resource), "identity": identity})
            if self.idn_contains and self.idn_contains.lower() in identity.lower():
                matches.append(str(resource))
            elif not self.idn_contains and any(token in identity.upper() for token in ("34401", "DMM", "MULTIMETER")):
                matches.append(str(resource))

        if len(matches) == 1:
            return matches[0]
        if not matches and len(resources) == 1:
            return str(resources[0])
        raise RuntimeError(
            "Unable to select one DMM automatically. Use --dmm-resource or "
            f"--dmm-idn-contains. Probed resources: {probes}"
        )

    def open(self) -> None:
        pyvisa = self._import_pyvisa()
        self.output_path.parent.mkdir(parents=True, exist_ok=True)
        self.rm = pyvisa.ResourceManager(self.backend) if self.backend else pyvisa.ResourceManager()
        self.resource = self.resolve_resource()
        self.instrument = self.rm.open_resource(self.resource)
        self.instrument.timeout = max(1000, int(self.timeout_s * 1000))
        # Common text termination defaults; vendor drivers may already supply these.
        try:
            self.instrument.read_termination = "\n"
            self.instrument.write_termination = "\n"
        except Exception:
            pass
        self.identity = self.query("*IDN?")
        if self.idn_contains and self.idn_contains.lower() not in self.identity.lower():
            raise RuntimeError(
                f"DMM identity {self.identity!r} does not contain {self.idn_contains!r}"
            )
        for command in self.init_commands:
            self.write(command)
        self.logger.event("dmm_opened", resource=self.resource, identity=self.identity)

    def close(self) -> None:
        if self.instrument is not None:
            try:
                self.instrument.close()
            finally:
                self.instrument = None
        if self.rm is not None:
            try:
                self.rm.close()
            finally:
                self.rm = None
        self.logger.event("dmm_closed", resource=self.resource)

    def write(self, command: str) -> None:
        if self.instrument is None:
            raise RuntimeError("DMM is not open")
        start = time.perf_counter_ns()
        self._transcript("request", command, resource=self.resource)
        try:
            self.instrument.write(command)
        except Exception as exc:
            elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
            self._transcript(
                "error", repr(exc), command=command, elapsed_ms=elapsed_ms,
                resource=self.resource, exception_type=type(exc).__name__,
            )
            raise
        elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        self._transcript("response", "", command=command, elapsed_ms=elapsed_ms, resource=self.resource)

    def query(self, command: str) -> str:
        if self.instrument is None:
            raise RuntimeError("DMM is not open")
        start = time.perf_counter_ns()
        self._transcript("request", command, resource=self.resource)
        try:
            response = str(self.instrument.query(command)).strip()
        except Exception as exc:
            elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
            self._transcript(
                "error", repr(exc), command=command, elapsed_ms=elapsed_ms,
                resource=self.resource, exception_type=type(exc).__name__,
            )
            raise
        elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        self._transcript("response", response, command=command, elapsed_ms=elapsed_ms, resource=self.resource)
        return response

    def read_resistance(self) -> tuple[float, float]:
        start = time.perf_counter_ns()
        response = self.query(self.measure_command)
        elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        token = response.strip().split(",", 1)[0].strip()
        value = float(token)
        # Common SCPI overflow values are around 9.9E37.
        if abs(value) >= 1e35:
            value = math.inf
        return value, elapsed_ms


def wait_for_stable_resistance(
    dmm: VisaDmm,
    *,
    window: int,
    interval_s: float,
    timeout_s: float,
    relative_stdev_limit_percent: float,
    minimum_wait_s: float = 0.0,
) -> tuple[float, list[float], float, float, float, bool]:
    """Read until the latest window is finite and stable, or until timeout.

    Returns ``(median, samples, stdev, relative_stdev_percent, total_read_ms, stable)``.
    """
    if window < 2:
        raise ValueError("DMM stability window must be at least 2")
    deadline = time.monotonic() + timeout_s
    samples: list[float] = []
    total_read_ms = 0.0
    stable = False
    start = time.monotonic()
    while time.monotonic() < deadline:
        value, read_ms = dmm.read_resistance()
        total_read_ms += read_ms
        samples.append(value)
        finite_window = [item for item in samples[-window:] if math.isfinite(item)]
        if len(finite_window) == window and (time.monotonic() - start) >= minimum_wait_s:
            mean = statistics.fmean(finite_window)
            stdev = statistics.stdev(finite_window) if len(finite_window) > 1 else 0.0
            relative = abs(stdev / mean * 100.0) if mean else math.inf
            if relative <= relative_stdev_limit_percent:
                stable = True
                break
        if interval_s > 0:
            time.sleep(interval_s)

    finite = [item for item in samples if math.isfinite(item)]
    if finite:
        selected = finite[-window:] if len(finite) >= window else finite
        median = statistics.median(selected)
        mean = statistics.fmean(selected)
        stdev = statistics.stdev(selected) if len(selected) > 1 else 0.0
        relative = abs(stdev / mean * 100.0) if mean else math.inf
    else:
        median = math.inf
        stdev = math.inf
        relative = math.inf
    return median, samples, stdev, relative, total_read_ms, stable


def write_hil_measurements(path: Path, measurements: list[HilMeasurement]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(HilMeasurement.__dataclass_fields__.keys())
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for measurement in measurements:
            writer.writerow(measurement.to_dict())
