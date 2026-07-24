#!/usr/bin/env python3
"""Generate deterministic Gate 5 service/refactor model checks.

This is a host-side protocol/source oracle. It does not replace an Arduino-Pico
compile or the real HTTP/SCPI/HIL acceptance run.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "validation" / "gate5_service_oracle.json"


def normalize(raw: str, limit: int = 160) -> str | None:
    normalized = " ".join(raw.split()).upper()
    if not normalized or len(normalized) + 1 > limit:
        return None
    return normalized


def parse_channel(command: str) -> tuple[int, str] | None:
    p = command.lstrip(":")
    if p.startswith("ROUTE:"):
        p = p[6:]
    elif p.startswith("ROUT:"):
        p = p[5:]
    for prefix in ("CHANNEL", "CHAN", "CH"):
        if p.startswith(prefix):
            p = p[len(prefix):]
            break
    else:
        return None
    match = re.match(r"(\d+)", p)
    if not match:
        return None
    channel = int(match.group(1))
    if not 1 <= channel <= 8:
        return None
    tail = p[match.end():].lstrip(": ")
    return channel - 1, tail


def result(name: str, passed: bool, details: dict | None = None) -> dict:
    return {"name": name, "passed": bool(passed), "details": details or {}}


def main() -> int:
    registry = (ROOT / "scpi_command_registry.cpp").read_text(encoding="utf-8")
    routes = (ROOT / "http_routes.cpp").read_text(encoding="utf-8")
    diagnostics = (ROOT / "http_diagnostics.cpp").read_text(encoding="utf-8")
    api = (ROOT / "http_api_v1.cpp").read_text(encoding="utf-8")
    calibration = (ROOT / "http_calibration_page.cpp").read_text(encoding="utf-8")

    normalization_cases = {
        " *idn? ": "*IDN?",
        "system:status?": "SYSTEM:STATUS?",
        "  outp:all    off  ": "OUTP:ALL OFF",
        "route:channel8:mask?": "ROUTE:CHANNEL8:MASK?",
    }
    normalization_actual = {key: normalize(key) for key in normalization_cases}

    channel_cases = {
        "CH1:MASK?": (0, "MASK?"),
        "CHAN8:RES?": (7, "RES?"),
        "CHANNEL3:TARGET:CALC? 1000": (2, "TARGET:CALC? 1000"),
        "ROUT:CH4:MASK 0001": (3, "MASK 0001"),
        "ROUTE:CHANNEL2:CONF?": (1, "CONF?"),
    }
    channel_actual = {key: parse_channel(key) for key in channel_cases}
    invalid_channels = ["CH0:MASK?", "CH9:MASK?", "ROUT:BAD1:MASK?", "CH:MASK?"]

    registry_rows = re.findall(
        r'\{"([^"]+)",\s*ScpiCommandId::(\w+),\s*(nullptr|"[^"]*"),\s*(true|false)\}',
        registry,
    )
    exact_rows = [(cmd, command_id) for cmd, command_id, _help, _alias in registry_rows if command_id != "Unknown"]
    exact_commands = [cmd for cmd, _ in exact_rows]
    canonical_help = [cmd for cmd, _id, help_text, alias in registry_rows if alias == "false" and help_text != "nullptr"]

    all_post_paths = re.findall(r'server\.on\("([^"]+)", HTTP_POST', routes)
    mutation_paths = [path for path in all_post_paths if not path.startswith("/api/v1/")]
    rejected_gets = re.findall(r'server\.on\("([^"]+)", HTTP_GET, handleMethodNotAllowed\)', routes)
    v1_paths = re.findall(r'server\.on\("(/api/v1/[^"]+)"', routes)

    legacy_keys = [
        "status=", "firmware_version=", "test_mode=", "heap_free_bytes=",
        "core1_state=", "core_transport_generation=", "core_profile_transition_count=",
        "calibration_saved_mask=", "target_search_last_candidates=", "configured_ip=",
        "safety_ch%u_min_ohm=", "last_scpi_command=", "ch%u=%04X resistance=%s count=%lu",
    ]
    api_handlers = [
        "handleApiV1Health", "handleApiV1State", "handleApiV1Channels", "handleApiV1Diagnostics",
    ]

    checks = [
        result("SCPI whitespace/case normalization", normalization_actual == normalization_cases,
               {"expected": normalization_cases, "actual": normalization_actual}),
        result("SCPI channel alias parsing", channel_actual == channel_cases,
               {"expected": channel_cases, "actual": channel_actual}),
        result("SCPI invalid channels rejected", all(parse_channel(value) is None for value in invalid_channels),
               {"commands": invalid_channels}),
        result("SCPI exact registry has no duplicate command", len(exact_commands) == len(set(exact_commands)),
               {"exact_count": len(exact_commands), "unique_count": len(set(exact_commands))}),
        result("SCPI registry has canonical help coverage", len(canonical_help) >= 20,
               {"canonical_help_count": len(canonical_help), "commands": canonical_help}),
        result("HTTP mutation POST/GET rejection sets match", set(mutation_paths) <= set(rejected_gets),
               {"post_paths": mutation_paths, "rejected_gets": rejected_gets}),
        result("API v1 route set is non-empty and unique", len(v1_paths) >= 10 and len(v1_paths) == len(set(v1_paths)),
               {"count": len(v1_paths), "paths": v1_paths}),
        result("Legacy state compatibility keys retained", all(key in diagnostics for key in legacy_keys),
               {"keys": legacy_keys}),
        result("Focused API handler schemas present", all(name in api for name in api_handlers) and api.count("schema_version") >= 4,
               {"handlers": api_handlers, "schema_markers": api.count("schema_version")}),
        result("Calibration page fixed-buffer allocation model", "BUFFER_SIZE" not in calibration and "HttpResponseWriter out" in calibration,
               {"baseline_whole_string_reserve_bytes": 24000, "stream_buffer_bytes": 4096,
                "source_allocation_reduction_percent": round((1 - 4096 / 24000) * 100, 3)}),
    ]

    payload = {
        "gate": "G5",
        "firmware_version": "0.8.0",
        "passed": sum(item["passed"] for item in checks),
        "failed": sum(not item["passed"] for item in checks),
        "total": len(checks),
        "checks": checks,
        "limitations": [
            "The parser checks model the source algorithm but do not execute on RP2040.",
            "The source allocation reduction compares the old 24000-byte reserve to the fixed 4096-byte static stream buffer; connected heap telemetry is authoritative.",
            "HTTP latency, persistent heap drift and physical output behavior require Robot Framework HIL.",
        ],
    }
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    for item in checks:
        print(("PASS" if item["passed"] else "FAIL"), "-", item["name"])
    print(f"Summary: {payload['passed']} passed, {payload['failed']} failed")
    return 0 if payload["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
