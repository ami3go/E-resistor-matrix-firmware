from __future__ import annotations

import re
import subprocess
from pathlib import Path
from typing import Any


SIZE_PATTERNS = [
    re.compile(r"Sketch uses\s+([\d,]+) bytes.*Maximum is\s+([\d,]+) bytes", re.IGNORECASE),
    re.compile(r"Program storage space.*?([\d,]+) bytes", re.IGNORECASE),
]
RAM_PATTERNS = [
    re.compile(r"Global variables use\s+([\d,]+) bytes.*maximum is\s+([\d,]+) bytes", re.IGNORECASE),
]


def _int(value: str) -> int:
    return int(value.replace(",", ""))


def run_arduino_build(arduino_cli: str, fqbn: str, source_dir: Path, output_dir: Path) -> dict[str, Any]:
    build_dir = output_dir / "arduino_build"
    build_dir.mkdir(parents=True, exist_ok=True)
    command = [
        arduino_cli, "compile",
        "--fqbn", fqbn,
        "--build-path", str(build_dir),
        "--warnings", "all",
        str(source_dir),
    ]
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding="utf-8", errors="replace", check=False)
    log_text = completed.stdout
    (output_dir / "build.log").write_text(log_text, encoding="utf-8")

    result: dict[str, Any] = {
        "command": command,
        "returncode": completed.returncode,
        "passed": completed.returncode == 0,
        "warnings": len(re.findall(r"\bwarning:", log_text, re.IGNORECASE)),
        "errors": len(re.findall(r"\berror:", log_text, re.IGNORECASE)),
    }
    for pattern in SIZE_PATTERNS:
        match = pattern.search(log_text)
        if match:
            result["firmware_bytes"] = _int(match.group(1))
            if match.lastindex and match.lastindex >= 2:
                result["firmware_max_bytes"] = _int(match.group(2))
            break
    for pattern in RAM_PATTERNS:
        match = pattern.search(log_text)
        if match:
            result["global_ram_bytes"] = _int(match.group(1))
            result["global_ram_max_bytes"] = _int(match.group(2))
            break
    return result
