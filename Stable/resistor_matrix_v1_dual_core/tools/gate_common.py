#!/usr/bin/env python3
"""Shared helpers for the Gate G1-G5 static source checkers.

Three problems this module exists to solve:

1. **Layout drift.** Gate 3 and Gate 4 read `http_handlers.cpp` by name. That file
   was split into `http_*.cpp` during Gate 5, so both checkers raised
   FileNotFoundError and never reported a result at all. A checker that crashes is
   indistinguishable from one that was never run, so those gates had been failing
   *open* since the split. `http_layer()` and `scpi_layer()` address a logical
   layer instead of a specific filename.

2. **Version pins.** Each gate pinned an exact `FIRMWARE_VERSION` string (0.4.6,
   0.5.0, 0.6.2, 0.7.2). Every one of them fails permanently the moment the
   firmware moves on, which trains reviewers to ignore red checks. The intent was
   "this gate's criteria still hold at or above its baseline", which is what
   `version_at_least()` expresses.

3. **Silent crashes.** `run_gate()` converts any exception into an explicit failure
   with a non-zero exit code, so a broken checker is loud rather than invisible.
"""
from __future__ import annotations

import re
import sys
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SOURCE_SUFFIXES = {".cpp", ".h", ".ino"}


def read(name: str) -> str:
    """Read one source file relative to the sketch root."""
    return (ROOT / name).read_text(encoding="utf-8", errors="replace")


def _concat(paths) -> str:
    return "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(paths))


def all_sources() -> str:
    """Every firmware source file in the sketch root, concatenated."""
    return _concat(p for p in ROOT.glob("*") if p.suffix in SOURCE_SUFFIXES)


def http_layer() -> str:
    """The whole HTTP service layer.

    Replaces the pre-Gate-5 `http_handlers.cpp`, which was split into
    http_routes / http_pages / http_control / http_maintenance / http_api_v1 /
    http_compat_api / http_diagnostics / http_calibration_page / http_common /
    http_page_helpers / http_response_writer / http_static_assets.
    """
    return _concat(ROOT.glob("http_*.cpp")) + "\n" + read("http_api.h")


def scpi_layer() -> str:
    """The whole SCPI layer: parser, server and command registry.

    The exact command table moved from `scpi_server.cpp` into
    `scpi_command_registry.cpp` during Gate 5, so single-file lookups for command
    strings such as `SYST:DIAG:SERIAL?` began reporting absent capabilities that
    are in fact still present.
    """
    return _concat(ROOT.glob("scpi_*.cpp")) + "\n" + read("scpi_api.h")


def firmware_version() -> str:
    """Current FIRMWARE_VERSION string, or '' when it cannot be located."""
    match = re.search(r'FIRMWARE_VERSION\s*=\s*"([^"]+)"', read("firmware_identity.h"))
    return match.group(1) if match else ""


def _parts(version: str) -> tuple[int, ...]:
    return tuple(int(p) for p in re.findall(r"\d+", version))


def version_at_least(baseline: str) -> bool:
    """True when the current firmware version is >= the gate's baseline.

    Deliberately not an equality test: a gate's criteria must keep holding as the
    firmware advances, and an exact pin would make that gate fail forever.
    """
    current = firmware_version()
    return bool(current) and _parts(current) >= _parts(baseline)


def run_gate(main) -> None:
    """Run a gate's main(), converting any crash into a loud, non-zero failure."""
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception:
        gate = Path(sys.argv[0]).name
        print(f"FAIL - {gate} could not run to completion:", file=sys.stderr)
        traceback.print_exc()
        print(
            f"\n{gate}: CHECKER ERROR - this is a failure, not a skip. "
            "A gate that cannot execute has not been validated.",
            file=sys.stderr,
        )
        sys.exit(2)
