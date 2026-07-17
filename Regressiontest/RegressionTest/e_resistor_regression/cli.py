from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import sys
import uuid
from dataclasses import asdict
from datetime import datetime, timezone
from pathlib import Path

from . import __version__ as REGRESSION_PACKAGE_VERSION
from .build_check import run_arduino_build
from .coverage import (
    coverage_counts,
    evaluate_coverage,
    write_coverage_csv,
    write_coverage_json,
    write_coverage_markdown,
)
from .evidence import finalize_evidence
from .logging_ext import ExtendedLogger
from .models import RunConfig, TestResult
from .reports import compare_baseline, flatten_metrics, write_junit, write_markdown_report, write_metrics_csv
from .source_checks import evaluate_gate_expectations, scan_source
from .suite import RegressionSuite


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="E-Resistor gate regression runner")
    parser.add_argument("--host", default="192.168.0.55")
    parser.add_argument("--http-port", type=int, default=80)
    parser.add_argument("--scpi-port", type=int, default=5025)
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--iterations", type=int, default=30)
    parser.add_argument("--stress-iterations", type=int, default=100)
    parser.add_argument("--heap-drift-limit", type=int, default=2048)
    parser.add_argument("--latency-regression-percent", type=float, default=15.0)
    parser.add_argument("--gate", choices=[f"G{i}" for i in range(10)], default="G2")
    parser.add_argument("--profile", choices=["read_only", "safe_output", "hil_single_channel", "active_output", "storage", "ota", "watchdog"], default="read_only")
    parser.add_argument("--output", required=True)
    parser.add_argument("--source-dir")
    parser.add_argument("--baseline")
    parser.add_argument("--gate-manifest")
    parser.add_argument("--allow-output-tests", action="store_true")
    parser.add_argument("--allow-active-output-tests", action="store_true")
    parser.add_argument("--allow-storage-tests", action="store_true")
    parser.add_argument("--allow-ota-tests", action="store_true")
    parser.add_argument("--allow-watchdog-tests", action="store_true")
    parser.add_argument("--arduino-cli")
    parser.add_argument("--fqbn")
    parser.add_argument("--skip-device", action="store_true", help="Run only source/build checks; do not contact hardware")
    parser.add_argument("--serial-port", default="auto", help="RP2040 USB CDC/COM port, or auto")
    parser.add_argument("--serial-baud", type=int, default=115200)
    parser.add_argument("--serial-match", default="", help="Substring used for COM-port auto-selection")
    parser.add_argument("--dmm-resource", default="auto", help="PyVISA resource string, or auto")
    parser.add_argument("--dmm-idn-contains", default="34401", help="Text required in DMM *IDN? during auto-selection")
    parser.add_argument("--dmm-backend", default="", help="Optional PyVISA backend such as @py")
    parser.add_argument("--dmm-init-command", action="append", dest="dmm_init_commands",
                        help="DMM initialization command; repeat option for multiple commands")
    parser.add_argument("--dmm-init-commands", dest="dmm_init_commands_pipe", default="",
                        help="Pipe-separated DMM initialization commands; convenient for BAT files")
    parser.add_argument("--dmm-measure-command", default="READ?")
    parser.add_argument("--hil-channel", type=int, default=1, choices=range(1, 9))
    parser.add_argument("--hil-bits", default="0-15", help="Single-bit physical tests, e.g. 0-15 or 0,1,4-7")
    parser.add_argument("--hil-combination-masks", default="0003,0005,0009",
                        help="Comma-separated hexadecimal masks for combination tests")
    parser.add_argument("--hil-repeat-cycles", type=int, default=10)
    parser.add_argument("--hil-error-limit-percent", type=float, default=1.0)
    parser.add_argument("--hil-settle-timeout", type=float, default=20.0)
    parser.add_argument("--hil-sample-count", type=int, default=5)
    parser.add_argument("--hil-sample-interval", type=float, default=0.25)
    parser.add_argument("--hil-stability-percent", type=float, default=0.20)
    parser.add_argument("--hil-minimum-wait", type=float, default=0.5)
    parser.add_argument("--hil-off-min-ohm", type=float, default=50_000_000.0,
                        help="Minimum DMM resistance accepted with the selected channel OFF")
    parser.add_argument("--hil-serial-fault-pattern", action="append", dest="hil_serial_fault_patterns")
    parser.add_argument("--hil-serial-fault-patterns", dest="hil_serial_fault_patterns_pipe", default="",
                        help="Pipe-separated serial fault patterns; convenient for BAT files")
    parser.add_argument("--fixture-confirmation", default="",
                        help="For HIL use exactly: E_RESISTOR_SINGLE_CHANNEL_DMM")
    parser.add_argument("--verbose", action="store_true")
    return parser.parse_args(argv)



def _split_pipe_values(value: str) -> list[str]:
    """Split a Windows-BAT-friendly pipe-delimited value into non-empty entries."""
    return [item.strip() for item in value.split("|") if item.strip()]


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    output_dir = Path(args.output).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    logger = ExtendedLogger(output_dir, verbose=args.verbose)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_id = f"{args.gate}-{args.profile}-{timestamp}-{uuid.uuid4().hex[:6]}"

    if args.profile == "safe_output" and not args.allow_output_tests:
        logger.log.warning("safe_output profile requested without --allow-output-tests; output tests will be skipped")
    if args.profile == "hil_single_channel":
        if not args.allow_active_output_tests:
            logger.log.warning("hil_single_channel requires --allow-active-output-tests")
        if args.fixture_confirmation != "E_RESISTOR_SINGLE_CHANNEL_DMM":
            logger.log.warning("hil_single_channel fixture confirmation is missing or incorrect")
    if args.profile == "active_output" and not args.allow_active_output_tests:
        logger.log.warning("active_output profile requires --allow-active-output-tests and a fixture extension")
    if args.profile == "storage" and not args.allow_storage_tests:
        logger.log.warning("storage profile requires --allow-storage-tests and a fixture extension")
    if args.profile == "ota" and not args.allow_ota_tests:
        logger.log.warning("ota profile requires --allow-ota-tests and a fixture extension")
    if args.profile == "watchdog" and not args.allow_watchdog_tests:
        logger.log.warning("watchdog profile requires --allow-watchdog-tests and test firmware")

    config = RunConfig(
        host=args.host,
        http_port=args.http_port,
        scpi_port=args.scpi_port,
        timeout_s=args.timeout,
        iterations=max(1, args.iterations),
        stress_iterations=max(1, args.stress_iterations),
        heap_drift_limit_bytes=max(0, args.heap_drift_limit),
        latency_regression_percent=max(0.0, args.latency_regression_percent),
        gate=args.gate,
        profile=args.profile,
        output_dir=str(output_dir),
        source_dir=args.source_dir,
        baseline=args.baseline,
        allow_output_tests=args.allow_output_tests,
        allow_active_output_tests=args.allow_active_output_tests,
        allow_storage_tests=args.allow_storage_tests,
        allow_ota_tests=args.allow_ota_tests,
        allow_watchdog_tests=args.allow_watchdog_tests,
        arduino_cli=args.arduino_cli,
        fqbn=args.fqbn,
        skip_device=args.skip_device,
        serial_port=args.serial_port,
        serial_baud=max(1, args.serial_baud),
        serial_match=args.serial_match,
        dmm_resource=args.dmm_resource,
        dmm_idn_contains=args.dmm_idn_contains,
        dmm_backend=args.dmm_backend,
        dmm_init_commands=(args.dmm_init_commands
                           or _split_pipe_values(args.dmm_init_commands_pipe)
                           or ["*CLS", "CONF:RES AUTO", "TRIG:SOUR IMM", "SAMP:COUN 1"]),
        dmm_measure_command=args.dmm_measure_command,
        hil_channel=args.hil_channel,
        hil_bits=args.hil_bits,
        hil_combination_masks=args.hil_combination_masks,
        hil_repeat_cycles=max(1, args.hil_repeat_cycles),
        hil_error_limit_percent=max(0.0, args.hil_error_limit_percent),
        hil_settle_timeout_s=max(1.0, args.hil_settle_timeout),
        hil_sample_count=max(2, args.hil_sample_count),
        hil_sample_interval_s=max(0.0, args.hil_sample_interval),
        hil_stability_percent=max(0.0, args.hil_stability_percent),
        hil_minimum_wait_s=max(0.0, args.hil_minimum_wait),
        hil_off_min_ohm=max(0.0, args.hil_off_min_ohm),
        hil_serial_fault_patterns=(args.hil_serial_fault_patterns
                                   or _split_pipe_values(args.hil_serial_fault_patterns_pipe)
                                   or ["fatal", "panic", "assert", "hardfault", "queue overflow"]),
        fixture_confirmation=args.fixture_confirmation,
    )

    environment = {
        "schema_version": 2,
        "runner": "pure_python",
        "run_id": run_id,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "python": sys.version,
        "platform": platform.platform(),
        "machine": platform.machine(),
        "cwd": os.getcwd(),
        "regression_package_version": REGRESSION_PACKAGE_VERSION,
        "config": asdict(config),
    }
    (output_dir / "environment.json").write_text(
        json.dumps(environment, indent=2, sort_keys=True), encoding="utf-8"
    )
    (output_dir / "RUN_INCOMPLETE").write_text(
        f"run_id={run_id}\nstarted_utc={environment['timestamp_utc']}\n",
        encoding="utf-8",
    )
    logger.event("suite_initialized", run_id=run_id, gate=args.gate, profile=args.profile)

    source_metrics = None
    source_results: list[TestResult] = []
    build_result = None
    if args.source_dir:
        source_dir = Path(args.source_dir).resolve()
        source_metrics = scan_source(source_dir)
        (output_dir / "source_metrics.json").write_text(json.dumps(source_metrics, indent=2, sort_keys=True), encoding="utf-8")
        manifest_path = Path(args.gate_manifest).resolve() if args.gate_manifest else None
        checks = evaluate_gate_expectations(source_metrics, source_dir, args.gate, manifest_path)
        for index, check in enumerate(checks, start=1):
            source_results.append(TestResult(
                f"SRC-{index:03d}", check["name"], "PASS" if check["passed"] else "FAIL", 0.0,
                f"actual={check['actual']!r}, expected={check['expected']!r}", details=check,
            ))

        if args.arduino_cli and args.fqbn:
            cli_path = shutil.which(args.arduino_cli) or args.arduino_cli
            build_result = run_arduino_build(cli_path, args.fqbn, source_dir, output_dir)
            source_results.append(TestResult(
                "BUILD-001", "Arduino CLI compilation", "PASS" if build_result["passed"] else "FAIL", 0.0,
                f"returncode={build_result['returncode']}, warnings={build_result['warnings']}, errors={build_result['errors']}",
                metrics={key: value for key, value in build_result.items() if isinstance(value, (int, float))},
                details=build_result,
            ))

    runtime_results: list[TestResult] = []
    if not args.skip_device:
        suite = RegressionSuite(config, logger)
        runtime_results = suite.run(output_dir)
    else:
        logger.log.info("Device tests skipped by --skip-device")
        runtime_results.append(TestResult("DEVICE-000", "Hardware regression", "SKIP", 0.0, "disabled by --skip-device"))
    results = source_results + runtime_results

    counts = {status: sum(1 for item in results if item.status == status) for status in ("PASS", "FAIL", "SKIP")}
    summary = {
        "schema_version": 2,
        "runner": "pure_python",
        "run_id": run_id,
        "regression_package_version": REGRESSION_PACKAGE_VERSION,
        "config": asdict(config),
        "counts": counts,
        "overall_status": "FAIL" if counts["FAIL"] else "PASS",
        "results": [item.to_dict() for item in results],
        "flat_metrics": flatten_metrics(results),
        "source_metrics": source_metrics,
        "build": build_result,
    }
    baseline_path = Path(args.baseline).resolve() if args.baseline else None
    comparisons = compare_baseline(summary, baseline_path, config.latency_regression_percent)
    summary["baseline_comparison"] = comparisons
    failed_comparisons = [item for item in comparisons if not item["passed"]]
    if failed_comparisons:
        summary["overall_status"] = "FAIL"
        summary["counts"]["FAIL"] += len(failed_comparisons)

    coverage_rows = evaluate_coverage(summary)
    summary["test_coverage"] = {
        "counts": coverage_counts(coverage_rows),
        "rows": coverage_rows,
    }

    (output_dir / "results.json").write_text(json.dumps(summary, indent=2, sort_keys=True), encoding="utf-8")
    write_metrics_csv(output_dir / "metrics.csv", results)
    write_junit(output_dir / "junit.xml", results)
    write_coverage_csv(output_dir / "test_coverage.csv", coverage_rows)
    write_coverage_json(output_dir / "test_coverage.json", coverage_rows)
    write_coverage_markdown(output_dir / "test_coverage.md", coverage_rows)
    write_markdown_report(output_dir / "report.md", summary)

    logger.log.info("Overall %s: PASS=%d FAIL=%d SKIP=%d", summary["overall_status"],
                    summary["counts"]["PASS"], summary["counts"]["FAIL"], summary["counts"]["SKIP"])
    logger.log.info("Report: %s", output_dir / "report.md")
    logger.event(
        "suite_finalized", run_id=run_id, gate=args.gate, profile=args.profile,
        overall_status=summary["overall_status"], counts=summary["counts"],
    )
    logger.flush()
    verification = finalize_evidence(output_dir, run_id, summary["overall_status"])
    print(
        f"Evidence manifest verified: {verification['checked_files']} files; "
        f"report={output_dir / 'report.md'}"
    )
    return 0 if summary["overall_status"] == "PASS" else 1
