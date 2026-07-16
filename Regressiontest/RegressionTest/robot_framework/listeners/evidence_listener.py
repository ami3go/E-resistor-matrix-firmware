from __future__ import annotations

import json
import os
import threading
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


class EvidenceListener:
    """Robot Framework listener that records runner-level suite/test lifecycle events."""

    ROBOT_LISTENER_API_VERSION = 3

    def __init__(self, output_path: str = "") -> None:
        configured = output_path or os.environ.get("ERESISTOR_ROBOT_LISTENER_LOG", "")
        self.path = Path(configured).expanduser().resolve() if configured else None
        self._lock = threading.Lock()

    @staticmethod
    def _utc_now() -> str:
        return datetime.now(timezone.utc).isoformat()

    def _write(self, event: str, **data: Any) -> None:
        if self.path is None:
            return
        self.path.parent.mkdir(parents=True, exist_ok=True)
        record = {"timestamp": self._utc_now(), "event": event, **data}
        with self._lock:
            with self.path.open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")

    def start_suite(self, data, result) -> None:
        self._write("robot_suite_start", name=data.name, longname=data.longname)

    def end_suite(self, data, result) -> None:
        self._write(
            "robot_suite_end",
            name=data.name,
            status=result.status,
            message=result.message,
            elapsed_time_ms=result.elapsed_time.total_seconds() * 1000.0,
        )

    def start_test(self, data, result) -> None:
        self._write(
            "robot_test_start",
            name=data.name,
            longname=data.longname,
            tags=sorted(str(tag) for tag in data.tags),
        )

    def end_test(self, data, result) -> None:
        self._write(
            "robot_test_end",
            name=data.name,
            longname=data.longname,
            status=result.status,
            message=result.message,
            tags=sorted(str(tag) for tag in data.tags),
            elapsed_time_ms=result.elapsed_time.total_seconds() * 1000.0,
        )

    def close(self) -> None:
        self._write("robot_run_close")
