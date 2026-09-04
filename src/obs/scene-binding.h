#pragma once

#include <obs-frontend-api.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace obsdmx {

class Show;

/// An OBS scene as we present it to the user.
struct SceneEntry {
	std::string uuid;
	std::string name;
};

/// Bridges OBS's events and the show: scene changes, saving and loading the
/// collection.
///
/// OBS's callbacks arrive on the GUI thread. The show takes its own lock, so
/// nothing special is needed here.
class SceneBinder {
public:
	explicit SceneBinder(Show &show);
	~SceneBinder();

	SceneBinder(const SceneBinder &) = delete;
	SceneBinder &operator=(const SceneBinder &) = delete;

	void start();
	void stop();

	/// Called after a collection is loaded, so the interface can rebuild
	/// itself.
	void setOnReloaded(std::function<void()> callback);

	/// Scenes of the current collection, in OBS's own order.
	static std::vector<SceneEntry> currentScenes();

	/// Identifier of the scene currently on air, empty if there is none.
	static std::string currentSceneUuid();

private:
	static void onFrontendEvent(obs_frontend_event event, void *data);
	static void onSave(obs_data_t *saveData, bool saving, void *data);

	void applyCurrentScene();

	Show &show_;
	std::function<void()> onReloaded_;
	std::string lastLoggedScene_;
	bool started_ = false;
};

} // namespace obsdmx
