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

/// These objects live as long as the module. The dock belongs to OBS the moment
/// it is registered, so we never destroy it ourselves.
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

	// Profiles carry their own labels and so cannot go through the module's
	// translation file: we tell them which language to pick among those they
	// declare.
	if (const char *locale = obs_get_locale())
		g_library->setLanguage(locale);

	char *path = obs_module_file("fixtures");
	if (!path) {
		blog(LOG_ERROR, "[obs-dmx] fixture directory not found: the library will be empty");
		return;
	}

	std::vector<std::string> warnings;
	const size_t count = g_library->loadDirectory(path, warnings);
	bfree(path);

	for (const auto &warning : warnings)
		blog(LOG_WARNING, "[obs-dmx] %s", warning.c_str());

	blog(LOG_INFO, "[obs-dmx] %zu fixture profile(s) loaded", count);
}

void onBlackoutHotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed || !g_dock)
		return;
	// The hotkey toggles: one gesture to kill the lights and to bring them back.
	g_dock->setBlackout(!g_engine->blackout());
}

} // namespace

bool obs_module_load(void)
{
	loadFixtureLibrary();

	g_engine = std::make_unique<obsdmx::DmxEngine>();
	g_show = std::make_unique<obsdmx::Show>(*g_library);

	g_audio = std::make_unique<obsdmx::ObsAudioTap>();
	g_audio->loadSettings();
	g_audio->start();

	g_dock = new obsdmx::DmxDock(*g_engine, *g_show, *g_library, *g_audio);
	if (!obs_frontend_add_dock_by_id("obs-dmx-dock", obs_module_text("Dock.Title"), g_dock)) {
		blog(LOG_ERROR, "[obs-dmx] failed to register the dock");
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

	blog(LOG_INFO, "[obs-dmx] loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	if (g_blackoutHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(g_blackoutHotkey);
		g_blackoutHotkey = OBS_INVALID_HOTKEY_ID;
	}

	// Order matters: stop the events first, then the engine thread, before
	// releasing what they use.
	g_binder.reset();

	if (g_engine)
		g_engine->stop();

	// The audio tap stops before the engine: the real-time callback must no
	// longer be able to fire once the analyser goes away.
	if (g_audio)
		g_audio->stop();

	g_dock = nullptr;
	g_audio.reset();
	g_engine.reset();
	g_show.reset();
	g_library.reset();

	blog(LOG_INFO, "[obs-dmx] unloaded");
}
