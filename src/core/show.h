#pragma once

#include "core/effect-runner.h"
#include "core/patch.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace obsdmx {

/// Ce qu'un programme demande a un projecteur.
struct FixtureLook {
	std::string fixtureId;
	LightState state;
};

/// Une ambiance lumineuse, associable a une scene OBS.
struct Program {
	std::string id;
	std::string name;
	/// La base : les projecteurs non cites restent eteints.
	std::vector<FixtureLook> looks;
	/// Les effets s'empilent par-dessus la base, dans l'ordre.
	std::vector<Effect> effects;

	const LightState *lookFor(const std::string &fixtureId) const;
};

/// Association entre une scene OBS et un programme.
struct SceneBinding {
	/// Identifiant unique de la scene OBS. On n'utilise pas son nom, qui
	/// change des que l'utilisateur la renomme.
	std::string sceneUuid;
	/// Dernier nom connu, uniquement pour l'affichage.
	std::string sceneName;
	std::string programId;
	/// Duree du fondu a l'entree, en millisecondes.
	int fadeMs = 500;
};

/// Le spectacle : les projecteurs, les programmes, et ce qui les declenche.
///
/// Cet objet est lu par le thread du moteur et modifie par l'interface : toutes
/// ses methodes publiques prennent son verrou. Elles ne doivent jamais appeler
/// le moteur en retour, sous peine d'interblocage.
class Show {
public:
	using Clock = std::chrono::steady_clock;

	explicit Show(const FixtureLibrary &library) : patch_(library) {}

	/// Acces au patch. L'appelant doit tenir le verrou via withPatch().
	template <typename Fn> auto withPatch(Fn &&fn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return fn(patch_);
	}
	template <typename Fn> auto withPatch(Fn &&fn) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return fn(patch_);
	}

	std::vector<Program> programs() const;
	std::optional<Program> program(const std::string &id) const;
	void setPrograms(std::vector<Program> programs);
	std::string addProgram(Program program);
	bool removeProgram(const std::string &id);
	void updateProgram(const Program &program);

	std::vector<SceneBinding> bindings() const;
	void setBindings(std::vector<SceneBinding> bindings);
	/// Associe une scene a un programme. Un programme vide dissocie la scene.
	void bindScene(const std::string &sceneUuid, const std::string &sceneName, const std::string &programId,
		       int fadeMs);
	std::optional<SceneBinding> bindingFor(const std::string &sceneUuid) const;

	/// Declenche le programme associe a cette scene, avec son fondu.
	void activateScene(const std::string &sceneUuid, Clock::time_point now);
	/// Declenche un programme directement, par exemple depuis un raccourci.
	void activateProgram(const std::string &programId, int fadeMs, Clock::time_point now);

	std::string activeProgramId() const;

	/// Programme en cours d'edition : il prend la main sur la sortie tant que
	/// l'editeur est ouvert, pour que l'utilisateur voie ce qu'il fait.
	void setPreview(std::optional<Program> program);
	bool hasPreview() const;

	/// Rendu d'une trame. Appele depuis le thread du moteur.
	void render(std::vector<Universe> &universes, Clock::time_point now, const AudioSnapshot &audio);

	void clear();

private:
	/// Etat courant de chaque projecteur, fige au debut d'une transition.
	std::unordered_map<std::string, LightState> currentStates(Clock::time_point now) const;

	void beginTransition(const std::string &programId, int fadeMs, Clock::time_point now);
	void applyBuiltinEffects(const Program &program, std::vector<Universe> &universes) const;

	mutable std::mutex mutex_;
	Patch patch_;
	std::vector<Program> programs_;
	std::vector<SceneBinding> bindings_;

	std::string activeProgramId_;
	std::optional<Program> preview_;

	/// Etats de depart du fondu en cours.
	std::unordered_map<std::string, LightState> fadeFrom_;
	EffectRunner effects_;
	Clock::time_point fadeStart_{};
	int fadeMs_ = 0;
	int nextProgramId_ = 1;
};

} // namespace obsdmx
