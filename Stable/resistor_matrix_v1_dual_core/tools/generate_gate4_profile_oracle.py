#!/usr/bin/env python3
"""Host model oracle for Gate 4 two-phase profile invariants."""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "validation" / "gate4_profile_oracle.json"


@dataclass
class Model:
    physical: list[int] = field(default_factory=lambda: [0] * 8)
    published: list[int] = field(default_factory=lambda: [0] * 8)
    snapshots: list[list[int]] = field(default_factory=lambda: [[0] * 8])
    bbm_count: int = 0
    transitions: int = 0
    failures: int = 0

    def publish(self) -> None:
        self.published = self.physical.copy()
        self.snapshots.append(self.published.copy())

    def apply(self, target: list[int], fail_after_clear: bool = False, fail_make_at: int | None = None) -> bool:
        assert len(target) == 8
        # phase 1: clear all physical outputs; do not publish the intermediate state
        self.physical = [0] * 8
        self.bbm_count += 1
        if fail_after_clear:
            self.failures += 1
            self.publish()
            return False
        # phase 2: make sequentially; still do not publish intermediate state
        for index, mask in enumerate(target):
            self.physical[index] = mask
            if fail_make_at == index:
                self.physical = [0] * 8
                self.failures += 1
                self.publish()
                return False
        self.transitions += 1
        self.publish()
        return True


def main() -> int:
    model = Model()
    cases = []

    target = [1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80]
    before_snapshot_count = len(model.snapshots)
    ok = model.apply(target)
    cases.append({
        "name": "successful profile publishes only final tuple",
        "passed": ok and model.published == target and len(model.snapshots) == before_snapshot_count + 1,
    })

    old = model.published.copy()
    ok = model.apply([0] * 8, fail_after_clear=True)
    cases.append({
        "name": "failure after clear publishes all off",
        "passed": (not ok) and model.published == [0] * 8 and model.physical == [0] * 8,
    })

    ok = model.apply(target, fail_make_at=3)
    cases.append({
        "name": "failure during make rolls back all off",
        "passed": (not ok) and model.published == [0] * 8 and model.physical == [0] * 8,
    })

    before_bbm = model.bbm_count
    for _ in range(1000):
        assert model.apply([0] * 8)
    cases.append({
        "name": "one global BBM per profile",
        "passed": model.bbm_count - before_bbm == 1000,
    })

    coherent = all(len(s) == 8 for s in model.snapshots)
    cases.append({"name": "all published snapshots are complete eight-channel tuples", "passed": coherent})

    payload = {
        "gate": "G4",
        "passed": sum(c["passed"] for c in cases),
        "failed": sum(not c["passed"] for c in cases),
        "cases": cases,
        "final": {
            "physical": model.physical,
            "published": model.published,
            "bbm_count": model.bbm_count,
            "transitions": model.transitions,
            "failures": model.failures,
            "snapshot_count": len(model.snapshots),
        },
    }
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(json.dumps(payload, indent=2))
    return 0 if payload["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
