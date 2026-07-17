"""Shared pass/fail criteria for production checks and boundary self-tests.

The Robot boundary tests call these same functions used by the real regression
checks, preventing the acceptance-test logic from drifting away from runtime
classification.
"""
from __future__ import annotations

import math


def passes_absolute_error(error_percent: float, limit_percent: float) -> bool:
    """Pass when a finite signed error is inside the inclusive absolute limit."""
    return (
        math.isfinite(float(error_percent))
        and math.isfinite(float(limit_percent))
        and float(limit_percent) >= 0.0
        and abs(float(error_percent)) <= float(limit_percent)
    )


def passes_stability(relative_stdev_percent: float, limit_percent: float) -> bool:
    """Pass when finite relative standard deviation is within the inclusive limit."""
    return (
        math.isfinite(float(relative_stdev_percent))
        and math.isfinite(float(limit_percent))
        and float(relative_stdev_percent) >= 0.0
        and float(limit_percent) >= 0.0
        and float(relative_stdev_percent) <= float(limit_percent)
    )


def passes_off_resistance(measured_ohm: float, minimum_ohm: float) -> bool:
    """Pass high-isolation measurements at/above the minimum, including +infinity.

    Positive infinity is the normalized representation of a DMM overload/open
    indication. NaN, negative infinity, and negative values fail.
    """
    measured = float(measured_ohm)
    minimum = float(minimum_ohm)
    if not math.isfinite(minimum) or minimum < 0.0 or math.isnan(measured):
        return False
    if math.isinf(measured):
        return measured > 0.0
    return measured >= 0.0 and measured >= minimum


def passes_heap_decline(decline_bytes: int | float, limit_bytes: int | float) -> bool:
    """Pass when heap decline is at or below the inclusive byte limit."""
    decline = float(decline_bytes)
    limit = float(limit_bytes)
    return math.isfinite(decline) and math.isfinite(limit) and limit >= 0.0 and decline <= limit


def passes_latency_regression(change_percent: float, limit_percent: float) -> bool:
    """Pass when latency change is at or below the inclusive regression limit."""
    change = float(change_percent)
    limit = float(limit_percent)
    return math.isfinite(change) and math.isfinite(limit) and limit >= 0.0 and change <= limit


def upper_margin(limit: float, value: float) -> float:
    """Return remaining margin for an upper-bound criterion."""
    return float(limit) - float(value)


def lower_margin(value: float, minimum: float) -> float:
    """Return remaining margin for a lower-bound criterion."""
    return float(value) - float(minimum)
