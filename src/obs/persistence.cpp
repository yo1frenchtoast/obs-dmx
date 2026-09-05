#include "obs/persistence.h"

#include "core/show.h"

#include <obs-module.h>

#include <algorithm>

namespace obsdmx {

namespace {

void serializeState(obs_data_t *item, const LightState &state)
{
	obs_data_set_double(item, "intensity", state.intensity);
	obs_data_set_double(item, "color_mix", state.colorMix);
	obs_data_set_double(item, "hue", state.hue);
	obs_data_set_double(item, "saturation", state.saturation);
	obs_data_set_double(item, "cct", state.cct);
	obs_data_set_double(item, "green_magenta", state.greenMagenta);
	obs_data_set_double(item, "strobe_hz", state.strobeHz);
}

LightState parseState(obs_data_t *item)
{
	LightState state;
	state.intensity = static_cast<float>(obs_data_get_double(item, "intensity"));
	state.hue = static_cast<float>(obs_data_get_double(item, "hue"));
	state.saturation = static_cast<float>(obs_data_get_double(item, "saturation"));
	state.greenMagenta = static_cast<float>(obs_data_get_double(item, "green_magenta"));
	state.strobeHz = static_cast<float>(obs_data_get_double(item, "strobe_hz"));

	// Explicit defaults: a document written by an earlier version may not have
	// these fields, and zero would be a poor choice for a colour temperature.
	state.colorMix = obs_data_has_user_value(item, "color_mix")
				 ? static_cast<float>(obs_data_get_double(item, "color_mix"))
				 : 1.0f;
	state.cct = obs_data_has_user_value(item, "cct") ? static_cast<float>(obs_data_get_double(item, "cct"))
							 : 5600.0f;
	return state;
}

obs_data_t *serializeEffect(const Effect &effect)
{
	obs_data_t *item = obs_data_create();
	obs_data_set_string(item, "id", effect.id.c_str());
	obs_data_set_string(item, "name", effect.name.c_str());
	obs_data_set_int(item, "type", static_cast<int>(effect.type));
	obs_data_set_bool(item, "enabled", effect.enabled);
	obs_data_set_int(item, "blend", static_cast<int>(effect.blend));

	obs_data_array_t *targets = obs_data_array_create();
	for (const auto &fixtureId : effect.fixtureIds) {
		obs_data_t *target = obs_data_create();
		obs_data_set_string(target, "fixture", fixtureId.c_str());
		obs_data_array_push_back(targets, target);
		obs_data_release(target);
	}
	obs_data_set_array(item, "fixtures", targets);
	obs_data_array_release(targets);

	obs_data_t *chaser = obs_data_create();
	obs_data_set_int(chaser, "step_ms", effect.chaser.stepMs);
	obs_data_set_int(chaser, "timing", static_cast<int>(effect.chaser.timing));
	obs_data_set_double(chaser, "bpm", effect.chaser.bpm);
	obs_data_set_double(chaser, "fade_ratio", effect.chaser.fadeRatio);
	obs_data_set_int(chaser, "direction", static_cast<int>(effect.chaser.direction));
	obs_data_array_t *steps = obs_data_array_create();
	for (const auto &step : effect.chaser.steps) {
		obs_data_t *stepItem = obs_data_create();
		serializeState(stepItem, step);
		obs_data_array_push_back(steps, stepItem);
		obs_data_release(stepItem);
	}
	obs_data_set_array(chaser, "steps", steps);
	obs_data_array_release(steps);
	obs_data_set_obj(item, "chaser", chaser);
	obs_data_release(chaser);

	obs_data_t *strobe = obs_data_create();
	obs_data_set_double(strobe, "hz", effect.strobe.hz);
	obs_data_set_double(strobe, "duty", effect.strobe.dutyCycle);
	obs_data_set_bool(strobe, "use_base_color", effect.strobe.useBaseColor);
	obs_data_set_bool(strobe, "prefer_hardware", effect.strobe.preferHardware);
	serializeState(strobe, effect.strobe.color);
	obs_data_set_obj(item, "strobe", strobe);
	obs_data_release(strobe);

	obs_data_t *sound = obs_data_create();
	obs_data_set_int(sound, "target", static_cast<int>(effect.sound.target));
	obs_data_set_int(sound, "band", effect.sound.band);
	obs_data_set_double(sound, "sensitivity", effect.sound.sensitivity);
	obs_data_set_double(sound, "threshold", effect.sound.threshold);
	obs_data_set_double(sound, "smoothing_ms", effect.sound.smoothingMs);
	obs_data_set_bool(sound, "use_base_color", effect.sound.useBaseColor);
	serializeState(sound, effect.sound.color);
	obs_data_set_obj(item, "sound", sound);
	obs_data_release(sound);

	obs_data_t *builtin = obs_data_create();
	obs_data_set_string(builtin, "effect", effect.builtin.effectId.c_str());
	obs_data_set_int(builtin, "frequency", effect.builtin.frequency);
	obs_data_set_int(builtin, "variant", effect.builtin.variant);
	obs_data_set_bool(builtin, "use_manual", effect.builtin.useManual);

	obs_data_array_t *manual = obs_data_array_create();
	for (const auto &entry : effect.builtin.manual) {
		obs_data_t *manualItem = obs_data_create();
		obs_data_set_int(manualItem, "channel", entry.channel);
		obs_data_set_int(manualItem, "value", entry.value);
		obs_data_array_push_back(manual, manualItem);
		obs_data_release(manualItem);
	}
	obs_data_set_array(builtin, "manual", manual);
	obs_data_array_release(manual);

	obs_data_set_obj(item, "builtin", builtin);
	obs_data_release(builtin);

	return item;
}

Effect parseEffect(obs_data_t *item)
{
	Effect effect;
	effect.id = obs_data_get_string(item, "id");
	effect.name = obs_data_get_string(item, "name");
	effect.type = static_cast<EffectType>(obs_data_get_int(item, "type"));
	effect.enabled = obs_data_get_bool(item, "enabled");
	effect.blend = static_cast<BlendMode>(obs_data_get_int(item, "blend"));

	if (obs_data_array_t *targets = obs_data_get_array(item, "fixtures")) {
		const size_t count = obs_data_array_count(targets);
		for (size_t i = 0; i < count; ++i) {
			obs_data_t *target = obs_data_array_item(targets, i);
			effect.fixtureIds.emplace_back(obs_data_get_string(target, "fixture"));
			obs_data_release(target);
		}
		obs_data_array_release(targets);
	}

	if (obs_data_t *chaser = obs_data_get_obj(item, "chaser")) {
		effect.chaser.stepMs = static_cast<int>(obs_data_get_int(chaser, "step_ms"));

		// Documents written before the third timing mode carry a use_bpm flag
		// instead.
		if (obs_data_has_user_value(chaser, "timing"))
			effect.chaser.timing = static_cast<ChaserTiming>(obs_data_get_int(chaser, "timing"));
		else
			effect.chaser.timing = obs_data_get_bool(chaser, "use_bpm") ? ChaserTiming::Bpm
										   : ChaserTiming::Duration;
		effect.chaser.bpm = static_cast<float>(obs_data_get_double(chaser, "bpm"));
		effect.chaser.fadeRatio = static_cast<float>(obs_data_get_double(chaser, "fade_ratio"));
		effect.chaser.direction = static_cast<ChaserDirection>(obs_data_get_int(chaser, "direction"));
		if (obs_data_array_t *steps = obs_data_get_array(chaser, "steps")) {
			const size_t count = obs_data_array_count(steps);
			for (size_t i = 0; i < count; ++i) {
				obs_data_t *stepItem = obs_data_array_item(steps, i);
				effect.chaser.steps.push_back(parseState(stepItem));
				obs_data_release(stepItem);
			}
			obs_data_array_release(steps);
		}
		obs_data_release(chaser);
	}

	if (obs_data_t *strobe = obs_data_get_obj(item, "strobe")) {
		effect.strobe.hz = static_cast<float>(obs_data_get_double(strobe, "hz"));
		effect.strobe.dutyCycle = static_cast<float>(obs_data_get_double(strobe, "duty"));
		effect.strobe.useBaseColor = obs_data_get_bool(strobe, "use_base_color");
		effect.strobe.preferHardware = obs_data_get_bool(strobe, "prefer_hardware");
		effect.strobe.color = parseState(strobe);
		obs_data_release(strobe);
	}

	if (obs_data_t *sound = obs_data_get_obj(item, "sound")) {
		// Value 2 was a sound target that claimed to step a chase and never
		// did. Anything saved with it falls back to following the level, and
		// says so, since the setting has moved onto the chase itself.
		const int savedTarget = static_cast<int>(obs_data_get_int(sound, "target"));
		if (savedTarget == 2) {
			blog(LOG_WARNING,
			     "[obs-dmx] effect '%s' used the old 'step the chase on the beat' setting, "
			     "which never worked; it now follows the level. The setting lives on the "
			     "chase itself.",
			     effect.name.c_str());
			effect.sound.target = SoundTarget::Intensity;
		} else {
			effect.sound.target = static_cast<SoundTarget>(savedTarget);
		}
		effect.sound.band = static_cast<int>(obs_data_get_int(sound, "band"));
		effect.sound.sensitivity = static_cast<float>(obs_data_get_double(sound, "sensitivity"));
		effect.sound.threshold = static_cast<float>(obs_data_get_double(sound, "threshold"));
		effect.sound.smoothingMs = static_cast<float>(obs_data_get_double(sound, "smoothing_ms"));
		// Effects saved before this setting followed the programme's colour,
		// so that is the fallback.
		effect.sound.useBaseColor = !obs_data_has_user_value(sound, "use_base_color") ||
					    obs_data_get_bool(sound, "use_base_color");
		effect.sound.color = parseState(sound);
		obs_data_release(sound);
	}

	if (obs_data_t *builtin = obs_data_get_obj(item, "builtin")) {
		effect.builtin.effectId = obs_data_get_string(builtin, "effect");
		effect.builtin.frequency = static_cast<int>(obs_data_get_int(builtin, "frequency"));
		effect.builtin.variant = static_cast<int>(obs_data_get_int(builtin, "variant"));
		effect.builtin.useManual = obs_data_get_bool(builtin, "use_manual");

		if (obs_data_array_t *manual = obs_data_get_array(builtin, "manual")) {
			const size_t count = obs_data_array_count(manual);
			for (size_t k = 0; k < count; ++k) {
				obs_data_t *manualItem = obs_data_array_item(manual, k);
				ManualChannel entry;
				entry.channel = static_cast<int>(obs_data_get_int(manualItem, "channel"));
				entry.value = static_cast<uint8_t>(
					std::clamp<long long>(obs_data_get_int(manualItem, "value"), 0, 255));
				effect.builtin.manual.push_back(entry);
				obs_data_release(manualItem);
			}
			obs_data_array_release(manual);
		}
		obs_data_release(builtin);
	}

	return effect;
}

obs_data_t *serializeLook(const FixtureLook &look)
{
	obs_data_t *item = obs_data_create();
	obs_data_set_string(item, "fixture", look.fixtureId.c_str());
	serializeState(item, look.state);
	return item;
}

FixtureLook parseLook(obs_data_t *item)
{
	FixtureLook look;
	look.fixtureId = obs_data_get_string(item, "fixture");
	look.state = parseState(item);
	return look;
}

} // namespace

void saveShow(const Show &show, obs_data_t *collectionData)
{
	obs_data_t *root = obs_data_create();

	// --- fixtures ---
	obs_data_array_t *fixtures = obs_data_array_create();
	show.withPatch([&](const Patch &patch) {
		for (const auto &fixture : patch.fixtures()) {
			obs_data_t *item = obs_data_create();
			obs_data_set_string(item, "id", fixture.id.c_str());
			obs_data_set_string(item, "name", fixture.name.c_str());
			obs_data_set_string(item, "profile", fixture.profileId.c_str());
			obs_data_set_string(item, "mode", fixture.modeId.c_str());
			obs_data_set_int(item, "universe", fixture.universe);
			obs_data_set_int(item, "address", fixture.address);
			obs_data_set_int(item, "order", fixture.order);
			obs_data_array_push_back(fixtures, item);
			obs_data_release(item);
		}
	});
	obs_data_set_array(root, "fixtures", fixtures);
	obs_data_array_release(fixtures);

	// --- programmes ---
	obs_data_array_t *programs = obs_data_array_create();
	for (const auto &program : show.programs()) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "id", program.id.c_str());
		obs_data_set_string(item, "name", program.name.c_str());

		obs_data_array_t *looks = obs_data_array_create();
		for (const auto &look : program.looks) {
			obs_data_t *lookItem = serializeLook(look);
			obs_data_array_push_back(looks, lookItem);
			obs_data_release(lookItem);
		}
		obs_data_set_array(item, "looks", looks);
		obs_data_array_release(looks);

		obs_data_array_t *effects = obs_data_array_create();
		for (const auto &effect : program.effects) {
			obs_data_t *effectItem = serializeEffect(effect);
			obs_data_array_push_back(effects, effectItem);
			obs_data_release(effectItem);
		}
		obs_data_set_array(item, "effects", effects);
		obs_data_array_release(effects);

		obs_data_array_push_back(programs, item);
		obs_data_release(item);
	}
	obs_data_set_array(root, "programs", programs);
	obs_data_array_release(programs);

	// --- scene attachments ---
	obs_data_array_t *bindings = obs_data_array_create();
	for (const auto &binding : show.bindings()) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "scene_uuid", binding.sceneUuid.c_str());
		obs_data_set_string(item, "scene_name", binding.sceneName.c_str());
		obs_data_set_string(item, "program", binding.programId.c_str());
		obs_data_set_int(item, "fade_ms", binding.fadeMs);
		obs_data_array_push_back(bindings, item);
		obs_data_release(item);
	}
	obs_data_set_array(root, "bindings", bindings);
	obs_data_array_release(bindings);

	obs_data_set_obj(collectionData, kShowDataKey, root);
	obs_data_release(root);
}

void loadShow(Show &show, obs_data_t *collectionData)
{
	show.clear();

	obs_data_t *root = obs_data_get_obj(collectionData, kShowDataKey);
	if (!root)
		return;

	// --- fixtures ---
	if (obs_data_array_t *fixtures = obs_data_get_array(root, "fixtures")) {
		show.withPatch([&](Patch &patch) {
			const size_t count = obs_data_array_count(fixtures);
			for (size_t i = 0; i < count; ++i) {
				obs_data_t *item = obs_data_array_item(fixtures, i);
				Fixture fixture;
				fixture.id = obs_data_get_string(item, "id");
				fixture.name = obs_data_get_string(item, "name");
				fixture.profileId = obs_data_get_string(item, "profile");
				fixture.modeId = obs_data_get_string(item, "mode");
				fixture.universe = static_cast<uint16_t>(obs_data_get_int(item, "universe"));
				fixture.address = static_cast<int>(obs_data_get_int(item, "address"));
				fixture.order = static_cast<int>(obs_data_get_int(item, "order"));
				patch.add(std::move(fixture));
				obs_data_release(item);
			}
		});
		obs_data_array_release(fixtures);
	}

	// --- programmes ---
	std::vector<Program> programs;
	if (obs_data_array_t *array = obs_data_get_array(root, "programs")) {
		const size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; ++i) {
			obs_data_t *item = obs_data_array_item(array, i);

			Program program;
			program.id = obs_data_get_string(item, "id");
			program.name = obs_data_get_string(item, "name");

			if (obs_data_array_t *looks = obs_data_get_array(item, "looks")) {
				const size_t lookCount = obs_data_array_count(looks);
				for (size_t j = 0; j < lookCount; ++j) {
					obs_data_t *lookItem = obs_data_array_item(looks, j);
					program.looks.push_back(parseLook(lookItem));
					obs_data_release(lookItem);
				}
				obs_data_array_release(looks);
			}

			if (obs_data_array_t *effects = obs_data_get_array(item, "effects")) {
				const size_t effectCount = obs_data_array_count(effects);
				for (size_t j = 0; j < effectCount; ++j) {
					obs_data_t *effectItem = obs_data_array_item(effects, j);
					program.effects.push_back(parseEffect(effectItem));
					obs_data_release(effectItem);
				}
				obs_data_array_release(effects);
			}

			programs.push_back(std::move(program));
			obs_data_release(item);
		}
		obs_data_array_release(array);
	}
	show.setPrograms(std::move(programs));

	// --- scene attachments ---
	std::vector<SceneBinding> bindings;
	if (obs_data_array_t *array = obs_data_get_array(root, "bindings")) {
		const size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; ++i) {
			obs_data_t *item = obs_data_array_item(array, i);
			SceneBinding binding;
			binding.sceneUuid = obs_data_get_string(item, "scene_uuid");
			binding.sceneName = obs_data_get_string(item, "scene_name");
			binding.programId = obs_data_get_string(item, "program");
			binding.fadeMs = static_cast<int>(obs_data_get_int(item, "fade_ms"));
			bindings.push_back(std::move(binding));
			obs_data_release(item);
		}
		obs_data_array_release(array);
	}
	show.setBindings(std::move(bindings));

	const size_t fixtureCount = show.withPatch([](const Patch &patch) { return patch.fixtures().size(); });
	blog(LOG_INFO, "[obs-dmx] rig loaded: %zu fixture(s), %zu programme(s), %zu attachment(s)",
	     fixtureCount, show.programs().size(), show.bindings().size());

	obs_data_release(root);
}

} // namespace obsdmx
