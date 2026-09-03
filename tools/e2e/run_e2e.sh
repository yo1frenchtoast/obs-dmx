#!/usr/bin/env bash
# Test de bout en bout : OBS change de scene, on regarde le DMX qui sort.
set -uo pipefail
SCRATCH="$(cd "$(dirname "$0")" && pwd)"
CFG=~/.var/app/com.obsproject.Studio/config/obs-studio

flatpak kill com.obsproject.Studio 2>/dev/null; sleep 3
# Sans cela, un arret force fait apparaitre le dialogue de mode securise, qui
# bloque le demarrage.
rm -f "$CFG"/.sentinel/run_*

python3 "$SCRATCH/setup_collection.py" || exit 1
python3 -c "
import json,pathlib,os
p=pathlib.Path(os.path.expanduser('$CFG/plugin_config/obs-dmx/output.json'))
d=json.loads(p.read_text()); d.update(protocol='artnet',host='127.0.0.1',universe=0,enabled=True)
p.write_text(json.dumps(d))"

PASS=$(python3 -c "import json;print(json.load(open('$CFG/plugin_config/obs-websocket/config.json'))['server_password'])")
setsid flatpak run com.obsproject.Studio --collection obs-dmx-test --minimize-to-tray \
  >"$SCRATCH/obs-e2e.log" 2>&1 &

for i in $(seq 1 60); do timeout 1 bash -c "</dev/tcp/127.0.0.1/4455" 2>/dev/null && break; sleep 0.5; done
sleep 2
python3 "$SCRATCH/e2e_scenes.py" "$PASS"; RESULT=$?

flatpak kill com.obsproject.Studio 2>/dev/null; sleep 2
rm -f "$CFG"/.sentinel/run_*
exit $RESULT
