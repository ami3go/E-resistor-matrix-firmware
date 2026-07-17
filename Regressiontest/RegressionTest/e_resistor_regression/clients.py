from __future__ import annotations

import socket
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from typing import Mapping

from .logging_ext import ExtendedLogger


@dataclass
class HttpResponse:
    status: int
    headers: Mapping[str, str]
    body: bytes
    elapsed_ms: float

    @property
    def text(self) -> str:
        return self.body.decode("utf-8", errors="replace")


class HttpClient:
    def __init__(self, host: str, port: int, timeout_s: float, logger: ExtendedLogger) -> None:
        self.base = f"http://{host}:{port}"
        self.timeout_s = timeout_s
        self.logger = logger

    def request(self, path: str, method: str = "GET", data: bytes | None = None,
                headers: dict[str, str] | None = None) -> HttpResponse:
        url = self.base + path
        req = urllib.request.Request(url, data=data, method=method, headers=headers or {})
        start = time.perf_counter_ns()
        self.logger.transcript("http", "request", data.decode("utf-8", "replace") if data else "",
                               method=method, url=url)
        try:
            with urllib.request.urlopen(req, timeout=self.timeout_s) as response:
                body = response.read()
                status = int(response.status)
                response_headers = dict(response.headers.items())
        except urllib.error.HTTPError as exc:
            body = exc.read()
            status = int(exc.code)
            response_headers = dict(exc.headers.items()) if exc.headers else {}
        except Exception as exc:
            elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
            self.logger.transcript(
                "http", "error", repr(exc), url=url, method=method,
                elapsed_ms=elapsed_ms, exception_type=type(exc).__name__,
            )
            raise
        elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        self.logger.transcript(
            "http", "response", body.decode("utf-8", "replace"),
            status=status, elapsed_ms=elapsed_ms, url=url, headers=response_headers,
        )
        return HttpResponse(status, response_headers, body, elapsed_ms)


class ScpiClient:
    def __init__(self, host: str, port: int, timeout_s: float, logger: ExtendedLogger) -> None:
        self.host = host
        self.port = port
        self.timeout_s = timeout_s
        self.logger = logger
        self.sock: socket.socket | None = None
        self.greeting = ""

    def __enter__(self) -> "ScpiClient":
        self.connect()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def connect(self) -> None:
        self.close()
        start = time.perf_counter_ns()
        try:
            self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout_s)
            self.sock.settimeout(self.timeout_s)
            self.greeting = self._read_until_idle(idle_s=0.15, total_timeout_s=self.timeout_s).strip()
            elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
            self.logger.transcript(
                "scpi", "greeting", self.greeting, host=self.host, port=self.port,
                elapsed_ms=elapsed_ms,
            )
        except Exception as exc:
            elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
            self.logger.transcript(
                "scpi", "error", repr(exc), host=self.host, port=self.port,
                elapsed_ms=elapsed_ms, exception_type=type(exc).__name__, phase="connect",
            )
            self.close()
            raise

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            finally:
                self.sock = None

    def _read_until_idle(self, idle_s: float = 0.12, total_timeout_s: float | None = None) -> str:
        if self.sock is None:
            raise RuntimeError("SCPI socket is not connected")
        deadline = time.monotonic() + (total_timeout_s or self.timeout_s)
        chunks: list[bytes] = []
        received_any = False
        while time.monotonic() < deadline:
            remaining = max(0.01, deadline - time.monotonic())
            self.sock.settimeout(min(idle_s if received_any else remaining, remaining))
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                if received_any:
                    break
                continue
            if not chunk:
                break
            received_any = True
            chunks.append(chunk)
        return b"".join(chunks).decode("utf-8", errors="replace")

    def query(self, command: str, response_timeout_s: float | None = None) -> tuple[str, float]:
        if self.sock is None:
            self.connect()
        assert self.sock is not None
        payload = (command.rstrip("\r\n") + "\n").encode("ascii", errors="strict")
        self.logger.transcript("scpi", "request", command)
        start = time.perf_counter_ns()
        try:
            self.sock.sendall(payload)
            response = self._read_until_idle(total_timeout_s=response_timeout_s or self.timeout_s)
        except Exception as exc:
            elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
            self.logger.transcript(
                "scpi", "error", repr(exc), command=command, elapsed_ms=elapsed_ms,
                exception_type=type(exc).__name__, phase="query",
            )
            raise
        elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
        response = response.strip("\r\n")
        self.logger.transcript("scpi", "response", response, command=command, elapsed_ms=elapsed_ms)
        return response, elapsed_ms
