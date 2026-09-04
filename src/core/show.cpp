#include "core/show.h"

#include <algorithm>
#include <cstdlib>

namespace obsdmx {

const LightState *Program::lookFor(const std::string &fixtureId) const
{
	const auto it = std::find_if(looks.begin(), looks.end(),
				     [&fixtureId](const FixtureLook &look) { return look.fixtureId == fixtureId; });
	return it != looks.end() ? &it->state : nullptr;
}

std::vector<Program> Show::programs() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return programs_;
}

std::optional<Program> Show::program(const std::string &id) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = std::find_if(programs_.begin(), programs_.end(),
				     [&id](const Program &p) { return p.id == id; });
	if (it == programs_.end())
		return std::nullopt;
	return *it;
}

void Show::setPrograms(std::vector<Program> programs)
{
	std::lock_guard<std::mutex> lock(mutex_);
	programs_ = std::move(programs);

	// A generated identifier must never replay one that was loaded.
	for (const auto &program : programs_)
		if (program.id.rfind("program-", 0) == 0)
			nextProgramId_ = std::max(nextProgramId_, std::atoi(program.id.c_str() + 8) + 1);
}

std::string Show::addProgram(Program program)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (program.id.empty())
		program.id = "program-" + std::to_string(nextProgramId_++);
	const std::string id = program.id;
	programs_.push_back(std::move(program));
	return id;
}

bool Show::removeProgram(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex_);

	const auto it = std::find_if(programs_.begin(), programs_.end(),
				     [&id](const Program &p) { return p.id == id; });
	if (it == programs_.end())
		return false;
	programs_.erase(it);

	// Scenes that pointed at it are left with no programme rather than
	// pointing at nothing.
	for (auto &binding : bindings_)
		if (binding.programId == id)
			binding.programId.clear();

	if (activeProgramId_ == id)
		activeProgramId_.clear();

	return true;
}

void Show::updateProgram(const Program &program)
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = std::find_if(programs_.begin(), programs_.end(),
				     [&program](const Program &p) { return p.id == program.id; });
	if (it != programs_.end())
		*it = program;
}

std::vector<SceneBinding> Show::bindings() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return bindings_;
}

void Show::setBindings(std::vector<SceneBinding> bindings)
{
	std::lock_guard<std::mutex> lock(mutex_);
	bindings_ = std::move(bindings);
}

void Show::bindScene(const std::string &sceneUuid, const std::string &sceneName, const std::string &programId,
		     int fadeMs)
{
	std::lock_guard<std::mutex> lock(mutex_);

	const auto it = std::find_if(bindings_.begin(), bindings_.end(),
				     [&sceneUuid](const SceneBinding &b) { return b.sceneUuid == sceneUuid; });
	if (it != bindings_.end()) {
		it->sceneName = sceneName;
		it->programId = programId;
		it->fadeMs = fadeMs;
		return;
	}

	bindings_.push_back({sceneUuid, sceneName, programId, fadeMs});
}

std::optional<SceneBinding> Show::bindingFor(const std::string &sceneUuid) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = std::find_if(bindings_.begin(), bindings_.end(),
				     [&sceneUuid](const SceneBinding &b) { return b.sceneUuid == sceneUuid; });
	if (it == bindings_.end())
		return std::nullopt;
	return *it;
}

std::string Show::activeProgramId() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return activeProgramId_;
}

void Show::activateScene(const std::string &sceneUuid, Clock::time_point now)
{
	std::lock_guard<std::mutex> lock(mutex_);

	const auto it = std::find_if(bindings_.begin(), bindings_.end(),
				     [&sceneUuid](const SceneBinding &b) { return b.sceneUuid == sceneUuid; });

	// An unattached scene puts the lights out, with a default fade: that is
	// more predictable than leaving the previous scene lit.
	if (it == bindings_.end()) {
		beginTransition({}, 500, now);
		return;
	}

	beginTransition(it->programId, it->fadeMs, now);
}

void Show::activateProgram(const std::string &programId, int fadeMs, Clock::time_point now)
{
	std::lock_guard<std::mutex> lock(mutex_);
	beginTransition(programId, fadeMs, now);
}

void Show::beginTransition(const std::string &programId, int fadeMs, Clock::time_point now)
{
	// OBS emits several events for a single scene change. Replaying the
	// transition each time would restart the fade and make chases stutter.
	if (programId == activeProgramId_ && !fadeFrom_.empty())
		return;

	// A chase must restart from its first step when the programme changes, not
	// resume where the previous programme left it.
	if (programId != activeProgramId_)
		effects_.reset();

	// Freeze the current picture, running fade included: chaining two
	// transitions must not cause a jump.
	fadeFrom_ = currentStates(now);
	fadeStart_ = now;
	fadeMs_ = std::max(fadeMs, 0);
	activeProgramId_ = programId;
}

std::unordered_map<std::string, LightState> Show::currentStates(Clock::time_point now) const
{
	// Called with the lock held.
	const Program *target = nullptr;
	if (preview_) {
		target = &*preview_;
	} else {
		const auto it = std::find_if(programs_.begin(), programs_.end(),
					     [this](const Program &p) { return p.id == activeProgramId_; });
		if (it != programs_.end())
			target = &*it;
	}

	float t = 1.0f;
	if (fadeMs_ > 0) {
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - fadeStart_);
		t = std::min(1.0f, static_cast<float>(elapsed.count()) / static_cast<float>(fadeMs_));
	}

	std::unordered_map<std::string, LightState> states;
	for (const auto &fixture : patch_.fixtures()) {
		const LightState *to = target ? target->lookFor(fixture.id) : nullptr;
		const LightState toState = to ? *to : LightState::black();

		const auto from = fadeFrom_.find(fixture.id);
		const LightState fromState = from != fadeFrom_.end() ? from->second : LightState::black();

		states[fixture.id] = lerp(fromState, toState, t);
	}
	return states;
}

void Show::setPreview(std::optional<Program> program)
{
	std::lock_guard<std::mutex> lock(mutex_);
	// The preview takes over at once: the user wants to see what their setting
	// does, not wait out a fade.
	preview_ = std::move(program);
}

bool Show::hasPreview() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return preview_.has_value();
}

void Show::render(std::vector<Universe> &universes, Clock::time_point now, const AudioSnapshot &audio)
{
	std::lock_guard<std::mutex> lock(mutex_);

	const Program *target = nullptr;
	if (preview_) {
		target = &*preview_;
	} else {
		const auto it = std::find_if(programs_.begin(), programs_.end(),
					     [this](const Program &p) { return p.id == activeProgramId_; });
		if (it != programs_.end())
			target = &*it;
	}

	// The preview short-circuits the fade.
	float t = 1.0f;
	if (!preview_ && fadeMs_ > 0) {
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - fadeStart_);
		t = std::min(1.0f, static_cast<float>(elapsed.count()) / static_cast<float>(fadeMs_));
	}

	// The base, fade included.
	std::unordered_map<std::string, LightState> states;
	states.reserve(patch_.fixtures().size());
	for (const auto &fixture : patch_.fixtures()) {
		const LightState *to = target ? target->lookFor(fixture.id) : nullptr;
		const LightState toState = to ? *to : LightState::black();

		if (t < 1.0f) {
			const auto from = fadeFrom_.find(fixture.id);
			const LightState fromState = from != fadeFrom_.end() ? from->second : LightState::black();
			states[fixture.id] = lerp(fromState, toState, t);
		} else {
			states[fixture.id] = toState;
		}
	}

	// Then the effects, stacked on top.
	if (target)
		effects_.apply(target->effects, patch_, audio, now, states);

	for (const auto &fixture : patch_.fixtures()) {
		const auto it = states.find(fixture.id);
		patch_.renderFixture(fixture, it != states.end() ? it->second : LightState::black(), universes);
	}

	// Built-in effects force raw channels, so they come after the normal
	// render: they replace whatever the colour put there.
	if (target)
		applyBuiltinEffects(*target, universes);
}

void Show::applyBuiltinEffects(const Program &program, std::vector<Universe> &universes) const
{
	for (const auto &effect : program.effects) {
		if (!effect.enabled || effect.type != EffectType::BuiltinFx)
			continue;

		for (const std::string &fixtureId : effect.fixtureIds) {
			const Fixture *fixture = patch_.find(fixtureId);
			if (!fixture)
				continue;

			const FixtureMode *mode = patch_.modeOf(*fixture);
			if (!mode)
				continue;

			for (const auto &[channel, value] : builtinFxChannels(*mode, effect.builtin))
				patch_.writeChannel(*fixture, channel, value, universes);
		}
	}
}

void Show::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	patch_.clear();
	programs_.clear();
	bindings_.clear();
	activeProgramId_.clear();
	preview_.reset();
	fadeFrom_.clear();
}

} // namespace obsdmx
