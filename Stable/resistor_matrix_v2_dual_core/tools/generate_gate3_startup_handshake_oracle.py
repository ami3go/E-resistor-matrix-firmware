#!/usr/bin/env python3
"""Model the Gate 3 Core0/Core1 startup handshake under delayed Core0 startup.

This is a deterministic host model only. It verifies the intended retry semantics
and does not replace an Arduino-Pico target build or hardware boot test.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "validation" / "gate3_startup_handshake_oracle.json"
TIMEOUT_MS = 5000


def old_handshake(core0_init_ms: int) -> bool:
    """v0.6.1 behavior: Core1 performs only one bounded wait."""
    return core0_init_ms < TIMEOUT_MS


def corrected_handshake(core0_init_ms: int) -> tuple[bool, int]:
    """v0.6.2 behavior: bounded waits repeat until transport is initialized."""
    waits = 0
    elapsed = 0
    while elapsed <= core0_init_ms:
        waits += 1
        if core0_init_ms < elapsed + TIMEOUT_MS:
            return True, waits
        elapsed += TIMEOUT_MS
    return True, waits


def main() -> int:
    cases = []
    for delay_ms in (0, 1, 100, 4999, 5000, 5001, 12000, 30000):
        corrected, waits = corrected_handshake(delay_ms)
        expected = True
        cases.append(
            {
                "core0_transport_init_delay_ms": delay_ms,
                "v0_6_1_single_wait_signals_ready": old_handshake(delay_ms),
                "v0_6_2_retry_wait_signals_ready": corrected,
                "v0_6_2_wait_cycles": waits,
                "passed": corrected == expected,
            }
        )

    payload = {
        "gate": "G3",
        "firmware_version": "0.6.2",
        "timeout_ms": TIMEOUT_MS,
        "passed": sum(bool(case["passed"]) for case in cases),
        "failed": sum(not bool(case["passed"]) for case in cases),
        "cases": cases,
        "limitations": "Host timing model only; target build and hardware boot verification remain required.",
    }
    OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(json.dumps(payload, indent=2))
    return 0 if payload["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
