#include "obs/persistence.h"

#include "core/show.h"

#include <obs-module.h>

namespace obsdmx {

namespace {

obs_data_t *serializeLook(const FixtureLook &look)
{
	obs_data_t *item = obs_data_create();
	obs_data_set_string(item, "fixture", look.fixtureId.c_str());
	obs_data_set_double(item, "intensity", look.state.intensity);
	obs_data_set_double(item, "color_mix", look.state.colorMix);
	obs_data_set_double(item, "hue", look.state.hue);
	obs_data_set_double(item, "saturation", look.state.saturation);
	obs_data_set_double(item, "cct", look.state.cct);
	obs_data_set_double(item, "green_magenta", look.state.greenMagenta);
	obs_data_set_double(item, "strobe_hz", look.state.strobeHz);
	return item;
}

FixtureLook parseLook(obs_data_t *item)
{
	FixtureLook look;
	look.fixtureId = obs_data_get_string(item, "fixture");
	look.state.intensity = static_cast<float>(obs_data_get_double(item, "intensity"));
	look.state.hue = static_cast<float>(obs_data_get_double(item, "hue"));
	look.state.saturation = static_cast<float>(obs_data_get_double(item, "saturation"));
	look.state.greenMagenta = static_cast<float>(obs_data_get_double(item, "green_magenta"));
	look.state.strobeHz = static_cast<float>(obs_data_get_double(item, "strobe_hz"));

	// Valeurs par defaut explicites : un document ecrit par une version
	// anterieure n'a pas forcement ces champs, et 0 serait un mauvais choix
	// pour une temperature de couleur.
	look.state.colorMix = obs_data_has_user_value(item, "color_mix")
				      ? static_cast<float>(obs_data_get_double(item, "color_mix"))
				      : 1.0f;
	look.state.cct = obs_data_has_user_value(item, "cct") ? static_cast<float>(obs_data_get_double(item, "cct"))
							      : 5600.0f;
	return look;
}

} // namespace

void saveShow(const Show &show, obs_data_t *collectionData)
{
	obs_data_t *root = obs_data_create();

	// --- projecteurs ---
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

		obs_data_array_push_back(programs, item);
		obs_data_release(item);
	}
	obs_data_set_array(root, "programs", programs);
	obs_data_array_release(programs);

	// --- associations aux scenes ---
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

	// --- projecteurs ---
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

			programs.push_back(std::move(program));
			obs_data_release(item);
		}
		obs_data_array_release(array);
	}
	show.setPrograms(std::move(programs));

	// --- associations aux scenes ---
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
	blog(LOG_INFO, "[obs-dmx] montage charge : %zu projecteur(s), %zu programme(s), %zu association(s)",
	     fixtureCount, show.programs().size(), show.bindings().size());

	obs_data_release(root);
}

} // namespace obsdmx
