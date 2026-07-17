from __future__ import annotations

import json
import logging
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


class ExtendedLogger:
    def __init__(self, output_dir: Path, verbose: bool = False) -> None:
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self.events_path = output_dir / "events.jsonl"
        self.http_path = output_dir / "http_transcript.log"
        self.scpi_path = output_dir / "scpi_transcript.log"
        self.dmm_path = output_dir / "dmm_transcript.log"
        self.serial_path = output_dir / "serial_transcript.log"

        self.log = logging.getLogger(f"e_resistor_regression.{id(self)}")
        self.log.setLevel(logging.DEBUG)
        self.log.propagate = False
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")

        file_handler = logging.FileHandler(output_dir / "run.log", encoding="utf-8")
        file_handler.setLevel(logging.DEBUG)
        file_handler.setFormatter(formatter)
        self.log.addHandler(file_handler)

        console = logging.StreamHandler()
        console.setLevel(logging.DEBUG if verbose else logging.INFO)
        console.setFormatter(formatter)
        self.log.addHandler(console)

    @staticmethod
    def _utc_now() -> str:
        return datetime.now(timezone.utc).isoformat()

    def event(self, event_type: str, **data: Any) -> None:
        record = {"timestamp": self._utc_now(), "monotonic_ns": time.monotonic_ns(), "event": event_type, **data}
        with self._lock:
            with self.events_path.open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")


    def flush(self) -> None:
        """Flush Python log handlers before evidence hashing."""
        with self._lock:
            for handler in self.log.handlers:
                try:
                    handler.flush()
                except Exception:
                    pass

    def transcript(self, channel: str, direction: str, payload: str, **metadata: Any) -> None:
        paths = {
            "http": self.http_path,
            "scpi": self.scpi_path,
            "dmm": self.dmm_path,
            "serial": self.serial_path,
        }
        path = paths.get(channel, self.output_dir / f"{channel}_transcript.log")
        record = {
            "timestamp": self._utc_now(),
            "monotonic_ns": time.monotonic_ns(),
            "direction": direction,
            "metadata": metadata,
            "payload": payload,
        }
        with self._lock:
            with path.open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")
