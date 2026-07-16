from __future__ import annotations

import csv
import json
import math
from pathlib import Path
from typing import Any
from xml.etree.ElementTree import Element, SubElement, ElementTree

from .models import TestResult


def write_metrics_csv(path: Path, results: list[TestResult]) -> None:
    rows: list[dict[str, Any]] = []
    for result in results:
        for key, value in result.metrics.items():
            rows.append({"test_id": result.test_id, "metric": key, "value": value})
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["test_id", "metric", "value"])
        writer.writeheader()
        writer.writerows(rows)


def write_junit(path: Path, results: list[TestResult]) -> None:
    failures = sum(1 for item in results if item.status == "FAIL")
    skipped = sum(1 for item in results if item.status == "SKIP")
    suite = Element("testsuite", {
        "name": "E-Resistor regression",
        "tests": str(len(results)),
        "failures": str(failures),
        "skipped": str(skipped),
    })
    for result in results:
        case = SubElement(suite, "testcase", {
            "classname": "e_resistor_regression",
            "name": f"{result.test_id} {result.name}",
            "time": f"{result.duration_ms / 1000.0:.6f}",
        })
        if result.status == "FAIL":
            failure = SubElement(case, "failure", {"message": result.message or "failed"})
            failure.text = json.dumps(result.details, ensure_ascii=False, indent=2)
        elif result.status == "SKIP":
            SubElement(case, "skipped", {"message": result.message or "skipped"})
        if result.metrics:
            output = SubElement(case, "system-out")
            output.text = json.dumps(result.metrics, ensure_ascii=False, indent=2)
    ElementTree(suite).write(path, encoding="utf-8", xml_declaration=True)


def flatten_metrics(results: list[TestResult]) -> dict[str, float | int | str]:
    flattened: dict[str, float | int | str] = {}
    for result in results:
        for key, value in result.metrics.items():
            flattened[f"{result.test_id}.{key}"] = value
    return flattened


def compare_baseline(current: dict[str, Any], baseline_path: Path | None,
                     allowed_regression_percent: float) -> list[dict[str, Any]]:
    if not baseline_path or not baseline_path.exists():
        return []
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    old_metrics = baseline.get("flat_metrics", {})
    new_metrics = current.get("flat_metrics", {})
    comparisons: list[dict[str, Any]] = []
    for key, new_value in new_metrics.items():
        if not key.endswith((
            "_avg_ms", "_p95_ms", "_p99_ms", "_max_ms",
            "_max_abs_error_percent", "_avg_abs_error_percent",
            "_repeatability_stdev_percent", "_heap_decline_bytes",
        )):
            continue
        old_value = old_metrics.get(key)
        if not isinstance(old_value, (int, float)) or not isinstance(new_value, (int, float)):
            continue
        if old_value == 0 or not math.isfinite(float(old_value)):
            continue
        change_percent = (float(new_value) - float(old_value)) / float(old_value) * 100.0
        comparisons.append({
            "metric": key,
            "baseline": old_value,
            "current": new_value,
            "change_percent": change_percent,
            "passed": change_percent <= allowed_regression_percent,
        })
    return comparisons


def write_markdown_report(path: Path, summary: dict[str, Any]) -> None:
    lines = [
        "# E-Resistor Regression Report",
        "",
        f"- Gate: **{summary['config']['gate']}**",
        f"- Profile: **{summary['config']['profile']}**",
        f"- Host: `{summary['config']['host']}`",
        f"- Overall: **{summary['overall_status']}**",
        f"- Passed: {summary['counts']['PASS']}",
        f"- Failed: {summary['counts']['FAIL']}",
        f"- Skipped: {summary['counts']['SKIP']}",
        "",
        "## Test results",
        "",
        "| ID | Test | Status | Duration, ms | Message |",
        "|---|---|---:|---:|---|",
    ]
    for result in summary["results"]:
        message = str(result.get("message", "")).replace("|", "\\|").replace("\n", " ")
        lines.append(
            f"| {result['test_id']} | {result['name']} | {result['status']} | "
            f"{result['duration_ms']:.2f} | {message} |"
        )

    comparisons = summary.get("baseline_comparison", [])
    if comparisons:
        lines += [
            "",
            "## Baseline comparison",
            "",
            "| Metric | Baseline | Current | Change | Status |",
            "|---|---:|---:|---:|---:|",
        ]
        for item in comparisons:
            status = "PASS" if item["passed"] else "FAIL"
            lines.append(
                f"| {item['metric']} | {item['baseline']:.3f} | {item['current']:.3f} | "
                f"{item['change_percent']:+.2f}% | {status} |"
            )

    coverage = summary.get("test_coverage", {})
    if coverage:
        counts = coverage.get("counts", {})
        lines += [
            "",
            "## Test coverage",
            "",
            "The complete requirement traceability table is written to `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json`.",
            "",
            "| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |",
            "|---:|---:|---:|---:|---:|---:|---:|",
            f"| {counts.get('PASS', 0)} | {counts.get('FAIL', 0)} | {counts.get('PARTIAL', 0)} | "
            f"{counts.get('SKIP', 0)} | {counts.get('NOT_RUN', 0)} | {counts.get('PLANNED', 0)} | "
            f"{counts.get('MANUAL', 0)} |",
        ]

        exceptions = [
            row for row in coverage.get("rows", [])
            if row.get("run_status") in {"FAIL", "PARTIAL", "SKIP", "NOT_RUN", "PLANNED", "MANUAL"}
        ]
        if exceptions:
            lines += [
                "",
                "### Coverage gaps and exceptions",
                "",
                "| ID | Requirement | Status | Test(s) |",
                "|---|---|---:|---|",
            ]
            for row in exceptions:
                requirement = str(row.get("requirement", "")).replace("|", "\\|").replace("\n", " ")
                tests = str(row.get("test_patterns", "")).replace("|", "\\|") or "—"
                lines.append(
                    f"| {row.get('coverage_id')} | {requirement} | {row.get('run_status')} | {tests} |"
                )

    if summary.get("source_metrics"):
        source = summary["source_metrics"]
        lines += [
            "",
            "## Source metrics",
            "",
            f"- Files: {source.get('file_count')}",
            f"- Lines: {source.get('total_lines')}",
            f"- `app.h` lines: {source.get('app_h_lines')}",
            f"- `http_handlers.cpp` lines: {source.get('http_handlers_lines')}",
            f"- `String` tokens: {source.get('string_token_count')}",
            f"- Core-related Serial calls: {source.get('core1_serial_call_count')}",
            f"- State-changing GET routes: {', '.join(source.get('mutation_get_routes', [])) or 'none'}",
        ]

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
