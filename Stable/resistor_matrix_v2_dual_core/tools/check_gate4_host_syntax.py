#!/usr/bin/env python3
"""Run host g++ syntax-only checks with minimal Arduino/Pico stubs.

This validates C++ structure only. It does not compile or link for RP2040.
"""
from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STUBS = ROOT / "tools" / "host_syntax_stubs"
OUT = ROOT / "validation" / "gate4_host_cpp_syntax_validation.json"


def run_command(name: str, command: list[str]) -> dict:
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    return {
        "file": name,
        "passed": completed.returncode == 0,
        "returncode": completed.returncode,
        "stdout": completed.stdout[-2000:],
        "stderr": completed.stderr[-4000:],
    }


def main() -> int:
    compiler = shutil.which("g++")
    if compiler is None:
        payload = {
            "gate": "G4",
            "passed": 0,
            "failed": 1,
            "failures": ["g++ was not found"],
            "limitations": "This is not an Arduino-Pico target compile or link.",
        }
        OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print("FAIL - g++ was not found")
        return 1

    common = [compiler, "-std=c++17", "-fsyntax-only", f"-I{STUBS}", f"-I{ROOT}"]
    records = []
    for name in (
        "core_transport.cpp",
        "core1_event_queue.cpp",
        "shift_registers.cpp",
        "core1_output_engine.cpp",
        "core1_runtime.cpp",
    ):
        records.append(run_command(name, common + [str(ROOT / name)]))

    with tempfile.TemporaryDirectory(prefix="e_resistor_g4_syntax_") as tmp:
        focused = Path(tmp) / "core_command.cpp"
        source = (ROOT / "core_command.cpp").read_text(encoding="utf-8")
        source = source.replace('#include "app.h"', '#include "app_stub.h"', 1)
        focused.write_text(source, encoding="utf-8")
        command = common + [f"-I{STUBS}", str(focused)]
        records.append(run_command("core_command.cpp (focused stub)", command))

    payload = {
        "gate": "G4",
        "compiler": compiler,
        "passed": sum(bool(item["passed"]) for item in records),
        "failed": sum(not bool(item["passed"]) for item in records),
        "records": records,
        "limitations": "This is not an Arduino-Pico target compile or link.",
    }
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    for item in records:
        print(("PASS" if item["passed"] else "FAIL"), "-", item["file"])
    return 0 if payload["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
