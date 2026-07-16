from __future__ import annotations

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
library = root / "robot_framework" / "libraries" / "e_resistor_robot_library.py"
output = root / "robot_framework" / "docs" / "EResistorRobotLibrary.html"
raise SystemExit(subprocess.call([
    sys.executable, "-m", "robot.libdoc", str(library), str(output)
], cwd=root))
