from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUITES = ROOT / "robot_framework" / "suites"
SUITE_MODULE = ROOT / "e_resistor_regression" / "suite.py"
OUTPUT = ROOT / "validation" / "G4_robot_static_validation.json"


def parse_suite(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    if "*** Settings ***" not in text or "*** Test Cases ***" not in text:
        raise AssertionError(f"{path.name}: missing required section")
    section = text.split("*** Test Cases ***", 1)[1]
    tests = []
    mappings = []
    current = None
    for line_number, line in enumerate(section.splitlines(), start=1):
        if line.startswith("*** "):
            break
        if line and not line[0].isspace():
            current = line.strip()
            tests.append(current)
        match = re.match(
            r"\s+Run Regression Check\s{2,}(\S+)\s{2,}.*?\s{2,}(test_[A-Za-z0-9_]+)\s*$",
            line,
        )
        if match:
            mappings.append({"test_case": current, "test_id": match.group(1), "method": match.group(2)})
    if len(tests) != len(mappings):
        raise AssertionError(f"{path.name}: {len(tests)} tests but {len(mappings)} mappings")
    ids = [m["test_id"] for m in mappings]
    if len(ids) != len(set(ids)):
        raise AssertionError(f"{path.name}: duplicate test IDs")
    return {"suite": path.name, "test_count": len(tests), "mappings": mappings}


def main() -> int:
    # Parse method names without importing Robot Framework.
    source = SUITE_MODULE.read_text(encoding="utf-8")
    methods = set(re.findall(r"^\s+def\s+(test_[A-Za-z0-9_]+)\s*\(", source, re.MULTILINE))
    expected = {
        "read_only.robot": 20,
        "safe_output.robot": 23,
        "hil_single_channel.robot": 29,
        "gate3_transport_fault.robot": 7,
        "gate4_profile.robot": 6,
        "gate4_profile_fault.robot": 7,
    }
    records = []
    failures = []
    for name, expected_count in expected.items():
        try:
            record = parse_suite(SUITES / name)
            if record["test_count"] != expected_count:
                raise AssertionError(
                    f"{name}: expected {expected_count} tests, found {record['test_count']}"
                )
            missing_methods = sorted({m["method"] for m in record["mappings"]} - methods)
            if missing_methods:
                raise AssertionError(f"{name}: missing methods {missing_methods}")
            records.append(record)
        except Exception as exc:
            failures.append(str(exc))
    required_g3 = {"G3-001", "G3-002", "G3-003", "G3-FI-001", "G3-FI-002", "G3-FI-003"}
    required_g4 = {"G4-001", "G4-002", "G4-003", "G4-FI-001"}
    mapped = {m["test_id"] for r in records for m in r["mappings"]}
    missing_g3 = sorted(required_g3 - mapped)
    if missing_g3:
        failures.append(f"missing Gate 3 IDs: {missing_g3}")
    missing_g4 = sorted(required_g4 - mapped)
    if missing_g4:
        failures.append(f"missing Gate 4 IDs: {missing_g4}")
    robot_dryrun = {"available": False, "passed": None, "returncode": None}
    try:
        import robot  # noqa: F401
        robot_dryrun["available"] = True
        dryrun_dir = ROOT / "validation" / "G4_robot_dryrun"
        dryrun_dir.mkdir(parents=True, exist_ok=True)
        command = [
            sys.executable, "-m", "robot", "--dryrun",
            "--pythonpath", str(ROOT), "--outputdir", str(dryrun_dir),
            "--output", "NONE", "--log", "NONE", "--report", "NONE",
            str(SUITES),
        ]
        completed = subprocess.run(command, cwd=ROOT, check=False)
        robot_dryrun["returncode"] = completed.returncode
        robot_dryrun["passed"] = completed.returncode == 0
        if completed.returncode != 0:
            failures.append(f"Robot Framework dry run failed with {completed.returncode}")
    except ImportError:
        pass

    payload = {
        "gate": "G4",
        "package_version": "2.7.0",
        "suite_count": len(records),
        "test_count": sum(r["test_count"] for r in records),
        "required_gate3_ids": sorted(required_g3),
        "required_gate4_ids": sorted(required_g4),
        "robot_dryrun": robot_dryrun,
        "failures": failures,
        "passed": not failures,
        "suites": records,
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    print(json.dumps({k: payload[k] for k in ("passed", "suite_count", "test_count", "failures")}, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
