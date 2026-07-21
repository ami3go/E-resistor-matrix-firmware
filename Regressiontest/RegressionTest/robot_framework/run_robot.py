from __future__ import annotations

import argparse
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RF_ROOT = Path(__file__).resolve().parent
SUITES = {
    "read_only": RF_ROOT / "suites" / "read_only.robot",
    "safe_output": RF_ROOT / "suites" / "safe_output.robot",
    "hil_single_channel": RF_ROOT / "suites" / "hil_single_channel.robot",
    "gate3_transport_fault": RF_ROOT / "suites" / "gate3_transport_fault.robot",
    "gate4_profile": RF_ROOT / "suites" / "gate4_profile.robot",
    "gate4_profile_fault": RF_ROOT / "suites" / "gate4_profile_fault.robot",
}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run E-Resistor regression using Robot Framework"
    )
    parser.add_argument("--profile", choices=sorted(SUITES), default="read_only")
    parser.add_argument("--gate", choices=[f"G{i}" for i in range(10)], default="G4")
    parser.add_argument("--output", default=str(ROOT / "results" / "robot"))
    parser.add_argument("--host", default="192.168.0.55")
    parser.add_argument("--http-port", type=int, default=80)
    parser.add_argument("--scpi-port", type=int, default=5025)
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--iterations", type=int, default=30)
    parser.add_argument("--stress-iterations", type=int, default=100)
    parser.add_argument("--heap-drift-limit", type=int, default=2048)
    parser.add_argument("--latency-regression-percent", type=float, default=15.0)
    parser.add_argument("--baseline", default="")
    parser.add_argument("--gate-manifest", default="")
    parser.add_argument("--allow-output-tests", action="store_true")
    parser.add_argument("--allow-active-output-tests", action="store_true")
    parser.add_argument("--allow-storage-tests", action="store_true")
    parser.add_argument("--allow-ota-tests", action="store_true")
    parser.add_argument("--allow-watchdog-tests", action="store_true")
    parser.add_argument("--fixture-confirmation", default="")
    parser.add_argument("--serial-port", default="auto")
    parser.add_argument("--serial-baud", type=int, default=115200)
    parser.add_argument("--serial-match", default="")
    parser.add_argument("--dmm-resource", default="auto")
    parser.add_argument("--dmm-idn-contains", default="34401")
    parser.add_argument("--dmm-backend", default="")
    parser.add_argument("--dmm-init-commands", default="*CLS|CONF:RES AUTO|TRIG:SOUR IMM|SAMP:COUN 1")
    parser.add_argument("--dmm-measure-command", default="READ?")
    parser.add_argument("--hil-channel", type=int, choices=range(1, 9), default=1)
    parser.add_argument("--hil-bits", default="0-15")
    parser.add_argument("--hil-combination-masks", default="0003,0005,0009")
    parser.add_argument("--hil-repeat-cycles", type=int, default=50)
    parser.add_argument("--hil-error-limit-percent", type=float, default=1.0)
    parser.add_argument("--hil-settle-timeout", type=float, default=20.0)
    parser.add_argument("--hil-sample-count", type=int, default=5)
    parser.add_argument("--hil-sample-interval", type=float, default=0.25)
    parser.add_argument("--hil-stability-percent", type=float, default=0.20)
    parser.add_argument("--hil-minimum-wait", type=float, default=0.5)
    parser.add_argument("--hil-off-min-ohm", type=float, default=50_000_000.0)
    parser.add_argument("--hil-serial-fault-patterns", default="fatal|panic|assert|hardfault|queue overflow")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument("--robot-arg", action="append", default=[], help="Extra raw Robot option; repeat as needed")
    return parser.parse_args(argv)


def _set(env: dict[str, str], name: str, value: object) -> None:
    env[f"ERESISTOR_{name}"] = str(value)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if args.profile in {"safe_output", "gate4_profile"} and not args.allow_output_tests:
        raise SystemExit(f"{args.profile} requires --allow-output-tests")
    if args.profile in {"hil_single_channel", "gate3_transport_fault", "gate4_profile_fault"}:
        if not args.allow_active_output_tests:
            raise SystemExit(f"{args.profile} requires --allow-active-output-tests")
        if args.fixture_confirmation != "E_RESISTOR_SINGLE_CHANNEL_DMM":
            raise SystemExit(
                f"{args.profile} requires --fixture-confirmation "
                "E_RESISTOR_SINGLE_CHANNEL_DMM"
            )

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_dir = Path(args.output).expanduser().resolve() / f"{args.gate}-{args.profile}-{timestamp}"
    output_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    values = {
        "GATE": args.gate,
        "HOST": args.host,
        "HTTP_PORT": args.http_port,
        "SCPI_PORT": args.scpi_port,
        "TIMEOUT_S": args.timeout,
        "ITERATIONS": args.iterations,
        "STRESS_ITERATIONS": args.stress_iterations,
        "HEAP_DRIFT_LIMIT_BYTES": args.heap_drift_limit,
        "LATENCY_REGRESSION_PERCENT": args.latency_regression_percent,
        "BASELINE": args.baseline,
        "GATE_MANIFEST": args.gate_manifest,
        "ALLOW_OUTPUT_TESTS": args.allow_output_tests,
        "ALLOW_ACTIVE_OUTPUT_TESTS": args.allow_active_output_tests,
        "ALLOW_STORAGE_TESTS": args.allow_storage_tests,
        "ALLOW_OTA_TESTS": args.allow_ota_tests,
        "ALLOW_WATCHDOG_TESTS": args.allow_watchdog_tests,
        "FIXTURE_CONFIRMATION": args.fixture_confirmation,
        "SERIAL_PORT": args.serial_port,
        "SERIAL_BAUD": args.serial_baud,
        "SERIAL_MATCH": args.serial_match,
        "DMM_RESOURCE": args.dmm_resource,
        "DMM_IDN_CONTAINS": args.dmm_idn_contains,
        "DMM_BACKEND": args.dmm_backend,
        "DMM_INIT_COMMANDS": args.dmm_init_commands,
        "DMM_MEASURE_COMMAND": args.dmm_measure_command,
        "HIL_CHANNEL": args.hil_channel,
        "HIL_BITS": args.hil_bits,
        "HIL_COMBINATION_MASKS": args.hil_combination_masks,
        "HIL_REPEAT_CYCLES": args.hil_repeat_cycles,
        "HIL_ERROR_LIMIT_PERCENT": args.hil_error_limit_percent,
        "HIL_SETTLE_TIMEOUT_S": args.hil_settle_timeout,
        "HIL_SAMPLE_COUNT": args.hil_sample_count,
        "HIL_SAMPLE_INTERVAL_S": args.hil_sample_interval,
        "HIL_STABILITY_PERCENT": args.hil_stability_percent,
        "HIL_MINIMUM_WAIT_S": args.hil_minimum_wait,
        "HIL_OFF_MIN_OHM": args.hil_off_min_ohm,
        "HIL_SERIAL_FAULT_PATTERNS": args.hil_serial_fault_patterns,
        "VERBOSE": args.verbose,
    }
    for name, value in values.items():
        _set(env, name, value)

    listener_log = output_dir / "robot_events.jsonl"
    env["ERESISTOR_ROBOT_LISTENER_LOG"] = str(listener_log)

    command = [
        sys.executable,
        "-m",
        "robot",
        "--pythonpath",
        str(ROOT),
        "--outputdir",
        str(output_dir),
        "--name",
        f"E-Resistor {args.gate} {args.profile}",
        "--metadata",
        f"Gate:{args.gate}",
        "--metadata",
        f"Profile:{args.profile}",
        "--metadata",
        f"Host:{args.host}",
        "--listener",
        "robot_framework.listeners.evidence_listener.EvidenceListener",
    ]
    if args.verbose:
        command += ["--loglevel", "DEBUG", "--console", "verbose"]
    for tag in args.include:
        command += ["--include", tag]
    for tag in args.exclude:
        command += ["--exclude", tag]
    command += args.robot_arg
    command.append(str(SUITES[args.profile]))

    print("Running:", " ".join(f'"{item}"' if " " in item else item for item in command))
    print("Output:", output_dir)
    completed = subprocess.run(command, cwd=ROOT, env=env, check=False)
    return int(completed.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
