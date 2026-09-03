#include "core/dmx-engine.h"
#include "core/fixture-library.h"
#include "audio/obs-audio-tap.h"
#include "core/show.h"
#include "obs/scene-binding.h"
#include "ui/dmx-dock.h"

#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <obs-module.h>

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

/// Ces objets vivent aussi longtemps que le module. Le dock, lui, appartient a
/// OBS des qu'il est enregistre : on ne le detruit pas nous-memes.
std::unique_ptr<obsdmx::FixtureLibrary> g_library;
std::unique_ptr<obsdmx::DmxEngine> g_engine;
std::unique_ptr<obsdmx::Show> g_show;
std::unique_ptr<obsdmx::SceneBinder> g_binder;
std::unique_ptr<obsdmx::ObsAudioTap> g_audio;
obsdmx::DmxDock *g_dock = nullptr;
obs_hotkey_id g_blackoutHotkey = OBS_INVALID_HOTKEY_ID;

void loadFixtureLibrary()
{
	g_library = std::make_unique<obsdmx::FixtureLibrary>();

	char *path = obs_module_file("fixtures");
	if (!path) {
		blog(LOG_ERROR, "[obs-dmx] dossier de profils introuvable : la bibliotheque sera vide");
		return;
	}

	std::vector<std::string> warnings;
	const size_t count = g_library->loadDirectory(path, warnings);
	bfree(path);

	for (const auto &warning : warnings)
		blog(LOG_WARNING, "[obs-dmx] %s", warning.c_str());

	blog(LOG_INFO, "[obs-dmx] %zu profil(s) de projecteur charge(s)", count);
}

void onBlackoutHotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed || !g_dock)
		return;
	// Le raccourci bascule : un unique geste pour couper et pour rallumer.
	g_dock->setBlackout(!g_engine->blackout());
}

} // namespace

bool obs_module_load(void)
{
	loadFixtureLibrary();

	g_engine = std::make_unique<obsdmx::DmxEngine>();
	g_show = std::make_unique<obsdmx::Show>(*g_library);

	g_audio = std::make_unique<obsdmx::ObsAudioTap>();
	g_audio->start();

	g_dock = new obsdmx::DmxDock(*g_engine, *g_show, *g_library, *g_audio);
	if (!obs_frontend_add_dock_by_id("obs-dmx-dock", obs_module_text("Dock.Title"), g_dock)) {
		blog(LOG_ERROR, "[obs-dmx] echec de l'enregistrement du dock");
		delete g_dock;
		g_dock = nullptr;
		g_audio.reset();
		g_show.reset();
		g_engine.reset();
		g_library.reset();
		return false;
	}

	g_binder = std::make_unique<obsdmx::SceneBinder>(*g_show);
	g_binder->setOnReloaded([] {
		if (g_dock)
			g_dock->reloadFromShow();
	});
	g_binder->start();

	g_blackoutHotkey = obs_hotkey_register_frontend("obs-dmx.blackout", obs_module_text("Hotkey.Blackout"),
							&onBlackoutHotkey, nullptr);

	g_engine->start();

	blog(LOG_INFO, "[obs-dmx] charge (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	if (g_blackoutHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(g_blackoutHotkey);
		g_blackoutHotkey = OBS_INVALID_HOTKEY_ID;
	}

	// L'ordre compte : on coupe d'abord les evenements, puis le thread du
	// moteur, avant de liberer ce qu'ils utilisent.
	g_binder.reset();

	if (g_engine)
		g_engine->stop();

	// La prise audio s'arrete avant le moteur : le rappel temps reel ne doit
	// plus pouvoir arriver quand l'analyseur disparait.
	if (g_audio)
		g_audio->stop();

	g_dock = nullptr;
	g_audio.reset();
	g_engine.reset();
	g_show.reset();
	g_library.reset();

	blog(LOG_INFO, "[obs-dmx] decharge");
}
