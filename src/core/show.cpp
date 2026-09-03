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

	// Un identifiant genere ne doit jamais rejouer un identifiant charge.
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

	// Les scenes qui pointaient dessus se retrouvent sans programme plutot
	// que de pointer dans le vide.
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

	// Une scene sans association eteint la lumiere, avec un fondu par defaut :
	// c'est plus previsible que de laisser la scene precedente allumee.
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
	// Fige l'image courante, fondu en cours compris : enchainer deux
	// transitions ne doit pas provoquer de saut.
	fadeFrom_ = currentStates(now);
	fadeStart_ = now;
	fadeMs_ = std::max(fadeMs, 0);
	activeProgramId_ = programId;
}

std::unordered_map<std::string, LightState> Show::currentStates(Clock::time_point now) const
{
	// Appele avec le verrou tenu.
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
	// L'apercu prend la main immediatement : l'utilisateur veut voir l'effet
	// de son reglage, pas attendre un fondu.
	preview_ = std::move(program);
}

bool Show::hasPreview() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return preview_.has_value();
}

void Show::render(std::vector<Universe> &universes, Clock::time_point now)
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

	// L'apercu court-circuite le fondu.
	float t = 1.0f;
	if (!preview_ && fadeMs_ > 0) {
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - fadeStart_);
		t = std::min(1.0f, static_cast<float>(elapsed.count()) / static_cast<float>(fadeMs_));
	}

	for (const auto &fixture : patch_.fixtures()) {
		const LightState *to = target ? target->lookFor(fixture.id) : nullptr;
		const LightState toState = to ? *to : LightState::black();

		LightState state = toState;
		if (t < 1.0f) {
			const auto from = fadeFrom_.find(fixture.id);
			const LightState fromState = from != fadeFrom_.end() ? from->second : LightState::black();
			state = lerp(fromState, toState, t);
		}

		patch_.renderFixture(fixture, state, universes);
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
