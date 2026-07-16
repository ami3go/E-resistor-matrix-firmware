from __future__ import annotations

import hashlib
from pathlib import Path


def write_sha256_manifest(
    output_dir: Path,
    manifest_name: str = "evidence_manifest.sha256",
) -> Path:
    """Hash every finalized evidence file except the manifest itself."""
    output_dir = Path(output_dir)
    manifest_path = output_dir / manifest_name
    rows: list[str] = []
    for path in sorted(output_dir.rglob("*")):
        if not path.is_file() or path == manifest_path:
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        rows.append(f"{digest}  {path.relative_to(output_dir).as_posix()}")
    manifest_path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return manifest_path


def verify_sha256_manifest(
    output_dir: Path,
    manifest_name: str = "evidence_manifest.sha256",
) -> list[str]:
    """Return integrity errors; an empty list means the manifest is valid."""
    output_dir = Path(output_dir)
    manifest_path = output_dir / manifest_name
    if not manifest_path.is_file():
        return [f"missing manifest: {manifest_name}"]

    errors: list[str] = []
    for line_number, raw in enumerate(
        manifest_path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw.strip()
        if not line:
            continue
        try:
            expected, relative = line.split(None, 1)
        except ValueError:
            errors.append(f"line {line_number}: malformed entry")
            continue
        relative = relative.strip().lstrip("*")
        path = output_dir / relative
        if not path.is_file():
            errors.append(f"missing file: {relative}")
            continue
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual.lower() != expected.lower():
            errors.append(f"hash mismatch: {relative}")
    return errors
