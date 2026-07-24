from __future__ import annotations

import csv
import fnmatch
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable


ALL_GATES = tuple(f"G{i}" for i in range(10))


def gates_from(start: int) -> tuple[str, ...]:
    return tuple(f"G{i}" for i in range(start, 10))


@dataclass(frozen=True)
class CoverageItem:
    coverage_id: str
    category: str
    requirement: str
    gates: tuple[str, ...]
    profiles: tuple[str, ...]
    test_patterns: tuple[str, ...]
    coverage_type: str
    implementation: str
    hardware: str
    acceptance: str
    evidence: str


COVERAGE_ITEMS: tuple[CoverageItem, ...] = (
    CoverageItem("COV-001", "Connectivity", "Board TCP services are reachable", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("NET-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Required TCP ports accept connections within timeout", "results.json, scpi_transcript.log"),
    CoverageItem("COV-002", "HTTP", "HTTP health endpoint responds", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("HTTP-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "HTTP response is successful and recognizable", "http_transcript.log"),
    CoverageItem("COV-003", "HTTP", "Runtime state schema and readiness are valid", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("HTTP-002",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "State contains firmware, readiness, heap and eight channel records", "state_before.txt, results.json"),
    CoverageItem("COV-004", "GUI", "Required GUI pages remain available", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("HTTP-003",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Every required route returns HTTP 200", "http_transcript.log"),
    CoverageItem("COV-005", "SCPI", "SCPI identity and greeting remain compatible", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("SCPI-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "*IDN? identifies E-Resistor and connection greeting is valid", "scpi_transcript.log"),
    CoverageItem("COV-006", "SCPI", "SCPI state agrees with HTTP state", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("SCPI-002",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "All eight masks and reported states are consistent", "state_before.txt, scpi_transcript.log"),
    CoverageItem("COV-007", "Calibration", "All channel calibration tables contain valid 16-bit data", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("SCPI-003",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "CH1-CH8 each contain bits 0-15 with finite positive resistance", "scpi_transcript.log, results.json"),
    CoverageItem("COV-008", "SCPI", "Undefined command error handling remains correct", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("SCPI-004",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Invalid command enters and clears the expected error queue", "scpi_transcript.log"),
    CoverageItem("COV-009", "SCPI", "Oversized SCPI line is rejected and parser recovers", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("SCPI-005",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Oversized line is rejected and the next valid command succeeds", "scpi_transcript.log"),
    CoverageItem("COV-010", "Performance", "HTTP latency does not regress beyond gate limit", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("PERF-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Latency samples pass and baseline regression remains within configured percentage", "metrics.csv, report.md"),
    CoverageItem("COV-011", "Performance", "SCPI latency does not regress beyond gate limit", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("PERF-002",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Latency samples pass and baseline regression remains within configured percentage", "metrics.csv, report.md"),
    CoverageItem("COV-012", "Memory", "Repeated state requests do not cause excessive heap decline", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("MEM-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Heap decline remains below configured byte limit", "metrics.csv, state_before.txt, state_after.txt"),
    CoverageItem("COV-013", "Stress", "Concurrent HTTP and SCPI traffic remains stable", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("STRESS-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "No protocol error and latency remains within gate limits", "http_transcript.log, scpi_transcript.log"),
    CoverageItem("COV-014", "Files", "Combined calibration bundle downloads successfully", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("FILES-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Bundle contains BEGIN markers for CH1-CH8", "http_transcript.log"),
    CoverageItem("COV-015", "Log", "Firmware log exports as a text file", ALL_GATES,
                 ("read_only", "safe_output", "hil_single_channel"), ("LOG-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Log endpoint returns a successful text response", "http_transcript.log"),
    CoverageItem("COV-016", "Output safety", "Zero-mask all-channel command path is safe", gates_from(1),
                 ("safe_output",), ("SAFE-001",), "Automated", "Implemented", "E-Resistor Ethernet",
                 "Command returns OK and every channel remains 0000", "scpi_transcript.log, state_after.txt"),
    CoverageItem("COV-017", "Output safety", "Repeated ALL:OFF remains reliable", gates_from(1),
                 ("safe_output",), ("SAFE-002",), "Automated", "Implemented", "E-Resistor Ethernet",
                 "All requests return OK and final state is eight zero masks", "scpi_transcript.log, metrics.csv"),
    CoverageItem("COV-018", "HIL fixture", "USB COM and DMM are discovered and identified", ALL_GATES,
                 ("hil_single_channel",), ("HIL-001",), "HIL", "Implemented", "USB COM + USB/VISA DMM",
                 "Selected COM opens, DMM *IDN? matches and calibration is available", "serial_console.log, dmm_transcript.log"),
    CoverageItem("COV-019", "HIL safety", "Fixture starts with all outputs OFF and high isolation", ALL_GATES,
                 ("hil_single_channel",), ("HIL-002",), "HIL", "Implemented", "USB DMM on selected channel",
                 "All masks are 0000 and measured OFF resistance exceeds configured minimum", "hil_measurements.csv, scpi_transcript.log"),
    CoverageItem("COV-020", "Physical accuracy", "Every selected single resistor bit matches calibration", ALL_GATES,
                 ("hil_single_channel",), ("HIL-003",), "HIL", "Implemented", "USB DMM on selected channel",
                 "All selected bits stabilize and absolute error stays within configured limit", "hil_measurements.csv, dmm_transcript.log"),
    CoverageItem("COV-021", "Physical accuracy", "Selected parallel combinations match calculated resistance", ALL_GATES,
                 ("hil_single_channel",), ("HIL-004",), "HIL", "Implemented", "USB DMM on selected channel",
                 "Every configured mask stabilizes and remains within error limit", "hil_measurements.csv"),
    CoverageItem("COV-022", "Repeatability", "Repeated ON/OFF switching remains accurate and repeatable", ALL_GATES,
                 ("hil_single_channel",), ("HIL-005",), "HIL", "Implemented", "USB DMM on selected channel",
                 "All cycles pass accuracy, mask verification and repeatability limits", "hil_measurements.csv, metrics.csv"),
    CoverageItem("COV-023", "Diagnostics", "USB serial stream contains no fatal firmware fault patterns", ALL_GATES,
                 ("hil_single_channel",), ("HIL-006",), "HIL", "Implemented", "RP2040 USB COM",
                 "No configured panic, assert, hardfault or queue-overflow pattern is detected", "serial_console.log"),
    CoverageItem("COV-024", "HIL safety", "Final ALL:OFF produces physical channel isolation", ALL_GATES,
                 ("hil_single_channel",), ("HIL-007",), "HIL", "Implemented", "USB DMM on selected channel",
                 "Final masks are zero and measured OFF resistance exceeds configured minimum", "hil_measurements.csv, state_after.txt"),
    CoverageItem("COV-027", "Numeric model", "Numeric resistance model is equivalent to the previous implementation", gates_from(2),
                 ("read_only", "hil_single_channel"), ("SCPI-003", "SCPI-006", "HIL-003", "HIL-004"),
                 "Combined", "Implemented", "Ethernet + selected-channel DMM",
                 "Calibration parsing and physical bit/combination results pass; host oracle comparison is still recommended", "scpi_transcript.log, hil_measurements.csv"),
    CoverageItem("COV-028", "Command transport", "Expired or timed-out commands cannot actuate later", gates_from(3),
                 ("gate3_transport_fault",), ("G3-FI-003",), "HIL fault injection", "Implemented", "Gate 3 test firmware + USB DMM",
                 "Injected timeout is followed by deadline or generation rejection, no late mask change, and verified physical isolation",
                 "dmm_transcript.log, scpi_transcript.log, metrics.csv, state_after.txt"),
    CoverageItem("COV-029", "Dual core", "Core 1 remains a deterministic hardware-only engine", gates_from(3),
                 ("read_only", "hil_single_channel"), ("SRC-*", "HIL-005", "HIL-006"),
                 "Combined", "Implemented", "Source tree + USB COM + DMM",
                 "Source checks find no prohibited Core-1 services; repeat tests show no fault or timing regression", "source_metrics.json, serial_console.log, metrics.csv"),
    CoverageItem("COV-030", "Profile switching", "Two-phase eight-channel profile update avoids mixed published states", gates_from(4),
                 ("gate4_profile", "hil_single_channel", "gate4_profile_fault"), ("G4-001", "G4-002", "G4-003", "G4-FI-001"),
                 "Combined", "Implemented", "E-Resistor Ethernet + selected-channel DMM",
                 "One global BBM is counted per profile, only complete tuples are published, and failure after clear remains physically OFF",
                 "results.json, metrics.csv, http_transcript.log, scpi_transcript.log, dmm_transcript.log"),
    CoverageItem("COV-052", "HTTP architecture", "Browser pages use flash-backed cacheable CSS and JavaScript and retain required content", gates_from(5),
                 ("gate5_service",), ("G5-001",), "Automated", "Implemented", "E-Resistor Ethernet",
                 "Required pages and assets return 200, assets are immutable-cacheable, and inline style/event handlers are absent",
                 "http_transcript.log, results.json"),
    CoverageItem("COV-053", "API", "Versioned API v1 schemas and documented compatibility aliases remain coherent", gates_from(5),
                 ("gate5_service",), ("G5-002",), "Automated", "Implemented", "E-Resistor Ethernet",
                 "Focused JSON endpoints expose schema version 1, eight coherent channels, and legacy calibration aliases match",
                 "http_transcript.log, results.json"),
    CoverageItem("COV-054", "HTTP safety", "All state-changing legacy HTTP routes reject GET and preserve output state", gates_from(5),
                 ("gate5_service",), ("G5-003",), "Automated", "Implemented", "E-Resistor Ethernet",
                 "Every mutation GET returns 405 with Allow POST, rejection count advances, and no output command is submitted",
                 "http_transcript.log, state_before.txt, state_after.txt"),
    CoverageItem("COV-055", "SCPI compatibility", "SCPI aliases and generated documentation use the shared command registry", gates_from(5),
                 ("gate5_service",), ("G5-004",), "Automated", "Implemented", "E-Resistor Ethernet",
                 "Canonical and alias responses agree and canonical commands appear in both HELP and the SCPI page",
                 "scpi_transcript.log, http_transcript.log"),
    CoverageItem("COV-056", "HTTP memory", "Calibration page peak temporary heap is reduced by at least 50 percent from Gate 4", gates_from(5),
                 ("gate5_service",), ("G5-005",), "Automated", "Implemented", "E-Resistor Ethernet",
                 "Measured temporary heap is no more than half the embedded Gate 4 baseline and response is streamed",
                 "metrics.csv, results.json, http_transcript.log"),
    CoverageItem("COV-057", "HTTP performance", "One thousand page requests do not leak heap and /state p95 remains within five percent of Gate 4", gates_from(5),
                 ("gate5_service",), ("G5-006",), "Automated", "Implemented", "E-Resistor Ethernet + accepted G4 baseline",
                 "At least 1000 pages return 200, persistent heap decline stays within limit, and /state p95 is <= G4 p95 plus 5 percent",
                 "metrics.csv, results.json, http_transcript.log"),
    CoverageItem("COV-058", "HIL service stress", "Full selected-channel HIL runs while HTTP and SCPI state polling remains active", gates_from(5),
                 ("hil_single_channel",), ("G5-HIL-001", "HIL-003", "HIL-004", "HIL-005", "G5-HIL-002"),
                 "HIL", "Implemented", "E-Resistor Ethernet + USB COM + USB/VISA DMM",
                 "Concurrent HTTP polling has no errors, SCPI state polling remains active through the HIL sequence, and all physical tests pass",
                 "http_transcript.log, scpi_transcript.log, dmm_transcript.log, hil_measurements.csv"),
    CoverageItem("COV-031", "Storage", "Calibration export/import round trip preserves all eight tables", gates_from(6),
                 ("storage",), (), "Automated/HIL", "Planned", "E-Resistor + backup file",
                 "Pre/post calibration hashes match and HIL accuracy remains within limits", "future storage transcript and calibration hashes"),
    CoverageItem("COV-032", "Storage", "Interrupted configuration writes recover atomically", gates_from(6),
                 ("storage",), (), "Fault injection", "Planned", "Test firmware + controlled reset",
                 "After interruption either the old or complete new file loads; no truncated active file", "future storage recovery log"),
    CoverageItem("COV-033", "OTA", "Invalid, incomplete or corrupt OTA candidate never replaces running firmware", gates_from(7),
                 ("ota",), (), "Automated/HIL", "Planned", "E-Resistor + OTA test images",
                 "Every negative image is rejected before commit and current firmware identity remains unchanged", "future OTA transcript and hashes"),
    CoverageItem("COV-034", "OTA", "Valid OTA completes and preserves configuration", gates_from(7),
                 ("ota", "hil_single_channel"), (), "HIL", "Planned", "E-Resistor + signed image + DMM",
                 "New identity boots, calibration hash is unchanged and full HIL passes after reboot", "future OTA report and HIL evidence"),
    CoverageItem("COV-035", "OTA", "Power loss during bootloader copy recovers safely", gates_from(7),
                 ("ota",), (), "Manual fault injection", "Manual", "Controlled power switch + recovery access",
                 "Board resumes or restarts staged copy and returns to a documented recoverable state", "manual power-loss test record"),
    CoverageItem("COV-036", "Watchdog", "Core 0 freeze causes physical safe state and reset", gates_from(8),
                 ("watchdog",), (), "HIL fault injection", "Planned", "Test firmware + USB DMM",
                 "Core 1 forces OFF, DMM confirms isolation, watchdog reset reason identifies Core 0 fault", "future watchdog trace and DMM log"),
    CoverageItem("COV-037", "Watchdog", "Core 1 freeze is detected and leads to reset", gates_from(8),
                 ("watchdog",), (), "HIL fault injection", "Planned", "Test firmware + USB DMM",
                 "Core 0 latches fault, stops feeding watchdog, reboot occurs and startup forces OFF", "future watchdog report"),
    CoverageItem("COV-038", "Recovery", "Repeated watchdog failures enter recovery mode", gates_from(8),
                 ("watchdog",), (), "Fault injection", "Planned", "Test firmware",
                 "Configured reset count activates recovery mode and output commands are rejected", "future recovery-mode log"),
    CoverageItem("COV-041", "Target search", "Read-only target-mask calculation matches the calibrated host oracle and never changes outputs", gates_from(2),
                 ("read_only", "safe_output", "hil_single_channel"), ("SCPI-006",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Multiple targets return valid masks, resistance/error fields agree with host calculation, deadlines pass, and masks remain unchanged",
                 "scpi_transcript.log, metrics.csv, state_before.txt, state_after.txt"),
    CoverageItem("COV-042", "Diagnostics", "A deterministic firmware event is observable on the configured RP2040 USB COM port", gates_from(2),
                 ("hil_single_channel",), ("HIL-008",),
                 "HIL", "Implemented", "RP2040 USB COM",
                 "SYST:DIAG:SERIAL? returns OK and the matching structured SERIAL_TEST event is captured",
                 "serial_console.log, scpi_transcript.log"),
    CoverageItem("COV-043", "Core transport", "Bounded Core 0/Core 1 command transport reports healthy sequence and error counters", gates_from(3),
                 ("read_only", "safe_output", "hil_single_channel"), ("G3-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Transport generation and policy are valid; timeout, overflow, invalid and fail-safe counters remain zero in normal runs",
                 "scpi_transcript.log, metrics.csv, results.json"),
    CoverageItem("COV-044", "State coherence", "HTTP and SCPI expose the same atomic Core 1 output snapshot", gates_from(3),
                 ("read_only", "safe_output", "hil_single_channel"), ("G3-002",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "Snapshot sequence, generation, flags, masks and apply counters are coherent across interfaces",
                 "state_before.txt, scpi_transcript.log, results.json"),
    CoverageItem("COV-045", "Core transport", "Sequenced safe-output command stress has no overflow, timeout or generation mismatch", gates_from(3),
                 ("safe_output",), ("G3-003",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "At least 100 ALL:OFF commands complete in sequence without transport error-counter deltas and without generation change",
                 "scpi_transcript.log, metrics.csv, state_after.txt"),
    CoverageItem("COV-046", "Fault injection", "A queued command with an invalidated generation is rejected before physical actuation", gates_from(3),
                 ("gate3_transport_fault",), ("G3-FI-001", "G3-FI-002"),
                 "HIL fault injection", "Implemented", "Gate 3 test firmware + USB DMM",
                 "The next queued command is invalidated inside Core 1, generation rejection advances without timeout, all masks stay zero, and DMM confirms isolation",
                 "dmm_transcript.log, scpi_transcript.log, metrics.csv, state_after.txt"),
    CoverageItem("COV-047", "Fault injection", "A timed-out stale command is rejected before physical actuation", gates_from(3),
                 ("gate3_transport_fault",), ("G3-FI-001", "G3-FI-003"),
                 "HIL fault injection", "Implemented", "Gate 3 test firmware + USB DMM",
                 "Injected delay causes a Core 0 timeout and subsequent deadline or generation rejection; all masks stay zero and DMM confirms isolation",
                 "dmm_transcript.log, scpi_transcript.log, metrics.csv, state_after.txt"),
    CoverageItem("COV-048", "Profile diagnostics", "Gate 4 profile counters are coherent across interfaces", gates_from(4),
                 ("read_only", "safe_output", "hil_single_channel", "gate4_profile"), ("G4-001",),
                 "Automated", "Implemented", "E-Resistor Ethernet",
                 "SCPI transport, SCPI profile query and HTTP state expose matching transition, failure, BBM and duration values",
                 "scpi_transcript.log, http_transcript.log, metrics.csv"),
    CoverageItem("COV-049", "Profile performance", "Two-phase profile switching uses one BBM and improves internal p95 by at least 30 percent", gates_from(4),
                 ("gate4_profile",), ("G4-002",),
                 "Automated", "Implemented", "E-Resistor Ethernet + corrected G3 baseline",
                 "At least 1000 profiles pass, BBM delta equals transition count, error deltas remain zero and p95 improvement is at least 30 percent",
                 "results.json, metrics.csv, scpi_transcript.log"),
    CoverageItem("COV-050", "State coherence", "Published state never exposes an intermediate mixed profile", gates_from(4),
                 ("hil_single_channel",), ("G4-003",),
                 "HIL", "Implemented", "E-Resistor Ethernet + USB DMM",
                 "Concurrent observations contain only the complete old or complete new tuple and final physical isolation passes",
                 "results.json, http_transcript.log, dmm_transcript.log"),
    CoverageItem("COV-051", "Profile fault injection", "Failure after the global clear phase leaves all outputs physically OFF", gates_from(4),
                 ("gate4_profile_fault",), ("G4-FI-001",),
                 "HIL fault injection", "Implemented", "Gate 4 test firmware + USB DMM",
                 "Command returns an error, failure and BBM counters advance once, all masks remain zero and DMM confirms isolation",
                 "results.json, scpi_transcript.log, dmm_transcript.log, state_after.txt"),
    CoverageItem("COV-039", "Physical scope", "All eight output channels receive physical resistance verification", ("G9",),
                 ("hil_single_channel",), (), "Manual fixture expansion", "Manual", "DMM relay/multiplexer or manual rewiring",
                 "Full bit and combination HIL passes independently on CH1-CH8", "eight-channel HIL archive"),
    CoverageItem("COV-040", "Soak", "Production candidate completes extended stability soak", ("G9",),
                 ("read_only", "hil_single_channel"), (), "Automated/manual", "Planned", "E-Resistor + DMM",
                 "24-hour run has no reset, heap drift, protocol failure or unexplained resistance drift", "soak report and complete logs"),
)


def _matches(patterns: Iterable[str], test_id: str) -> bool:
    return any(fnmatch.fnmatchcase(test_id, pattern) for pattern in patterns)


def evaluate_coverage(summary: dict[str, Any]) -> list[dict[str, Any]]:
    gate = str(summary.get("config", {}).get("gate", "G0"))
    results = list(summary.get("results", []))
    rows: list[dict[str, Any]] = []

    for item in COVERAGE_ITEMS:
        if gate not in item.gates:
            continue

        matched = [result for result in results if _matches(item.test_patterns, str(result.get("test_id", "")))]
        statuses = [str(result.get("status", "")) for result in matched]
        matched_patterns = {
            pattern for pattern in item.test_patterns
            if any(fnmatch.fnmatchcase(str(result.get("test_id", "")), pattern) for result in results)
        }
        missing_patterns = set(item.test_patterns) - matched_patterns

        if item.implementation == "Planned":
            status = "PLANNED"
        elif item.implementation == "Manual" and not item.test_patterns:
            status = "MANUAL"
        elif not matched:
            status = "NOT_RUN"
        elif "FAIL" in statuses:
            status = "FAIL"
        elif statuses and all(value == "SKIP" for value in statuses):
            status = "SKIP"
        elif missing_patterns:
            status = "PARTIAL"
        elif "PASS" in statuses and "SKIP" in statuses:
            status = "PARTIAL"
        elif statuses and all(value == "PASS" for value in statuses):
            status = "PASS"
        else:
            status = "PARTIAL"

        row = asdict(item)
        row.update({
            "gates": ",".join(item.gates),
            "profiles": ",".join(item.profiles),
            "test_patterns": ",".join(item.test_patterns),
            "run_status": status,
            "matched_tests": ",".join(str(result.get("test_id", "")) for result in matched),
        })
        rows.append(row)
    return rows


def coverage_counts(rows: list[dict[str, Any]]) -> dict[str, int]:
    statuses = ("PASS", "FAIL", "PARTIAL", "SKIP", "NOT_RUN", "PLANNED", "MANUAL")
    return {status: sum(1 for row in rows if row.get("run_status") == status) for status in statuses}


def write_coverage_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    fields = [
        "coverage_id", "category", "requirement", "coverage_type", "implementation",
        "gates", "profiles", "test_patterns", "run_status", "matched_tests", "hardware",
        "acceptance", "evidence",
    ]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def write_coverage_json(path: Path, rows: list[dict[str, Any]]) -> None:
    payload = {"counts": coverage_counts(rows), "rows": rows}
    path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")


def _escape(value: Any) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def write_coverage_markdown(path: Path, rows: list[dict[str, Any]], title: str = "Test Coverage Table") -> None:
    counts = coverage_counts(rows)
    lines = [
        f"# {title}",
        "",
        "Coverage status is based on the current regression run. `NOT_RUN` means the test exists but the selected profile or command did not execute it. `PLANNED` and `MANUAL` are never counted as passed.",
        "",
        "## Coverage summary",
        "",
        "| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |",
        "|---:|---:|---:|---:|---:|---:|---:|",
        f"| {counts['PASS']} | {counts['FAIL']} | {counts['PARTIAL']} | {counts['SKIP']} | {counts['NOT_RUN']} | {counts['PLANNED']} | {counts['MANUAL']} |",
        "",
        "## Requirement coverage",
        "",
        "| ID | Area | Requirement | Type | Implementation | Profile(s) | Test(s) | Status | Acceptance | Evidence |",
        "|---|---|---|---|---|---|---|---:|---|---|",
    ]
    for row in rows:
        lines.append(
            "| {coverage_id} | {category} | {requirement} | {coverage_type} | {implementation} | "
            "{profiles} | {test_patterns} | {run_status} | {acceptance} | {evidence} |".format(
                **{key: _escape(value) for key, value in row.items()}
            )
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_static_coverage_markdown(path: Path) -> None:
    rows: list[dict[str, Any]] = []
    for item in COVERAGE_ITEMS:
        row = asdict(item)
        row.update({
            "gates": ",".join(item.gates),
            "profiles": ",".join(item.profiles),
            "test_patterns": ",".join(item.test_patterns) or "—",
            "run_status": item.implementation.upper(),
            "matched_tests": "",
        })
        rows.append(row)

    lines = [
        "# E-Resistor Test Coverage Table",
        "",
        "This is the master traceability table for the optimization program. The regression runner creates a gate-filtered `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json` after every execution.",
        "",
        "## Status definitions",
        "",
        "| State | Meaning |",
        "|---|---|",
        "| Implemented | Test logic exists in the current harness |",
        "| Planned | Required by a future gate but test logic is not implemented yet |",
        "| Manual | Requires controlled manual evidence or additional fixture hardware |",
        "",
        "## Master coverage matrix",
        "",
        "| ID | Gate(s) | Area | Requirement | Type | Implementation | Profile(s) | Test ID(s) | Hardware | Acceptance | Evidence |",
        "|---|---|---|---|---|---|---|---|---|---|---|",
    ]
    for row in rows:
        lines.append(
            "| {coverage_id} | {gates} | {category} | {requirement} | {coverage_type} | {implementation} | "
            "{profiles} | {test_patterns} | {hardware} | {acceptance} | {evidence} |".format(
                **{key: _escape(value) for key, value in row.items()}
            )
        )

    lines += [
        "",
        "## Known coverage limits",
        "",
        "- The available DMM fixture physically verifies one selected channel per run. Software state checks still cover all eight channel masks.",
        "- Full physical CH1–CH8 coverage requires manual rewiring or a relay/multiplexer fixture and remains a G9 manual requirement.",
        "- OTA, watchdog, interrupted-write, timeout-cancellation, and two-phase transition fault tests are explicitly marked planned until their gate-specific hooks exist.",
        "- A skipped or not-run test is not treated as covered for gate acceptance.",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
