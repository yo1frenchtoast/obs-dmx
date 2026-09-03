#include "core/dmx-engine.h"
#include "ui/dmx-dock.h"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <memory>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-dmx", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Plugin.Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("Plugin.Name");
}

namespace {

/// Le moteur vit aussi longtemps que le module. Le dock, lui, appartient a OBS
/// des qu'il est enregistre : on ne le detruit pas nous-memes.
std::unique_ptr<obsdmx::DmxEngine> g_engine;

} // namespace

bool obs_module_load(void)
{
	g_engine = std::make_unique<obsdmx::DmxEngine>();
	g_engine->start();

	auto *dock = new obsdmx::DmxDock(*g_engine);
	if (!obs_frontend_add_dock_by_id("obs-dmx-dock", obs_module_text("Dock.Title"), dock)) {
		blog(LOG_ERROR, "[obs-dmx] echec de l'enregistrement du dock");
		delete dock;
		g_engine->stop();
		g_engine.reset();
		return false;
	}

	blog(LOG_INFO, "[obs-dmx] charge (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	// OBS a deja detruit le dock a ce stade. On arrete le moteur pour que le
	// thread ne survive pas au dechargement du module.
	if (g_engine) {
		g_engine->stop();
		g_engine.reset();
	}
	blog(LOG_INFO, "[obs-dmx] decharge");
}
