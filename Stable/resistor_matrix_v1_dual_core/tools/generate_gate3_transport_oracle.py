#!/usr/bin/env python3
"""Deterministic host oracle for Gate 3 sequence/deadline/generation invariants."""
from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "validation" / "gate3_transport_oracle.json"


@dataclass
class Model:
    generation: int = 1
    policy_generation: int = 1
    policy_installed: bool = True
    mask: int = 0
    snapshot_sequence: int = 1
    last_command_sequence: int = 0
    expired: int = 0
    generation_rejects: int = 0
    actuations: int = 0

    def invalidate(self) -> None:
        self.generation += 1

    def timeout(self) -> None:
        self.invalidate()
        self.mask = 0
        self.snapshot_sequence += 1

    def install_policy(self) -> None:
        assert self.mask == 0
        self.policy_generation = self.generation
        self.policy_installed = True
        self.snapshot_sequence += 1

    def command(self, *, sequence: int, generation: int, deadline: int, now: int, mask: int) -> str:
        self.last_command_sequence = sequence
        if now >= deadline:
            self.expired += 1
            self.snapshot_sequence += 1
            return "EXPIRED"
        if generation != self.generation:
            self.generation_rejects += 1
            self.snapshot_sequence += 1
            return "GENERATION_MISMATCH"
        if not self.policy_installed or self.policy_generation != generation:
            self.snapshot_sequence += 1
            return "POLICY_NOT_INSTALLED"
        self.mask = mask
        self.actuations += 1
        self.snapshot_sequence += 1
        return "OK"


def run() -> dict:
    model = Model()
    cases: list[dict] = []

    def case(name, action, expected, actuation_delta=None):
        before = model.actuations
        actual = action()
        delta = model.actuations - before
        passed = actual == expected and (actuation_delta is None or delta == actuation_delta)
        cases.append({
            "name": name,
            "expected": expected,
            "actual": actual,
            "actuation_delta": delta,
            "passed": passed,
        })

    case(
        "valid command actuates",
        lambda: model.command(sequence=1, generation=1, deadline=1000, now=100, mask=1),
        "OK", 1,
    )

    queued_generation = model.generation
    model.invalidate()
    case(
        "queued command invalidated before validation never actuates",
        lambda: model.command(sequence=2, generation=queued_generation, deadline=5000, now=200, mask=2),
        "GENERATION_MISMATCH", 0,
    )
    case(
        "new generation requires policy install",
        lambda: model.command(sequence=3, generation=model.generation, deadline=5000, now=200, mask=4),
        "POLICY_NOT_INSTALLED", 0,
    )
    model.mask = 0
    model.install_policy()
    case(
        "expired command never actuates",
        lambda: model.command(sequence=4, generation=model.generation, deadline=100, now=100, mask=4),
        "EXPIRED", 0,
    )

    timed_out_generation = model.generation
    model.timeout()
    case(
        "late command after Core0 timeout never actuates",
        lambda: model.command(sequence=5, generation=timed_out_generation, deadline=5000, now=200, mask=8),
        "GENERATION_MISMATCH", 0,
    )
    case(
        "post-timeout generation requires policy install",
        lambda: model.command(sequence=6, generation=model.generation, deadline=5000, now=200, mask=8),
        "POLICY_NOT_INSTALLED", 0,
    )
    model.install_policy()
    case(
        "post-recovery command actuates",
        lambda: model.command(sequence=7, generation=model.generation, deadline=5000, now=200, mask=8),
        "OK", 1,
    )

    snapshot = {
        "snapshot_sequence": model.snapshot_sequence,
        "last_command_sequence": model.last_command_sequence,
        "generation": model.generation,
        "mask": model.mask,
    }
    coherent = (
        snapshot["snapshot_sequence"] >= snapshot["last_command_sequence"]
        and snapshot["generation"] == model.policy_generation
    )
    cases.append({
        "name": "coherent snapshot tuple",
        "actual": snapshot,
        "expected": "monotonic/current generation",
        "passed": coherent,
    })

    return {
        "gate": "G3",
        "passed": sum(bool(item["passed"]) for item in cases),
        "failed": sum(not bool(item["passed"]) for item in cases),
        "cases": cases,
        "final_model": asdict(model),
    }


if __name__ == "__main__":
    result = run()
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["failed"] == 0 else 1)
