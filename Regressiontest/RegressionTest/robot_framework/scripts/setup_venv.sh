#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VENV="${1:-.venv}"
python3 -m venv "$ROOT/$VENV"
"$ROOT/$VENV/bin/python" -m pip install --upgrade pip
"$ROOT/$VENV/bin/python" -m pip install -r "$ROOT/requirements-robot.txt"
"$ROOT/$VENV/bin/python" -m pip install -e "$ROOT"
echo "Activate with: source $VENV/bin/activate"
