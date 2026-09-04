#!/usr/bin/env bash
# End-to-end test: OBS switches scene, we watch the DMX that comes out.
set -uo pipefail
SCRATCH="$(cd "$(dirname "$0")" && pwd)"
CFG=~/.var/app/com.obsproject.Studio/config/obs-studio

if flatpak ps 2>/dev/null | grep -qi obsproject; then
  echo "OBS is already running: refusing to kill a session you may be working in." >&2
  exit 1
fi
flatpak kill com.obsproject.Studio 2>/dev/null; sleep 3
# Without this, force-killing OBS at the end of the previous test brings up the
# safe-mode dialog, which blocks start-up.
rm -f "$CFG"/.sentinel/run_*

# OBS's sandbox cannot see the host's /tmp, so the test tone must live under
# $HOME.
TONE="$CFG/obs-dmx-test-tone.wav"
python3 "$SCRATCH/make_tone.py" "$TONE" >/dev/null
python3 "$SCRATCH/setup_collection.py" "$TONE" || exit 1

python3 - "$CFG" <<'PY'
import json, pathlib, sys
cfg = pathlib.Path(sys.argv[1])
# DMX output to the loopback, so the test receiver can see it.
p = cfg / "plugin_config/obs-dmx/output.json"
d = json.loads(p.read_text())
d.update(protocol="artnet", host="127.0.0.1", universe=0, enabled=True)
p.write_text(json.dumps(d))
# obs-websocket is what drives the scene changes.
w = cfg / "plugin_config/obs-websocket/config.json"
ws = json.loads(w.read_text())
ws["server_enabled"] = True
w.write_text(json.dumps(ws, indent=2))
PY

PASS=$(python3 -c "import json;print(json.load(open('$CFG/plugin_config/obs-websocket/config.json'))['server_password'])")
setsid flatpak run com.obsproject.Studio --collection obs-dmx-test --minimize-to-tray \
  >"$SCRATCH/obs-e2e.log" 2>&1 &

for i in $(seq 1 60); do timeout 1 bash -c "</dev/tcp/127.0.0.1/4455" 2>/dev/null && break; sleep 0.5; done
sleep 2
python3 "$SCRATCH/e2e_scenes.py" "$PASS"; RESULT=$?

flatpak kill com.obsproject.Studio 2>/dev/null; sleep 2
rm -f "$CFG"/.sentinel/run_*
exit $RESULT
