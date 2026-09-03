#!/usr/bin/env bash
# Test de bout en bout : OBS change de scene, on regarde le DMX qui sort.
set -uo pipefail
SCRATCH="$(cd "$(dirname "$0")" && pwd)"
CFG=~/.var/app/com.obsproject.Studio/config/obs-studio

flatpak kill com.obsproject.Studio 2>/dev/null; sleep 3
# Sans cela, l'arret force du test precedent fait apparaitre le dialogue de
# mode securise, qui bloque le demarrage.
rm -f "$CFG"/.sentinel/run_*

# Le bac a sable d'OBS ne voit pas /tmp de l'hote : le son de test doit vivre
# sous $HOME.
TONE="$CFG/obs-dmx-test-tone.wav"
python3 "$SCRATCH/make_tone.py" "$TONE" >/dev/null
python3 "$SCRATCH/setup_collection.py" "$TONE" || exit 1

python3 - "$CFG" <<'PY'
import json, pathlib, sys
cfg = pathlib.Path(sys.argv[1])
# Sortie DMX vers la loopback, pour que le recepteur du test la voie.
p = cfg / "plugin_config/obs-dmx/output.json"
d = json.loads(p.read_text())
d.update(protocol="artnet", host="127.0.0.1", universe=0, enabled=True)
p.write_text(json.dumps(d))
# obs-websocket sert a piloter les changements de scene.
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
