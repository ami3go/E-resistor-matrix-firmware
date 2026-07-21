#!/usr/bin/env python3
"""Check source delimiter balance while ignoring comments and literals."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "validation" / "cpp_lexical_validation.json"


def strip_comments_and_literals(text: str) -> tuple[str, str]:
    out: list[str] = []
    index = 0
    state = "code"
    quote = ""
    while index < len(text):
        char = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if state == "code":
            if char == "/" and following == "/":
                state = "line"; out.extend("  "); index += 2; continue
            if char == "/" and following == "*":
                state = "block"; out.extend("  "); index += 2; continue
            if char in {'"', "'"}:
                state = "literal"; quote = char; out.append(" "); index += 1; continue
            out.append(char); index += 1; continue
        if state == "line":
            if char == "\n": state = "code"; out.append("\n")
            else: out.append(" ")
            index += 1; continue
        if state == "block":
            if char == "*" and following == "/":
                state = "code"; out.extend("  "); index += 2
            else:
                out.append("\n" if char == "\n" else " "); index += 1
            continue
        if state == "literal":
            if char == "\\": out.extend("  "); index += 2; continue
            if char == quote: state = "code"
            out.append("\n" if char == "\n" else " "); index += 1
    return "".join(out), state


def main() -> int:
    records = []
    sources = sorted([*ROOT.glob("*.cpp"), *ROOT.glob("*.h"), *ROOT.glob("*.ino")])
    closing = {")": "(", "]": "[", "}": "{"}
    for path in sources:
        text, state = strip_comments_and_literals(path.read_text(encoding="utf-8"))
        stack: list[tuple[str, int]] = []
        errors: list[str] = []
        for index, char in enumerate(text):
            if char in "([{": stack.append((char, index))
            elif char in ")]}":
                if not stack or stack[-1][0] != closing[char]:
                    errors.append(f"unmatched {char} at {index}")
                else:
                    stack.pop()
        errors.extend(f"unclosed {char} at {index}" for char, index in stack[-10:])
        if state not in {"code", "line"}: errors.append(f"unterminated lexical state {state}")
        records.append({"file": path.name, "passed": not errors, "errors": errors})
    payload = {
        "gate": "G4",
        "files": len(records),
        "passed": sum(bool(item["passed"]) for item in records),
        "failed": sum(not bool(item["passed"]) for item in records),
        "records": records,
    }
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"{payload['passed']}/{payload['files']} source files passed")
    return 0 if payload["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
