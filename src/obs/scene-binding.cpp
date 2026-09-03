#include "obs/scene-binding.h"

#include "core/show.h"
#include "obs/persistence.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

namespace obsdmx {

SceneBinder::SceneBinder(Show &show) : show_(show) {}

SceneBinder::~SceneBinder()
{
	stop();
}

void SceneBinder::start()
{
	if (started_)
		return;
	obs_frontend_add_event_callback(&SceneBinder::onFrontendEvent, this);
	obs_frontend_add_save_callback(&SceneBinder::onSave, this);
	started_ = true;
}

void SceneBinder::stop()
{
	if (!started_)
		return;
	obs_frontend_remove_event_callback(&SceneBinder::onFrontendEvent, this);
	obs_frontend_remove_save_callback(&SceneBinder::onSave, this);
	started_ = false;
}

void SceneBinder::setOnReloaded(std::function<void()> callback)
{
	onReloaded_ = std::move(callback);
}

std::vector<SceneEntry> SceneBinder::currentScenes()
{
	std::vector<SceneEntry> scenes;

	obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; ++i) {
		obs_source_t *source = list.sources.array[i];
		const char *uuid = obs_source_get_uuid(source);
		const char *name = obs_source_get_name(source);
		if (uuid && name)
			scenes.push_back({uuid, name});
	}
	obs_frontend_source_list_free(&list);

	return scenes;
}

std::string SceneBinder::currentSceneUuid()
{
	obs_source_t *scene = obs_frontend_get_current_scene();
	if (!scene)
		return {};

	const char *uuid = obs_source_get_uuid(scene);
	std::string result = uuid ? uuid : "";
	obs_source_release(scene);
	return result;
}

void SceneBinder::applyCurrentScene()
{
	const std::string uuid = currentSceneUuid();
	if (uuid.empty())
		return;

	show_.activateScene(uuid, Show::Clock::now());

	// Une scene sans programme est un cas courant et volontaire ; c'est le
	// silence total qui serait deroutant si la lumiere ne suit pas.
	const auto binding = show_.bindingFor(uuid);
	blog(LOG_INFO, "[obs-dmx] scene %s -> programme '%s'", uuid.c_str(),
	     binding && !binding->programId.empty() ? binding->programId.c_str() : "(aucun)");
}

void SceneBinder::onFrontendEvent(obs_frontend_event event, void *data)
{
	auto *self = static_cast<SceneBinder *>(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		self->applyCurrentScene();
		break;

	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		// La collection vient d'etre chargee : l'interface doit se
		// reconstruire, puis la scene courante prendre effet.
		if (self->onReloaded_)
			self->onReloaded_();
		self->applyCurrentScene();
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
	case OBS_FRONTEND_EVENT_EXIT:
		// On eteint plutot que de laisser la derniere ambiance figee sur
		// scene apres la fermeture.
		self->show_.activateProgram({}, 0, Show::Clock::now());
		break;

	default:
		break;
	}
}

void SceneBinder::onSave(obs_data_t *saveData, bool saving, void *data)
{
	auto *self = static_cast<SceneBinder *>(data);

	if (saving) {
		saveShow(self->show_, saveData);
		return;
	}

	loadShow(self->show_, saveData);
	if (self->onReloaded_)
		self->onReloaded_();
	self->applyCurrentScene();
}

} // namespace obsdmx
