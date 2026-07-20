#!/usr/bin/env python3
"""Offline structural checks for E-Resistor optimization Gate 3."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VALIDATION = ROOT / "validation"
CORE1_FILES = [
    ROOT / "core1_engine.h",
    ROOT / "core1_runtime.cpp",
    ROOT / "core1_output_engine.cpp",
    ROOT / "core1_event_queue.cpp",
    ROOT / "shift_registers.cpp",
    ROOT / "core_transport_types.h",
]


def strip_comments_and_literals(text: str) -> str:
    """Remove comments and string/character literal bodies for token checks."""
    out: list[str] = []
    i = 0
    state = "code"
    quote = ""
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if c == "/" and n == "/":
                state = "line"; out.extend("  "); i += 2; continue
            if c == "/" and n == "*":
                state = "block"; out.extend("  "); i += 2; continue
            if c in {'"', "'"}:
                state = "literal"; quote = c; out.append(" "); i += 1; continue
            out.append(c); i += 1; continue
        if state == "line":
            if c == "\n": state = "code"; out.append("\n")
            else: out.append(" ")
            i += 1; continue
        if state == "block":
            if c == "*" and n == "/":
                state = "code"; out.extend("  "); i += 2
            else:
                out.append("\n" if c == "\n" else " "); i += 1
            continue
        if state == "literal":
            if c == "\\": out.extend("  "); i += 2; continue
            if c == quote: state = "code"
            out.append("\n" if c == "\n" else " "); i += 1
    return "".join(out)


def check(name: str, passed: bool, actual: object = "", expected: object = "") -> dict[str, object]:
    return {"name": name, "passed": bool(passed), "actual": actual, "expected": expected}


def main() -> int:
    checks: list[dict[str, object]] = []
    app = (ROOT / "app.h").read_text(encoding="utf-8")
    transport_types = (ROOT / "core_transport_types.h").read_text(encoding="utf-8")
    transport = (ROOT / "core_transport.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "core1_output_engine.cpp").read_text(encoding="utf-8")
    runtime = (ROOT / "core1_runtime.cpp").read_text(encoding="utf-8")
    command = (ROOT / "core_command.cpp").read_text(encoding="utf-8")
    http = (ROOT / "http_handlers.cpp").read_text(encoding="utf-8")
    scpi = (ROOT / "scpi_server.cpp").read_text(encoding="utf-8")
    setup = (ROOT / "setup_loop.cpp").read_text(encoding="utf-8")

    checks.append(check("firmware version 0.6.0", 'FIRMWARE_VERSION = "0.6.0"' in app))
    checks.append(check("fixed-size command queue", "CORE_COMMAND_QUEUE_DEPTH" in transport_types and "CoreCommand s_commandQueue" in transport))
    checks.append(check("fixed-size result queue", "CORE_RESULT_QUEUE_DEPTH" in transport_types and "CoreResult s_resultQueue" in transport))
    checks.append(check("command sequence field", "uint32_t sequence;" in transport_types))
    checks.append(check("command deadline field", "uint32_t deadlineAtUs;" in transport_types))
    checks.append(check("command safety generation field", "uint32_t safetyGeneration;" in transport_types))
    checks.append(check("numeric result without message buffer", "CoreResponseStatus status;" in transport_types and "char message" not in transport_types))
    checks.append(check("result semaphore notification", "semaphore_t s_resultReady" in transport and "sem_acquire_timeout_ms" in transport))
    checks.append(check("startup semaphore synchronization", "semaphore_t s_core1Ready" in transport and "coreTransportWaitForCore1Ready" in command))
    checks.append(check("atomic output snapshot lock", "s_stateLock" in transport and "coreTransportPublishSnapshot" in transport and "coreTransportReadSnapshot" in transport))
    checks.append(check("immutable double-buffer policy", "CORE_POLICY_SLOT_COUNT = 2" in transport_types and "s_policySlots" in transport))
    checks.append(check("policy staging requires safe OFF snapshot", "CORE_SNAPSHOT_OUTPUTS_SAFE" in transport and "state.masks[ch] != 0U" in transport))
    checks.append(check("Core1 rejects expired commands before dispatch", "deadlineExpired" in engine and "CORE_RESP_EXPIRED" in engine and engine.index("validateEnvelope") < engine.index("switch (command.type)")))
    checks.append(check("Core1 rejects invalidated generation before dispatch", "CORE_RESP_GENERATION_MISMATCH" in engine and "coreTransportCurrentGeneration" in engine))
    checks.append(check("timeout invalidates generation", "coreTransportRecordTimeout" in command and "coreTransportInvalidateGeneration();" in command))
    checks.append(check("timeout requests direct all-off", "emergencyOffRequested = true" in command))
    checks.append(check("Core0 heartbeat fail-safe", "coreTransportCore0HeartbeatExpired" in engine and "CORE1_EVT_CORE0_FAILSAFE" in engine))
    checks.append(check("Core1 physical engine isolated", '#include "core1_engine.h"' in (ROOT / "shift_registers.cpp").read_text()))
    checks.append(check("Core1 status globals have a single Core1 writer", "core1OutputsReady =" not in command and "core1Fault =" not in command))
    checks.append(check("Core0-only setup loop", "void setup1()" not in setup and "void loop1()" not in setup))
    checks.append(check("no fixed Core1 startup delay", "delay(20" not in runtime and "coreTransportWaitUntilInitialized" in runtime))
    checks.append(check("HTTP exposes snapshot diagnostics", "core_snapshot_sequence=" in http and "core_transport_generation=" in http))
    checks.append(check("SCPI exposes transport diagnostics", "SYST:CORE:TRANSPORT?" in scpi))
    checks.append(check("SCPI exposes coherent snapshot", "SYST:CORE:SNAPSHOT?" in scpi and "snapshot_sequence=" in scpi))
    checks.append(check("test hooks compile-time guarded", "#ifdef ERESISTOR_TEST_MODE" in scpi and "#ifdef ERESISTOR_TEST_MODE" in engine))
    checks.append(check("test hooks require outputs OFF", "outputs_must_be_off" in scpi))
    checks.append(check("test duration is strictly bounded", "parsed < 1UL || parsed > 5000UL" in scpi and "duration_1_to_5000_ms" in scpi))
    checks.append(check("queued-command generation invalidation hook", "s_testInvalidateNextCommand" in engine and "core1TestInvalidateNextCommandGeneration" in engine and "SYST:TEST:CORE1:INVALIDATE:NEXT" in scpi))
    checks.append(check("queued invalidation occurs before envelope validation", engine.index("if (s_testInvalidateNextCommand)") < engine.index("if (!validateEnvelope(command")))
    checks.append(check("production build is default", "-TestBuild" not in (ROOT / "build_firmware.bat").read_text(errors="ignore")))
    checks.append(check("separate explicit Gate3 test build", (ROOT / "build_firmware_gate3_test.bat").exists()))

    prohibited = [r"\bString\b", r"\bLittleFS\b", r"\bWebServer\b", r"\bWiFi(?:Client|Server)?\b", r"\bSerial\b", r"\bsnprintf\b"]
    core1_violations: list[str] = []
    for path in CORE1_FILES:
        code = strip_comments_and_literals(path.read_text(encoding="utf-8"))
        for pattern in prohibited:
            if re.search(pattern, code):
                core1_violations.append(f"{path.name}:{pattern}")
    checks.append(check("Core1 code has no UI/storage/protocol/formatting dependencies", not core1_violations, core1_violations, []))

    legacy_patterns = ["CoreResponse ", "requestId", "CORE_CMD_GET_MASK", "CORE_CMD_GET_ALL", "forceAllOffPhysical(", "applyChannelMaskPhysical("]
    source_text = "\n".join(path.read_text(encoding="utf-8") for path in ROOT.glob("*.cpp"))
    legacy_hits = [pattern for pattern in legacy_patterns if pattern in source_text]
    checks.append(check("legacy command protocol removed", not legacy_hits, legacy_hits, []))

    summary = {
        "gate": "G3",
        "firmware_version": "0.6.0",
        "passed": sum(bool(item["passed"]) for item in checks),
        "failed": sum(not bool(item["passed"]) for item in checks),
        "total": len(checks),
        "checks": checks,
    }
    VALIDATION.mkdir(exist_ok=True)
    (VALIDATION / "gate3_source_check.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    for item in checks:
        print(("PASS" if item["passed"] else "FAIL"), "-", item["name"], item["actual"] if item["actual"] != "" else "")
    print(f"Summary: {summary['passed']} passed, {summary['failed']} failed")
    return 0 if summary["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
