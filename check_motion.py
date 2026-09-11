"""Validate the real-game motion trace emitted by the opt-in gameplay driver."""
import argparse
import csv
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
args = parser.parse_args()
rows = [list(map(int, row)) for row in csv.reader((args.directory / "motion-trace.csv").open())]
held = [row for row in rows if row[1]]
transitions = [i for i in range(1, len(held)) if held[i][2] != held[i - 1][2]]
assert len(transitions) >= 2, "Test did not traverse enough hexes"
# The final arrival may correctly switch to standing at an obstacle. Check
# between arrivals, excluding that terminal frame; any pause followed by
# another step remains inside this interval and fails the check.
moving = held[transitions[0]:transitions[-1]]
standing = sum(row[5] == 0 for row in moving)
max_jump = max(math.hypot(b[7] - a[7], b[8] - a[8]) for a, b in zip(rows, rows[1:]))
assert standing == 0, f"Walk/run animation reset to standing {standing} times between steps"
# Default manual pan is 288 px/s; include the 50 ms stalled-frame cap and
# integer-pixel rounding allowance (the follow camera alone is 240 px/s).
assert max_jump <= 17, f"Camera jumped {max_jump:.2f} pixels between rendered frames"
result = {
    "rendered_frames": len(rows),
    "hex_transitions": len(transitions),
    "standing_resets_between_steps": standing,
    "maximum_camera_step_pixels": round(max_jump, 3),
    "duration_ms": rows[-1][0] - rows[0][0],
}
(args.directory / "motion-validation.json").write_text(json.dumps(result, indent=2) + "\n")
print("PASS: smooth-motion trace:", json.dumps(result))
