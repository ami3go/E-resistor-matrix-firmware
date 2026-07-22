#!/usr/bin/env python3
"""Model the Gate 4 v0.7.2 level-triggered Core 1 startup handshake."""
from __future__ import annotations
import json
from pathlib import Path

TOKEN = 0xC011CAFE


def core0_accepts(events: list[dict]) -> bool:
    token = 0
    snapshot_ready = False
    snapshot_safe = False
    for event in events:
        if event.get("token") is not None:
            token = int(event["token"])
        if event.get("snapshot") is not None:
            snapshot_ready, snapshot_safe = event["snapshot"]
        # Production implementation accepts a coherent safe/ready snapshot.
        if snapshot_ready and snapshot_safe:
            return True
        # Token is persistent, but never substitutes for a safe snapshot.
        if token == TOKEN and snapshot_ready and snapshot_safe:
            return True
    return False


def main() -> int:
    cases = [
        ("ready before Core0 starts waiting", [{"snapshot": (True, True)}, {"token": TOKEN}], True),
        ("token before snapshot", [{"token": TOKEN}, {"snapshot": (True, True)}], True),
        ("snapshot before token", [{"snapshot": (True, True)}, {"token": TOKEN}], True),
        ("safe snapshot survives missed edge", [{"snapshot": (True, True)}], True),
        ("token without safe snapshot is rejected", [{"token": TOKEN}, {"snapshot": (False, False)}], False),
        ("fault snapshot is rejected", [{"snapshot": (False, False)}, {"token": TOKEN}], False),
    ]
    records=[]
    for name, events, expected in cases:
        actual=core0_accepts(events)
        records.append({"name":name,"expected":expected,"actual":actual,"passed":actual==expected})
    payload={"gate":"G4","firmware":"0.7.2","passed":sum(r["passed"] for r in records),
             "failed":sum(not r["passed"] for r in records),"cases":records}
    out=Path(__file__).resolve().parents[1]/"validation"/"gate4_startup_oracle.json"
    out.write_text(json.dumps(payload,indent=2),encoding="utf-8")
    print(json.dumps(payload,indent=2))
    return 0 if payload["failed"]==0 else 1

if __name__ == "__main__":
    raise SystemExit(main())
