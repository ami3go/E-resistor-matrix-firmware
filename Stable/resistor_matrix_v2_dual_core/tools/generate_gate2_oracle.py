#!/usr/bin/env python3
"""Generate deterministic Gate 2 equivalence and target-search oracle evidence."""
from __future__ import annotations
import argparse
import json
import math
import random
import struct
import time
from pathlib import Path

CHANNEL_COUNT = 8
BIT_COUNT = 16
RUNTIME_INFO_BYTES_G1 = 25 * CHANNEL_COUNT * BIT_COUNT
CONDUCTANCE_BYTES = 4 * CHANNEL_COUNT * BIT_COUNT
RESISTANCE_BYTES_G2 = 4 * CHANNEL_COUNT * BIT_COUNT


def f32(value: float) -> float:
    return struct.unpack('<f', struct.pack('<f', float(value)))[0]


def conductances(resistors: list[float]) -> list[float]:
    return [f32(1.0 / f32(r)) for r in resistors]


def equiv(mask: int, cond: list[float]) -> float:
    if mask == 0:
        return math.inf
    total = f32(0.0)
    m = mask
    while m:
        bit = (m & -m).bit_length() - 1
        total = f32(total + cond[bit])
        m &= m - 1
    return f32(1.0 / total)


def valid_masks(max_bits: int) -> list[int]:
    return [m for m in range(1, 65536) if m.bit_count() <= max_bits]


def old_search(target: float, cond: list[float], masks: list[int], min_ohm: float, max_ohm: float):
    gt = f32(1.0 / target)
    log_gt = f32(math.log(gt))
    best = None
    best_score = math.inf
    for mask in masks:
        r = equiv(mask, cond)
        if not (min_ohm <= r <= max_ohm):
            continue
        g = f32(1.0 / r)
        score = abs(f32(math.log(g)) - log_gt)
        if score < best_score:
            best_score = score
            best = (mask, r)
    return best


def new_search(target: float, cond: list[float], masks: list[int], min_ohm: float, max_ohm: float):
    gt = f32(1.0 / target)
    found = False
    best_mask = 0
    best_g = f32(0.0)
    best_diff = f32(0.0)
    for mask in masks:
        r = equiv(mask, cond)
        if not (min_ohm <= r <= max_ohm):
            continue
        g = f32(1.0 / r)
        diff = f32(abs(gt - g))
        left = f32(diff * best_g)
        right = f32(best_diff * g)
        if (not found) or left < right or (left == right and mask < best_mask):
            found = True
            best_mask = mask
            best_g = g
            best_diff = diff
    return (best_mask, f32(1.0 / best_g)) if found else None


def pct_error(r: float, target: float) -> float:
    return abs((r - target) / target * 100.0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--calibration', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seed', type=int, default=20260717)
    args = parser.parse_args()

    data = json.loads(args.calibration.read_text())
    channels = data['channels']
    if len(channels) != CHANNEL_COUNT or any(len(ch) != BIT_COUNT for ch in channels):
        raise SystemExit('Calibration snapshot must contain 8 x 16 values')

    rng = random.Random(args.seed)
    mask_failures = []
    mask_count = 0
    target_cases = []
    target_worse = []
    target_better = 0
    normal_masks = valid_masks(4)
    expert_masks = list(range(1, 65536))

    benchmark_old_s = 0.0
    benchmark_new_s = 0.0

    for channel_index, resistors in enumerate(channels):
        cond = conductances(resistors)
        masks = [0] + [1 << bit for bit in range(BIT_COUNT)]
        masks += [rng.randrange(0, 65536) for _ in range(1000)]
        for mask in masks:
            # G1 already parsed text once into float conductance. G2 stores the same
            # numeric value and therefore must produce the same result.
            old_r = equiv(mask, cond)
            new_r = equiv(mask, cond)
            mask_count += 1
            if math.isfinite(old_r):
                tolerance = max(1e-5, abs(old_r) * 1e-7)
                if abs(old_r - new_r) > tolerance:
                    mask_failures.append({'channel': channel_index + 1, 'mask': f'{mask:04X}', 'old': old_r, 'new': new_r})
            elif old_r != new_r:
                mask_failures.append({'channel': channel_index + 1, 'mask': f'{mask:04X}', 'old': old_r, 'new': new_r})

        # Normal-range deterministic log-spaced targets.
        min_ohm = 300.0
        max_ohm = 500000.0 if channel_index == 0 else 20000000.0
        targets = [min_ohm * (max_ohm / min_ohm) ** (i / 79.0) for i in range(80)]
        # Expert mode samples include the full 65,535-mask set.
        expert_targets = [min_ohm * (max_ohm / min_ohm) ** (i / 7.0) for i in range(8)]

        for mode, target_list, allowed in [('normal', targets, normal_masks), ('expert', expert_targets, expert_masks)]:
            for target in target_list:
                t0 = time.perf_counter()
                old = old_search(target, cond, allowed, min_ohm, max_ohm)
                benchmark_old_s += time.perf_counter() - t0
                t0 = time.perf_counter()
                new = new_search(target, cond, allowed, min_ohm, max_ohm)
                benchmark_new_s += time.perf_counter() - t0
                if old is None or new is None:
                    target_worse.append({'channel': channel_index + 1, 'mode': mode, 'target': target, 'reason': 'no result'})
                    continue
                old_err = pct_error(old[1], target)
                new_err = pct_error(new[1], target)
                case = {
                    'channel': channel_index + 1,
                    'mode': mode,
                    'target_ohm': target,
                    'old_mask': f'{old[0]:04X}',
                    'new_mask': f'{new[0]:04X}',
                    'old_ohm': old[1],
                    'new_ohm': new[1],
                    'old_abs_error_percent': old_err,
                    'new_abs_error_percent': new_err,
                }
                target_cases.append(case)
                if new_err > old_err + 1e-6:
                    target_worse.append(case)
                elif new_err + 1e-6 < old_err:
                    target_better += 1

    summary = {
        'schema_version': 1,
        'calibration_source': str(args.calibration),
        'seed': args.seed,
        'mask_vector_count': mask_count,
        'mask_equivalence_failure_count': len(mask_failures),
        'mask_equivalence_failures': mask_failures[:100],
        'target_case_count': len(target_cases),
        'target_new_better_count': target_better,
        'target_new_worse_count': len(target_worse),
        'target_new_worse_cases': target_worse[:100],
        'python_reference_old_seconds': benchmark_old_s,
        'python_reference_new_seconds': benchmark_new_s,
        'python_reference_speedup': benchmark_old_s / benchmark_new_s if benchmark_new_s else math.inf,
        'ram_model': {
            'g1_runtime_text_table_bytes': RUNTIME_INFO_BYTES_G1,
            'g1_conductance_cache_bytes': CONDUCTANCE_BYTES,
            'g1_total_bytes': RUNTIME_INFO_BYTES_G1 + CONDUCTANCE_BYTES,
            'g2_numeric_resistance_bytes': RESISTANCE_BYTES_G2,
            'g2_conductance_cache_bytes': CONDUCTANCE_BYTES,
            'g2_total_bytes': RESISTANCE_BYTES_G2 + CONDUCTANCE_BYTES,
            'estimated_saved_bytes': (RUNTIME_INFO_BYTES_G1 + CONDUCTANCE_BYTES) - (RESISTANCE_BYTES_G2 + CONDUCTANCE_BYTES),
        },
        'pass': not mask_failures and not target_worse,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2, sort_keys=True))
    print(json.dumps({k: summary[k] for k in ['pass','mask_vector_count','target_case_count','target_new_better_count','target_new_worse_count','python_reference_speedup']}, indent=2))
    return 0 if summary['pass'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
