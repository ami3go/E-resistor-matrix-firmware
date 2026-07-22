#!/usr/bin/env python3
"""Offline structural checks for E-Resistor optimization Gate 4 v0.7.2."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "validation" / "gate4_source_check.json"


def strip_comments_and_literals(text: str) -> str:
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
        if c == "\\": out.extend("  "); i += 2; continue
        if c == quote: state = "code"
        out.append("\n" if c == "\n" else " "); i += 1
    return "".join(out)


def item(name: str, passed: bool, actual=None, expected=None) -> dict:
    return {"name": name, "passed": bool(passed), "actual": actual, "expected": expected}


def main() -> int:
    app = (ROOT / "app.h").read_text(encoding="utf-8")
    types = (ROOT / "core_transport_types.h").read_text(encoding="utf-8")
    transport = (ROOT / "core_transport.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "core1_output_engine.cpp").read_text(encoding="utf-8")
    runtime = (ROOT / "core1_runtime.cpp").read_text(encoding="utf-8")
    command = (ROOT / "core_command.cpp").read_text(encoding="utf-8")
    shift = (ROOT / "shift_registers.cpp").read_text(encoding="utf-8")
    scpi = (ROOT / "scpi_server.cpp").read_text(encoding="utf-8")
    http = (ROOT / "http_handlers.cpp").read_text(encoding="utf-8")
    setup = (ROOT / "setup_loop.cpp").read_text(encoding="utf-8")

    checks = [
        item("firmware version 0.7.2", 'FIRMWARE_VERSION = "0.7.2"' in app),
        item("accepted Gate 3 startup order preserved",
             setup.index("initCoreCommandEngine()") < setup.index("Serial.begin(115200)") < setup.index("delay(100)")),
        item("event queue initializes once before transport publication",
             command.index("initCore1EventQueue()") < command.rindex("coreTransportInitialize()")),
        item("Core1 does not race event queue initialization", "initCore1EventQueue()" not in runtime),
        item("Core1 retries startup handshake", "while (!coreTransportWaitUntilInitialized(CORE_STARTUP_TIMEOUT_MS))" in runtime),
        item("cross-core publication uses memory barriers", transport.count("__dmb();") >= 2),
        item("persistent startup ready token", "CORE1_READY_TOKEN" in types and "s_core1ReadyToken" in transport),
        item("startup wait is level-triggered", "while (s_core1ReadyToken != CORE1_READY_TOKEN)" in transport),
        item("startup snapshot polling fallback", "refreshCore0OutputMirror()" in command and "readyTokenSeen" in command),
        item("startup stages published", "CORE1_STARTUP_LOOP_RUNNING" in types and "coreTransportSetCore1StartupStage" in runtime),
        item("startup failure diagnostics", "Core 1 startup stage:" in setup and "Core 1 ready token:" in setup),
        item("profile diagnostics structure", all(token in types for token in (
            "profileTransitionCount", "profileFailureCount", "profileBreakBeforeMakeCount",
            "lastProfileDurationUs", "lastProfileClearDurationUs",
        ))),
        item("profile diagnostics lock protected", "coreTransportRecordProfileResult" in transport and "s_diagLock" in transport),
        item("all-mask command validates all channels before physical call",
             engine.index("case CORE_CMD_SET_ALL_MASKS") < engine.index("core1ApplyAllMasksPhysical")),
        item("profile starts with one shared zero shift", "shiftMaskPhysical(0U);" in shift),
        item("all latches clear before global wait",
             shift[shift.index("bool core1ApplyAllMasksPhysical"):shift.index("bool core1ForceAllOffPhysical")].index("pulseLatchPhysical(ch);") <
             shift[shift.index("bool core1ApplyAllMasksPhysical"):shift.index("bool core1ForceAllOffPhysical")].index("delay(BREAK_BEFORE_MAKE_MS);")),
        item("one global break-before-make delay",
             shift[shift.index("bool core1ApplyAllMasksPhysical"):shift.index("bool core1ForceAllOffPhysical")].count("delay(BREAK_BEFORE_MAKE_MS);") == 1),
        item("make phase follows global delay", shift.index("delay(BREAK_BEFORE_MAKE_MS);") < shift.index("// Phase 2:")),
        item("clear failure forces all off", "CORE_DETAIL_PROFILE_CLEAR_FAILED" in shift and
             "core1ForceAllOffPhysical(sequence, CORE_DETAIL_PROFILE_CLEAR_FAILED)" in shift),
        item("make failure forces all off", "CORE_DETAIL_PROFILE_MAKE_FAILED" in shift and
             "core1ForceAllOffPhysical(sequence, CORE_DETAIL_PROFILE_MAKE_FAILED)" in shift),
        item("intermediate profile states are not published", "coreTransportPublishSnapshot" not in shift),
        item("profile completion event", "CORE1_EVT_PROFILE_DONE" in shift),
        item("profile failure event", "CORE1_EVT_PROFILE_FAILED" in shift),
        item("SCPI profile diagnostics", "SYST:CORE:PROFILE?" in scpi and "profile_clear_last_us" in scpi),
        item("HTTP profile diagnostics", "core_profile_transition_count=" in http and "core_profile_last_clear_duration_us=" in http),
        item("test failure after clear is compile-time guarded", "#ifdef ERESISTOR_TEST_MODE" in shift and "s_testFailNextProfileAfterClear" in shift),
        item("test hook requires outputs off", "SYST:TEST:PROFILE:FAIL:NEXT" in scpi and "outputs_must_be_off" in scpi),
        item("separate Gate 4 test build", (ROOT / "build_firmware_gate4_test.bat").exists()),
        item("production build does not request test mode", "-TestBuild" not in (ROOT / "build_firmware.bat").read_text(errors="ignore")),
    ]

    profile_body = shift[shift.index("bool core1ApplyAllMasksPhysical"):shift.index("bool core1ForceAllOffPhysical")]
    prohibited = []
    for token in ("String", "LittleFS", "Serial", "snprintf", "WiFi"):
        if re.search(rf"\b{re.escape(token)}\b", strip_comments_and_literals(profile_body)):
            prohibited.append(token)
    checks.append(item("profile engine has no text/network/storage dependencies", not prohibited, prohibited, []))

    payload = {
        "gate": "G4",
        "firmware_version": "0.7.2",
        "baseline_firmware": "0.6.2",
        "passed": sum(c["passed"] for c in checks),
        "failed": sum(not c["passed"] for c in checks),
        "total": len(checks),
        "checks": checks,
        "limitations": "Structural validation only; Arduino-Pico target build and connected HIL remain required.",
    }
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    for c in checks:
        print(("PASS" if c["passed"] else "FAIL"), "-", c["name"])
    print(f"Summary: {payload['passed']} passed, {payload['failed']} failed")
    return 0 if payload["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
