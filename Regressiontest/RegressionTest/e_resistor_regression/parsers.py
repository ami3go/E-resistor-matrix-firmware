from __future__ import annotations

import math
import re
from typing import Any


# Firmware v0.4.4 emits masks with a ``0x`` prefix while older builds and
# several stored baselines use four bare hexadecimal digits.  Accept both
# forms and always normalize the parsed mask to four uppercase digits.
CHANNEL_STATE_RE = re.compile(
    r"^ch(?P<channel>[1-8])=(?:0[xX])?(?P<mask>[0-9A-Fa-f]{4})"
    r"\s+resistance=(?P<resistance>.*?)\s+count=(?P<count>\d+)$",
    re.IGNORECASE,
)
SCPI_STATE_RE = re.compile(
    r"\bCH(?P<channel>[1-8])=(?:0[xX])?(?P<mask>[0-9A-Fa-f]{4}),"
    r"(?P<resistance>[^;\r\n]+)",
    re.IGNORECASE,
)


def parse_http_state(text: str) -> dict[str, Any]:
    result: dict[str, Any] = {"channels": {}}
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        match = CHANNEL_STATE_RE.match(line)
        if match:
            ch = int(match.group("channel"))
            result["channels"][ch] = {
                "mask": match.group("mask").upper(),
                "resistance": match.group("resistance").strip(),
                "count": int(match.group("count")),
            }
            continue
        if "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip()
    return result


def parse_scpi_state(text: str) -> dict[int, dict[str, str]]:
    result: dict[int, dict[str, str]] = {}
    for match in SCPI_STATE_RE.finditer(text):
        ch = int(match.group("channel"))
        result[ch] = {
            "mask": match.group("mask").upper(),
            "resistance": match.group("resistance").strip(),
        }
    return result


def parse_calibration_compact(text: str) -> dict[int, list[dict[str, Any]]]:
    channels: dict[int, list[dict[str, Any]]] = {}
    for channel_section in text.strip().split("|"):
        if not channel_section:
            continue
        prefix, sep, rows_text = channel_section.partition(":")
        if not sep or not prefix.upper().startswith("CH"):
            raise ValueError(f"Invalid calibration section: {channel_section[:80]!r}")
        channel = int(prefix[2:])
        rows: list[dict[str, Any]] = []
        for row_text in rows_text.split(";"):
            fields = row_text.split(",")
            if len(fields) != 3:
                raise ValueError(f"Invalid calibration row: {row_text!r}")
            bit = int(fields[0])
            mosfet = fields[1]
            resistance_text = fields[2]
            resistance = math.nan if resistance_text.lower() == "nan" else float(resistance_text)
            rows.append({"bit": bit, "mosfet": mosfet, "resistance_ohm": resistance})
        channels[channel] = rows
    return channels


def percentile(values: list[float], q: float) -> float:
    if not values:
        return math.nan
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * q
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def latency_metrics(values: list[float], prefix: str) -> dict[str, float | int]:
    if not values:
        return {}
    return {
        f"{prefix}_count": len(values),
        f"{prefix}_avg_ms": sum(values) / len(values),
        f"{prefix}_p50_ms": percentile(values, 0.50),
        f"{prefix}_p95_ms": percentile(values, 0.95),
        f"{prefix}_p99_ms": percentile(values, 0.99),
        f"{prefix}_max_ms": max(values),
        f"{prefix}_min_ms": min(values),
    }
