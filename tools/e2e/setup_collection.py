import json, pathlib, os, uuid
cfg = pathlib.Path(os.path.expanduser("~/.var/app/com.obsproject.Studio/config/obs-studio"))
p = cfg / "basic/scenes/obs-dmx-test.json"
d = json.loads(p.read_text())

scenes = {s["name"]: s["uuid"] for s in d["sources"] if s.get("id") == "scene"}
assert {"Plateau", "Interview"} <= scenes.keys(), scenes

def look(f, i, mix, hue, sat, cct=5600.0):
    return {"fixture": f, "intensity": i, "color_mix": mix, "hue": hue,
            "saturation": sat, "cct": cct, "green_magenta": 0.0, "strobe_hz": 0.0}

d.setdefault("modules", {})["obs-dmx"] = {
    "fixtures": [
        {"id": "f1", "name": "T4c jardin", "profile": "aputure-amaran-t4c", "mode": "mode3",
         "universe": 0, "address": 1, "order": 0},
        {"id": "f2", "name": "T4c cour", "profile": "aputure-amaran-t4c", "mode": "mode3",
         "universe": 0, "address": 10, "order": 1},
    ],
    "programs": [
        {"id": "program-1", "name": "Plateau chaud",
         "looks": [look("f1", 1.0, 0.0, 0.0, 0.0, 3200.0), look("f2", 1.0, 0.0, 0.0, 0.0, 3200.0)]},
        {"id": "program-2", "name": "Interview bleue",
         "looks": [look("f1", 0.6, 1.0, 240.0, 1.0), look("f2", 0.6, 1.0, 240.0, 1.0)]},
    ],
    "bindings": [
        {"scene_uuid": scenes["Plateau"], "scene_name": "Plateau", "program": "program-1", "fade_ms": 0},
        {"scene_uuid": scenes["Interview"], "scene_name": "Interview", "program": "program-2", "fade_ms": 0},
    ],
}
d["current_scene"] = d["current_program_scene"] = "Plateau"
p.write_text(json.dumps(d, indent=4))
print("montage installe pour", list(scenes))
