from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


SOURCE_SUFFIXES = {".cpp", ".h", ".ino", ".md"}


def collect_source_files(source_dir: Path) -> list[Path]:
    return sorted(path for path in source_dir.rglob("*") if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES)


def count_lines(path: Path) -> int:
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return sum(1 for _ in handle)


def scan_source(source_dir: Path) -> dict[str, Any]:
    files = collect_source_files(source_dir)
    combined_parts: list[str] = []
    per_file: dict[str, Any] = {}
    total_lines = 0
    for path in files:
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = str(path.relative_to(source_dir))
        lines = text.count("\n") + (1 if text else 0)
        total_lines += lines
        combined_parts.append(text)
        per_file[rel] = {
            "lines": lines,
            "bytes": path.stat().st_size,
            "string_tokens": len(re.findall(r"\bString\b", text)),
            "serial_flush": text.count("Serial.flush"),
            "serial_calls": len(re.findall(r"\bSerial\.(?:print|println|flush)\b", text)),
        }

    combined = "\n".join(combined_parts)
    # Core 1 production hardware code is primarily in shift_registers.cpp.
    # For setup_loop.cpp, inspect only setup1()/loop1() rather than counting
    # Core 0 setup/loop Serial diagnostics.
    core1_text_parts: list[str] = []
    shift_path = source_dir / "shift_registers.cpp"
    if shift_path.exists():
        core1_text_parts.append(shift_path.read_text(encoding="utf-8", errors="replace"))
    setup_path = source_dir / "setup_loop.cpp"
    if setup_path.exists():
        setup_text = setup_path.read_text(encoding="utf-8", errors="replace")
        for function_name in ("setup1", "loop1"):
            match = re.search(rf"void\s+{function_name}\s*\([^)]*\)\s*\{{", setup_text)
            if not match:
                continue
            start = match.start()
            depth = 0
            end = len(setup_text)
            for index in range(match.end() - 1, len(setup_text)):
                char = setup_text[index]
                if char == "{":
                    depth += 1
                elif char == "}":
                    depth -= 1
                    if depth == 0:
                        end = index + 1
                        break
            core1_text_parts.append(setup_text[start:end])
    core1_text = "\n".join(core1_text_parts)
    core1_serial_flush = core1_text.count("Serial.flush")
    core1_serial_calls = len(re.findall(r"\bSerial\.(?:print|println|flush)\b", core1_text))

    mutation_get_routes = re.findall(
        r'server\.on\("(?P<path>/[^"]+)",\s*HTTP_GET,\s*(?P<handler>[^)]+)\)', combined
    )
    known_mutations = {
        "/set", "/toggle_bit", "/alloff", "/profile_apply", "/profile_delete",
        "/target_apply", "/factory_reset"
    }
    mutation_get_routes = [path for path, _ in mutation_get_routes if path in known_mutations]

    return {
        "source_dir": str(source_dir),
        "file_count": len(files),
        "total_lines": total_lines,
        "total_bytes": sum(path.stat().st_size for path in files),
        "app_h_lines": per_file.get("app.h", {}).get("lines"),
        "http_handlers_lines": per_file.get("http_handlers.cpp", {}).get("lines"),
        "string_token_count": len(re.findall(r"\bString\b", combined)),
        "serial_flush_count": combined.count("Serial.flush"),
        "core1_serial_flush_count": core1_serial_flush,
        "core1_serial_call_count": core1_serial_calls,
        "logf_count": len(re.findall(r"\blogf\s*\(", combined)),
        "update_write_stream_count": combined.count("Update.writeStream"),
        "mutation_get_routes": mutation_get_routes,
        "per_file": per_file,
    }


def evaluate_gate_expectations(metrics: dict[str, Any], source_dir: Path, gate: str,
                               manifest_path: Path | None = None) -> list[dict[str, Any]]:
    checks: list[dict[str, Any]] = []
    manifest: dict[str, Any] = {}
    if manifest_path and manifest_path.exists():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    cfg = manifest.get(gate, {})
    combined = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in collect_source_files(source_dir))

    maximum = cfg.get("max_source_serial_flush_core1")
    if maximum is not None:
        actual = metrics["core1_serial_flush_count"]
        checks.append({
            "name": "Core 1 Serial.flush limit",
            "passed": actual <= maximum,
            "actual": actual,
            "expected": f"<= {maximum}",
        })

    max_http = cfg.get("max_http_handlers_lines")
    if max_http is not None:
        actual = metrics.get("http_handlers_lines") or 0
        checks.append({
            "name": "http_handlers.cpp line limit",
            "passed": actual <= max_http,
            "actual": actual,
            "expected": f"<= {max_http}",
        })

    for symbol in cfg.get("required_symbols", []):
        checks.append({
            "name": f"Required source marker: {symbol}",
            "passed": symbol in combined,
            "actual": symbol in combined,
            "expected": True,
        })

    for symbol in cfg.get("forbidden_symbols", []):
        checks.append({
            "name": f"Forbidden source marker: {symbol}",
            "passed": symbol not in combined,
            "actual": symbol in combined,
            "expected": False,
        })

    return checks
