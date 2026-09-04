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

/// What a programme asks of one fixture.
struct FixtureLook {
	std::string fixtureId;
	LightState state;
};

/// A lighting look, attachable to an OBS scene.
struct Program {
	std::string id;
	std::string name;
	/// The base: fixtures not listed stay dark.
	std::vector<FixtureLook> looks;
	/// Effects stack on top of the base, in order.
	std::vector<Effect> effects;

	const LightState *lookFor(const std::string &fixtureId) const;
};

/// Attachment between an OBS scene and a programme.
struct SceneBinding {
	/// Unique identifier of the OBS scene. Not its name, which changes the
	/// moment the user renames it.
	std::string sceneUuid;
	/// Last known name, for display only.
	std::string sceneName;
	std::string programId;
	/// Fade-in duration, in milliseconds.
	int fadeMs = 500;
};

/// The show: the fixtures, the programmes, and what triggers them.
///
/// This object is read by the engine thread and modified by the interface, so
/// every public method takes its lock. None of them may call back into the
/// engine, on pain of deadlock.
class Show {
public:
	using Clock = std::chrono::steady_clock;

	explicit Show(const FixtureLibrary &library) : patch_(library) {}

	/// Access to the patch. The caller holds the lock through withPatch().
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
	/// Attaches a scene to a programme. An empty programme detaches the scene.
	void bindScene(const std::string &sceneUuid, const std::string &sceneName, const std::string &programId,
		       int fadeMs);
	std::optional<SceneBinding> bindingFor(const std::string &sceneUuid) const;

	/// Triggers the programme attached to this scene, with its fade.
	void activateScene(const std::string &sceneUuid, Clock::time_point now);
	/// Triggers a programme directly, for instance from a hotkey.
	void activateProgram(const std::string &programId, int fadeMs, Clock::time_point now);

	std::string activeProgramId() const;

	/// Programme being edited: it takes over the output while the editor is
	/// open, so the user can see what they are doing.
	void setPreview(std::optional<Program> program);
	bool hasPreview() const;

	/// Renders one frame. Called from the engine thread.
	void render(std::vector<Universe> &universes, Clock::time_point now, const AudioSnapshot &audio);

	void clear();

private:
	/// Current state of every fixture, frozen when a transition starts.
	std::unordered_map<std::string, LightState> currentStates(Clock::time_point now) const;

	void beginTransition(const std::string &programId, int fadeMs, Clock::time_point now);
	void applyBuiltinEffects(const Program &program, std::vector<Universe> &universes) const;

	mutable std::mutex mutex_;
	Patch patch_;
	std::vector<Program> programs_;
	std::vector<SceneBinding> bindings_;

	std::string activeProgramId_;
	std::optional<Program> preview_;

	/// Starting states of the running fade.
	std::unordered_map<std::string, LightState> fadeFrom_;
	EffectRunner effects_;
	Clock::time_point fadeStart_{};
	int fadeMs_ = 0;
	int nextProgramId_ = 1;
};

} // namespace obsdmx
