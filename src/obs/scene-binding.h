#pragma once

#include <obs-frontend-api.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace obsdmx {

class Show;

/// Une scene OBS telle qu'on la presente a l'utilisateur.
struct SceneEntry {
	std::string uuid;
	std::string name;
};

/// Fait le lien entre les evenements d'OBS et le spectacle : changement de
/// scene, sauvegarde et chargement de la collection.
///
/// Les rappels d'OBS arrivent sur le thread graphique. Le spectacle prend son
/// propre verrou, il n'y a donc rien de particulier a faire ici.
class SceneBinder {
public:
	explicit SceneBinder(Show &show);
	~SceneBinder();

	SceneBinder(const SceneBinder &) = delete;
	SceneBinder &operator=(const SceneBinder &) = delete;

	void start();
	void stop();

	/// Appele apres un chargement de collection, pour que l'interface se
	/// reconstruise.
	void setOnReloaded(std::function<void()> callback);

	/// Scenes de la collection courante, dans l'ordre d'OBS.
	static std::vector<SceneEntry> currentScenes();

	/// Identifiant de la scene actuellement diffusee, vide s'il n'y en a pas.
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
