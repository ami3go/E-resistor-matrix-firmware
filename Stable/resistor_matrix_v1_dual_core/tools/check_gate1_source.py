#!/usr/bin/env python3
"""Static Gate G1 source checks. No Arduino toolchain is required."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE1_FILES = [
    ROOT / "shift_registers.cpp",
    ROOT / "core_command.cpp",
    ROOT / "core1_event_queue.cpp",
]


def extract_function(text: str, name: str) -> str:
    match = re.search(rf"void\s+{re.escape(name)}\s*\([^)]*\)\s*\{{", text)
    if not match:
        return ""
    depth = 0
    for index in range(match.end() - 1, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[match.start():index + 1]
    return text[match.start():]


def main() -> int:
    all_sources = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in ROOT.rglob("*")
        if path.suffix in {".cpp", ".h", ".ino"}
    )
    core1_text = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in CORE1_FILES)
    setup_text = (ROOT / "setup_loop.cpp").read_text(encoding="utf-8")
    core1_text += extract_function(setup_text, "setup1")
    core1_text += extract_function(setup_text, "loop1")

    checks = [
        ("firmware version", 'FIRMWARE_VERSION = "0.4.6"' in all_sources),
        ("no Core 1 Serial calls", re.search(r"\bSerial\.(?:print|println|flush)\b", core1_text) is None),
        ("compact Core 1 event queue", "struct Core1Event" in all_sources and "core1EmitEvent" in all_sources),
        ("verified bool forceAllOff", re.search(r"bool\s+forceAllOff\s*\(", all_sources) is not None),
        ("SCPI discard-until-newline", "scpiDiscardUntilNewline" in all_sources),
        ("separate Core 1 stack", "bool core1_separate_stack = true" in all_sources),
        ("Core 1 stack telemetry", "core1MinFreeStackBytes" in all_sources and "getFreeStack" in all_sources),
        ("central default IP", "DEFAULT_DEVICE_IP_TEXT = \"192.168.0.55\"" in all_sources),
        ("no obsolete split comments", "Split from" not in all_sources),
        ("channel constants in default table", "CHANNEL_DEFAULT_TABLES[CHANNEL_COUNT][BIT_COUNT]" in all_sources),
    ]
    result = {
        "gate": "G1",
        "passed": all(ok for _, ok in checks),
        "checks": [{"name": name, "passed": ok} for name, ok in checks],
    }
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
