"""Installs a throwaway scene collection for the end-to-end test."""
import json, pathlib, os, sys, uuid

cfg = pathlib.Path(os.path.expanduser("~/.var/app/com.obsproject.Studio/config/obs-studio"))
p = cfg / "basic/scenes/obs-dmx-test.json"
tone = sys.argv[1] if len(sys.argv) > 1 else ""

base = json.loads((cfg / "basic/scenes/Sans_nom.json").read_text())

def source(name, sid, settings, extra=None):
    d = {"prev_ver": base.get("version", 0), "name": name, "uuid": str(uuid.uuid4()),
         "id": sid, "versioned_id": sid, "settings": settings,
         "mixers": 255, "sync": 0, "flags": 0, "volume": 1.0, "balance": 0.5, "enabled": True,
         "muted": False, "push-to-mute": False, "push-to-mute-delay": 0, "push-to-talk": False,
         "push-to-talk-delay": 0, "hotkeys": {}, "deinterlace_mode": 0, "deinterlace_field_order": 0,
         "monitoring_type": 0, "private_settings": {}}
    if extra: d.update(extra)
    return d

def scene(name, items=()):
    return source(name, "scene", {"custom_size": False, "id_counter": len(items),
                                  "items": list(items)})

# An audio source playing on a loop: the mix sees it, the speakers do not,
# monitoring being left off.
media = source("Basse de test", "ffmpeg_source", {
    "local_file": tone, "looping": True, "restart_on_activate": True,
    "close_when_inactive": False, "hw_decode": False,
})

def item(src, ident):
    return {"name": src["name"], "source_uuid": src["uuid"], "visible": True, "locked": False,
            "rot": 0.0, "scale_ref": {"x": 1.0, "y": 1.0}, "align": 5, "bounds_type": 0,
            "bounds_align": 0, "bounds": {"x": 0.0, "y": 0.0}, "scale": {"x": 1.0, "y": 1.0},
            "pos": {"x": 0.0, "y": 0.0}, "crop_left": 0, "crop_top": 0, "crop_right": 0,
            "crop_bottom": 0, "id": ident, "group_item_backup": False,
            "private_settings": {}, "blend_method": "default", "blend_type": "normal",
            "hide_transition": {"duration": 0}, "show_transition": {"duration": 0}}

plateau = scene("Plateau")
interview = scene("Interview")
chase = scene("Chase")
strobe = scene("Strobe")
fx = scene("EffetIntegre")
musique = scene("Musique", [item(media, 1)] if tone else [])
# Same programme as Musique, but with no sound source: the control that lets us
# claim it really is the sound lighting the fixtures.
silence = scene("Silence")
# Direct channel entry, for a fixture whose profile does not know its effects:
# here channel 5 is forced to 200 on a T4c in mode 3.
fxmanuel = scene("FxManuel")

scenes = [plateau, interview, chase, strobe, fx, musique, silence, fxmanuel]

def look(f, i, mix, hue, sat, cct=5600.0):
    return {"fixture": f, "intensity": i, "color_mix": mix, "hue": hue,
            "saturation": sat, "cct": cct, "green_magenta": 0.0, "strobe_hz": 0.0}

def effect(eid, etype, fixtures, **kw):
    e = {"id": eid, "name": eid, "type": etype, "enabled": True, "blend": 1,
         "fixtures": [{"fixture": f} for f in fixtures],
         "chaser": {"step_ms": 500, "use_bpm": False, "bpm": 120, "fade_ratio": 0.0,
                    "direction": 0, "steps": []},
         "strobe": {"hz": 8.0, "duty": 0.5, "use_base_color": True, "prefer_hardware": True,
                    **look("", 1.0, 1.0, 0.0, 1.0)},
         "sound": {"target": 0, "band": 0, "sensitivity": 1.0, "threshold": 0.05,
                   "smoothing_ms": 120, **look("", 1.0, 1.0, 0.0, 1.0)},
         "builtin": {"effect": "", "frequency": 5, "variant": 0}}
    for key, value in kw.items():
        if isinstance(value, dict) and key in e:
            e[key].update(value)
        else:
            e[key] = value
    return e

fixtures = ["f1", "f2", "f3", "f4"]
allume = look("", 1.0, 1.0, 0.0, 1.0); allume.pop("fixture")
eteint = dict(allume, intensity=0.0)

show = {
    "fixtures": [
        {"id": f"f{i+1}", "name": f"T4c {i+1}", "profile": "aputure-amaran-t4c", "mode": "mode3",
         "universe": 0, "address": 1 + i * 10, "order": i} for i in range(4)
    ] + [
        {"id": "fx1", "name": "T4c effets", "profile": "aputure-amaran-t4c", "mode": "mode7",
         "universe": 0, "address": 100, "order": 4}
    ],
    "programs": [
        {"id": "program-1", "name": "Plateau chaud",
         "looks": [look(f, 1.0, 0.0, 0.0, 0.0, 3200.0) for f in fixtures]},
        {"id": "program-2", "name": "Interview bleue",
         "looks": [look(f, 0.6, 1.0, 240.0, 1.0) for f in fixtures]},
        {"id": "program-3", "name": "Chase",
         "looks": [],
         "effects": [effect("chaser", 0, fixtures,
                            chaser={"steps": [allume, eteint], "step_ms": 500})]},
        {"id": "program-4", "name": "Strobe sur fond bleu",
         "looks": [look(f, 0.5, 1.0, 240.0, 1.0) for f in fixtures],
         "effects": [effect("strobe", 1, fixtures,
                            strobe={"hz": 12.0, "use_base_color": True, "prefer_hardware": True})]},
        {"id": "program-5", "name": "Orage embarque",
         "looks": [],
         "effects": [effect("fx", 3, ["fx1"], builtin={"effect": "lightning", "frequency": 4})]},
        {"id": "program-7", "name": "Canaux a la main",
         "looks": [],
         "effects": [effect("manuel", 3, ["f1"],
                            builtin={"use_manual": True,
                                     "manual": [{"channel": 5, "value": 200},
                                                {"channel": 6, "value": 111},
                                                {"channel": 99, "value": 255}]})]},
        {"id": "program-6", "name": "Suit la musique",
         "looks": [],
         "effects": [effect("son", 2, fixtures, blend=0,
                            sound={"target": 0, "band": 0, "sensitivity": 1.0, "threshold": 0.02,
                                   "use_base_color": False, "hue": 300.0, "saturation": 1.0,
                                   "color_mix": 1.0, "intensity": 1.0, "cct": 5600.0,
                                   "green_magenta": 0.0, "strobe_hz": 0.0})]},
    ],
    "bindings": [
        {"scene_uuid": plateau["uuid"], "scene_name": "Plateau", "program": "program-1", "fade_ms": 0},
        {"scene_uuid": interview["uuid"], "scene_name": "Interview", "program": "program-2", "fade_ms": 0},
        {"scene_uuid": chase["uuid"], "scene_name": "Chase", "program": "program-3", "fade_ms": 0},
        {"scene_uuid": strobe["uuid"], "scene_name": "Strobe", "program": "program-4", "fade_ms": 0},
        {"scene_uuid": fx["uuid"], "scene_name": "EffetIntegre", "program": "program-5", "fade_ms": 0},
        {"scene_uuid": musique["uuid"], "scene_name": "Musique", "program": "program-6", "fade_ms": 0},
        {"scene_uuid": silence["uuid"], "scene_name": "Silence", "program": "program-6", "fade_ms": 0},
        {"scene_uuid": fxmanuel["uuid"], "scene_name": "FxManuel", "program": "program-7", "fade_ms": 0},
    ],
}

collection = dict(base)

# Mute the global audio inputs: they are mixed whatever scene is active, and
# room noise would make the silent control meaningless.
for key in ("DesktopAudioDevice1", "AuxAudioDevice1"):
    device = collection.get(key)
    if isinstance(device, dict):
        device = dict(device)
        device["muted"] = True
        device["volume"] = 0.0
        collection[key] = device

collection.update({
    "name": "obs-dmx-test",
    "sources": scenes + ([media] if tone else []),
    "scene_order": [{"name": s["name"]} for s in scenes],
    "current_scene": "Plateau", "current_program_scene": "Plateau",
    "modules": {**base.get("modules", {}), "obs-dmx": show},
})
p.write_text(json.dumps(collection, indent=4))
print("collection installed :", [s["name"] for s in scenes])
