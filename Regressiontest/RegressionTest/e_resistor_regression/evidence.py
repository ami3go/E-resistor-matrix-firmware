from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


MANIFEST_NAME = "evidence_manifest.sha256"
VERIFICATION_NAME = "evidence_manifest_verification.json"
INCOMPLETE_NAME = "RUN_INCOMPLETE"
COMPLETE_NAME = "RUN_COMPLETE"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _iter_manifest_files(root: Path, excluded: Iterable[str] = ()) -> list[Path]:
    excluded_names = {MANIFEST_NAME, VERIFICATION_NAME, *excluded}
    return [
        path
        for path in sorted(root.rglob("*"))
        if path.is_file() and path.name not in excluded_names
    ]


def write_evidence_manifest(root: Path, excluded: Iterable[str] = ()) -> Path:
    """Write SHA-256 hashes for every finalized evidence file.

    The manifest itself and its verification report are excluded to avoid circular
    hashes. ``RUN_INCOMPLETE`` should be removed before this function is called.
    """
    root = root.resolve()
    manifest_path = root / MANIFEST_NAME
    rows = [
        f"{sha256_file(path)}  {path.relative_to(root).as_posix()}"
        for path in _iter_manifest_files(root, excluded)
    ]
    manifest_path.write_text("\n".join(rows) + ("\n" if rows else ""), encoding="utf-8")
    return manifest_path


def verify_evidence_manifest(root: Path, manifest_path: Path | None = None) -> dict:
    root = root.resolve()
    manifest_path = manifest_path or (root / MANIFEST_NAME)
    failures: list[dict[str, str]] = []
    checked = 0
    if not manifest_path.exists():
        return {
            "verified": False,
            "checked_files": 0,
            "failures": [{"path": MANIFEST_NAME, "reason": "manifest missing"}],
        }

    for line_number, raw in enumerate(manifest_path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw.strip()
        if not line:
            continue
        expected, separator, relative = line.partition("  ")
        if not separator or len(expected) != 64 or not relative:
            failures.append({"path": f"line {line_number}", "reason": "invalid manifest syntax"})
            continue
        path = root / relative
        if not path.is_file():
            failures.append({"path": relative, "reason": "file missing"})
            continue
        checked += 1
        actual = sha256_file(path)
        if actual.lower() != expected.lower():
            failures.append({"path": relative, "reason": f"hash mismatch: {actual}"})

    return {
        "verified": not failures,
        "checked_files": checked,
        "manifest_sha256": sha256_file(manifest_path),
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "failures": failures,
    }


def finalize_evidence(root: Path, run_id: str, overall_status: str) -> dict:
    """Finalize completion markers, manifest, and verification report.

    Call only after all transcript/event writers have been flushed and no more
    evidence files will be modified.
    """
    root = root.resolve()
    incomplete = root / INCOMPLETE_NAME
    if incomplete.exists():
        incomplete.unlink()
    complete = root / COMPLETE_NAME
    complete.write_text(
        f"run_id={run_id}\nstatus={overall_status}\ncompleted_utc={datetime.now(timezone.utc).isoformat()}\n",
        encoding="utf-8",
    )
    manifest = write_evidence_manifest(root)
    verification = verify_evidence_manifest(root, manifest)
    (root / VERIFICATION_NAME).write_text(
        json.dumps(verification, indent=2, sort_keys=True), encoding="utf-8"
    )
    if not verification["verified"]:
        raise RuntimeError(f"Evidence manifest verification failed: {verification['failures']}")
    return verification
