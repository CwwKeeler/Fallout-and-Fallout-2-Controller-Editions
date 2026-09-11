"""Check real camera motion with both sticks, an edge cursor, and stationary pan."""
import argparse
import csv
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
args = parser.parse_args()
with (args.directory / "camera-trace.csv").open() as stream:
    rows = [list(map(float, row)) for row in csv.reader(stream)]
assert rows, "No camera frames recorded"
max_step = max(math.hypot(b[3]-a[3], b[4]-a[4]) for a, b in zip(rows, rows[1:]))
assert max_step <= 17, f"Stepped camera jump: {max_step:.2f} pixels"
total_pan = 0
for phase in (2, 4):
    held = [r for r in rows if r[1] == phase]
    assert len(held) >= 10, f"Missing manual pan phase {phase}"
    backwards = 0
    for a, b in zip(held, held[1:]):
        # A terrain anchor moves opposite to the camera; manual input must win
        # even when the player is moving the other way and the cursor is at an edge.
        projection = -(b[3]-a[3])*b[5] - (b[4]-a[4])*b[6]
        backwards += projection < -1
        total_pan += math.hypot(b[3]-a[3], b[4]-a[4])
    assert backwards == 0, f"Camera fought the right stick in phase {phase}"
    if phase == 2:
        assert len({r[2] for r in held}) >= 2, "Character did not move while panning"
    if phase == 4:
        assert len({r[2] for r in held}) == 1, "Stationary panning moved the character"
assert total_pan >= 10, "Manual panning did not move the view"
settled = [r for r in rows if r[1] == 5]
assert settled and settled[-1][0]-settled[0][0] >= 800
tail = [r for r in settled if r[0]-settled[0][0] >= 600]
assert len({(r[3], r[4]) for r in tail}) == 1, "Camera drifted after release while stationary"
assert (args.directory / "camera-legacy-check.log").read_text().startswith("PASS:")
result = {"frames": len(rows), "maximum_step_pixels": round(max_step, 3),
          "manual_pan_distance_pixels": round(total_pan, 3), "camera_reversals_against_input": 0,
          "stationary_release_drift": 0}
(args.directory / "camera-validation.json").write_text(json.dumps(result, indent=2) + "\n")
print("PASS: camera arbitration:", json.dumps(result))
