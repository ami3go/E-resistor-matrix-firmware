#!/usr/bin/env python3
"""Offline structural validation for E-Resistor optimization Gate 5 v0.8.0."""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "validation" / "gate5_source_check.json"

G4_CORE_HASHES = {
    "core_transport_types.h": "fdc7c0e27a5bbfc5575c84273f8139c2c3f391f0bb6bf418f617ce7dd682d03c",
    "core_transport.cpp": "ba9498918b9bd31848caa9333daa1d2894c81647b6960bed8cac0ba7921eb7d3",
    "core1_engine.h": "2f2225e106881dfe03ab7dbc88d89bac2adfa767f6672624a9c3dfe0ee666c9a",
    "core1_event_queue.cpp": "839183a213e7c75d749f32aca7f9c04eef0827a55b048c542a94fff7e2790098",
    "core1_output_engine.cpp": "069f1cc7da338f84fd5ce3eba75ed90e320e4eae37ab56e48d5acdb0602e8cc9",
    "core1_runtime.cpp": "ac1f8cb2f0757d62d508ff01920db5814257a102ecb8d7b2c90313913eea00e6",
    "shift_registers.cpp": "0b3a96cb51c5b3c4f6d589203f690294783c4e2191fe5e16ca9a65afe7fa10a4",
    "core_command.cpp": "82df52cb6926011d1974548b11da9bf3d7491a3b71c574d65c0a1162ad217d67",
    "setup_loop.cpp": "71bd42f46392ecf41dd787d5b61dbf391fa1866610748ab3b6368042e6ed52c4",
}


def read(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def check(name: str, passed: bool, actual=None, expected=None) -> dict:
    return {"name": name, "passed": bool(passed), "actual": actual, "expected": expected}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    identity = read("firmware_identity.h")
    app = read("app.h")
    routes = read("http_routes.cpp")
    static_assets = read("http_static_assets.cpp")
    response_writer = read("http_response_writer.cpp")
    calibration = read("http_calibration_page.cpp")
    diagnostics = read("http_diagnostics.cpp")
    api = read("http_api_v1.cpp")
    maintenance = read("http_maintenance.cpp")
    scpi_registry = read("scpi_command_registry.cpp")
    scpi_server = read("scpi_server.cpp")
    scpi_page = read("http_pages.cpp")
    page_helpers = read("http_page_helpers.cpp")
    http_header = read("http_api.h")

    required_modules = [
        "http_control.cpp", "http_calibration_page.cpp", "http_maintenance.cpp",
        "http_diagnostics.cpp", "http_api_v1.cpp", "http_response_writer.cpp",
        "http_page_helpers.cpp", "http_static_assets.cpp", "http_routes.cpp",
    ]
    mutation_paths = [
        "/set", "/toggle_bit", "/alloff", "/target_apply", "/identify_led",
        "/profile_save", "/profile_apply", "/profile_delete", "/factory_reset",
        "/safety_save", "/network_save", "/network_delete", "/file_delete",
        "/meta_save", "/meta_delete", "/upload_config", "/calibration_import_all",
        "/firmware_update",
    ]
    api_paths = [
        "/api/v1/health", "/api/v1/state", "/api/v1/channels",
        "/api/v1/diagnostics", "/api/v1/calibration/files",
        "/api/v1/calibration/download", "/api/v1/calibration/download_all",
        "/api/v1/control/channel", "/api/v1/control/all_off",
        "/api/v1/control/profile", "/api/v1/control/target",
        "/api/v1/control/toggle", "/api/v1/identify",
    ]

    checks = [
        check("firmware version 0.8.0", 'FIRMWARE_VERSION = "0.8.0"' in identity),
        check("API version v1", 'API_VERSION = "v1"' in identity),
        check("focused app umbrella", len(app.splitlines()) <= 20, len(app.splitlines()), "<=20 lines"),
        check("focused application headers exist", all((ROOT / n).exists() for n in (
            "platform.h", "firmware_identity.h", "firmware_types.h", "runtime_state.h",
            "utility_api.h", "calibration_api.h", "core_api.h", "http_api.h", "scpi_api.h",
        ))),
        check("HTTP service split modules", all((ROOT / n).exists() for n in required_modules), required_modules),
        check("monolithic HTTP module removed", not (ROOT / "http_handlers.cpp").exists()),
        check("flash-backed CSS", "kApplicationCss[] PROGMEM" in static_assets),
        check("flash-backed JavaScript", "kApplicationJs[] PROGMEM" in static_assets),
        check("static assets cacheable", "max-age=86400" in static_assets and "immutable" in static_assets),
        check("pages reference static assets", "/assets/app.css?v=" in page_helpers and "/assets/app.js?v=" in page_helpers),
        check("no inline event handlers", not any(token in "\n".join(read(n) for n in required_modules + ["http_pages.cpp", "littlefs_page_helpers.cpp"]) for token in ("onsubmit=", "onclick=", "onchange="))),
        check("fixed 512-byte response buffer", "BUFFER_SIZE = 512" in http_header),
        check("shared calibration bundle limit",
              "inline constexpr size_t CALIBRATION_BUNDLE_MAX_BYTES = 16384U" in http_header
              and "static constexpr size_t CALIBRATION_BUNDLE_MAX_BYTES" not in maintenance),
        check("chunked response writer", "chunkedResponseModeStart" in response_writer and "chunkedResponseFinalize" in response_writer),
        check("calibration page streams", "HttpResponseWriter out" in calibration and "streamCombinedChannelResistorTable" in calibration),
        check("calibration page does not build full String", "String html" not in calibration and "reserve(24000)" not in calibration),
        check("calibration heap telemetry", "httpCalibrationPageLastTempBytes" in calibration and "GATE4_CALIBRATION_TEMP_HEAP_BASELINE_BYTES" in api),
        check("legacy state captures one snapshot", diagnostics.count("captureRuntimeStateSnapshot(snapshot)") == 1),
        check("legacy state streams", "HttpResponseWriter out" in diagnostics and "writeLegacyState(out, snapshot)" in diagnostics),
        check("state formatter uses snapshot masks", "s.masks[ch]" in diagnostics and "channelMask[ch]" not in diagnostics),
        check("fixed-buffer resistance formatting", "formatOutputResistanceText" in diagnostics and "formatOutputResistanceText" in scpi_server),
        check("large downloads stream", all(token in maintenance for token in ("void handleLogDownload()", "void handleBackupDownload()", "HttpResponseWriter out"))),
        check("all mutation routes are POST", all(f'server.on("{path}", HTTP_POST' in routes for path in mutation_paths), mutation_paths),
        check("all legacy mutation GETs reject", all(f'server.on("{path}", HTTP_GET, handleMethodNotAllowed)' in routes for path in mutation_paths), mutation_paths),
        check("method rejection advertises POST", 'server.sendHeader("Allow", "POST")' in routes and "405" in routes),
        check("canonical API v1 routes", all(path in routes for path in api_paths), api_paths),
        check("legacy calibration aliases retained", all(path in routes for path in (
            "/api/calibration/files", "/api/calibration/download", "/api/calibration/download_all",
        ))),
        check("focused API schemas", all(token in api for token in (
            'schema_version', 'api_version', 'void handleApiV1Health()', 'void handleApiV1State()',
            'void handleApiV1Channels()', 'void handleApiV1Diagnostics()',
        ))),
        check("SCPI fixed input buffer", "char normalized[160]" in scpi_server and "normalizeScpiLine" in scpi_server),
        check("SCPI exact command table", "kCommands[]" in scpi_registry and "scpiLookupExactCommand" in scpi_registry),
        check("SCPI fixed channel parser", "parseScpiChannelCommandFixed" in scpi_registry),
        check("SCPI help generated from registry", "scpiCommandRegistry(count)" in scpi_registry and "scpiPrintHelp" in scpi_registry),
        check("SCPI browser table generated from registry", "scpiCommandRegistry(commandCount)" in scpi_page),
        check("SCPI compatibility aliases registered", all(alias in scpi_registry for alias in (
            "SYSTEM:SERIAL?", "SYSTEM:VERSION?", "SYSTEM:STATUS?",
            "SYSTEM:CORE:TRANSPORT?", "SYSTEM:CORE:SNAPSHOT?", "OUTPUT:ALL OFF",
        ))),
        check("SCPI status compatibility fields preserved", all(field in scpi_server for field in (
            "core1_ready=", "transport_generation=", "profile_bbm_count=", "ch", "_max_bits=",
        ))),
        check("exact SCPI dispatch precedes String allocation", scpi_server.index("scpiLookupExactCommand") < scpi_server.index("String cmd(normalized)")),
        check("Core 1 and output engine unchanged from accepted G4", all(sha(ROOT / n) == h for n, h in G4_CORE_HASHES.items())),
    ]

    # The old 8-24 KB whole-response allocation patterns must be absent from the
    # streamed calibration and download modules.
    critical = calibration + diagnostics + maintenance
    large_reserves = re.findall(r"reserve\s*\(\s*(?:[89]\d{3}|1\d{4}|2\d{4})\s*\)", critical)
    checks.append(check("critical streamed modules have no 8-24 KB reserve", not large_reserves, large_reserves, []))

    payload = {
        "gate": "G5",
        "firmware_version": "0.8.0",
        "baseline_firmware": "0.7.2",
        "passed": sum(c["passed"] for c in checks),
        "failed": sum(not c["passed"] for c in checks),
        "total": len(checks),
        "checks": checks,
        "limitations": "Structural validation only; Arduino-Pico target build and connected Gate 5 HIL remain required.",
    }
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    for c in checks:
        print(("PASS" if c["passed"] else "FAIL"), "-", c["name"])
    print(f"Summary: {payload['passed']} passed, {payload['failed']} failed")
    return 0 if payload["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
