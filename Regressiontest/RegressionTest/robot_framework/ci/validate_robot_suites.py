from __future__ import annotations

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
output = root / "robot_dryrun_output"
command = [
    sys.executable, "-m", "robot",
    "--pythonpath", str(root),
    "--dryrun",
    "--outputdir", str(output),
    "--output", "NONE",
    "--log", "NONE",
    "--report", "NONE",
    str(root / "robot_framework" / "suites"),
]
completed = subprocess.run(command, cwd=root, check=False)
raise SystemExit(completed.returncode)
