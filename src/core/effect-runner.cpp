#include "core/effect-runner.h"

#include "core/patch.h"

#include <algorithm>
#include <cmath>

namespace obsdmx {

namespace {

float clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

/// A simple, reproducible generator: we do not want a random chase whose
/// pattern depends on global program state.
uint32_t nextRandom(uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

double stepDurationMs(const ChaserSettings &chaser)
{
	if (chaser.timing == ChaserTiming::Bpm)
		// One step per beat: 60000 ms divided by the tempo.
		return 60000.0 / std::max(1.0f, chaser.bpm);
	return std::max(1, chaser.stepMs);
}

} // namespace

LightState blend(const LightState &base, const LightState &overlay, BlendMode mode)
{
	if (mode == BlendMode::Replace)
		return overlay;

	// Brightest wins. We take the winner's whole state rather than mixing
	// channel by channel: blending two hues would give a third colour nobody
	// asked for.
	return overlay.intensity >= base.intensity ? overlay : base;
}

void EffectRunner::reset()
{
	runtimes_.clear();
}

EffectRunner::Runtime &EffectRunner::runtimeFor(const Effect &effect, Clock::time_point now)
{
	auto it = runtimes_.find(effect.id);
	if (it == runtimes_.end()) {
		Runtime runtime;
		runtime.started = now;
		it = runtimes_.emplace(effect.id, runtime).first;
	}
	return it->second;
}

void EffectRunner::apply(const std::vector<Effect> &effects, const Patch &patch, const AudioSnapshot &audio,
			 Clock::time_point now, std::unordered_map<std::string, LightState> &states)
{
	for (const auto &effect : effects) {
		if (!effect.enabled || effect.fixtureIds.empty())
			continue;

		Runtime &runtime = runtimeFor(effect, now);

		switch (effect.type) {
		case EffectType::Chaser:
			applyChaser(effect, runtime, audio, now, states);
			break;
		case EffectType::Strobe:
			applyStrobe(effect, patch, now, states);
			break;
		case EffectType::Sound:
			applySound(effect, runtime, audio, states);
			break;
		case EffectType::BuiltinFx:
			// Built-in effects do not go through a lighting state: they
			// force channels directly, later in the render.
			break;
		}
	}
}

void EffectRunner::applyChaser(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio,
			       Clock::time_point now, std::unordered_map<std::string, LightState> &states)
{
	const auto &chaser = effect.chaser;
	if (chaser.steps.empty())
		return;

	const int stepCount = static_cast<int>(chaser.steps.size());

	int elapsedSteps = 0;
	double phase = 0.0;

	if (chaser.timing == ChaserTiming::Beat) {
		// Driven by the beats heard in the OBS mix. The counter is read rather
		// than a flag, so a beat falling between two frames is never missed.
		if (audio.beatCount != runtime.lastBeat) {
			// The interval between the last two beats becomes the step
			// duration, which is what the fade needs to have any length.
			if (runtime.lastBeatAt.time_since_epoch().count() != 0)
				runtime.beatIntervalMs =
					std::chrono::duration<double, std::milli>(now - runtime.lastBeatAt).count();
			runtime.lastBeatAt = now;
			runtime.lastBeat = audio.beatCount;
			++runtime.beatStep;
		}

		elapsedSteps = runtime.beatStep;

		// Without a beat yet, or without an interval to measure against, the
		// step is held rather than faded from nowhere.
		if (runtime.beatIntervalMs > 0.0 && runtime.lastBeatAt.time_since_epoch().count() != 0) {
			const double sinceBeat =
				std::chrono::duration<double, std::milli>(now - runtime.lastBeatAt).count();
			phase = std::min(1.0, sinceBeat / runtime.beatIntervalMs);
		} else {
			phase = 1.0;
		}
	} else {
		const double durationMs = stepDurationMs(chaser);
		const double elapsedMs =
			std::chrono::duration<double, std::milli>(now - runtime.started).count();
		elapsedSteps = static_cast<int>(elapsedMs / durationMs);
		phase = std::fmod(elapsedMs, durationMs) / durationMs;
	}

	// Position of the pattern. For the random direction we only recompute when
	// the step changes, otherwise the pattern would jump every frame.
	int offset = 0;
	switch (chaser.direction) {
	case ChaserDirection::Forward:
		offset = elapsedSteps;
		break;
	case ChaserDirection::Backward:
		offset = -elapsedSteps;
		break;
	case ChaserDirection::PingPong: {
		// Back and forth without repeating the end points.
		const int span = std::max(1, stepCount - 1);
		const int position = elapsedSteps % (2 * span);
		offset = position <= span ? position : 2 * span - position;
		break;
	}
	case ChaserDirection::Random:
		if (elapsedSteps != runtime.lastStepIndex) {
			runtime.lastStepIndex = elapsedSteps;
			runtime.step = static_cast<int>(nextRandom(runtime.randomState) % stepCount);
		}
		offset = runtime.step;
		break;
	}

	// The fade covers only part of the step: beyond it the colour is steady.
	const float fade = clamp01(chaser.fadeRatio);
	const float mix = fade <= 0.0f ? 1.0f : clamp01(static_cast<float>(phase) / fade);

	for (size_t i = 0; i < effect.fixtureIds.size(); ++i) {
		const std::string &fixtureId = effect.fixtureIds[i];

		// Each fixture is offset by one: that offset is what makes the light
		// run along the row.
		const int index = static_cast<int>(i) + offset;
		const int current = ((index % stepCount) + stepCount) % stepCount;
		const int previous = ((current - 1) % stepCount + stepCount) % stepCount;

		const LightState value =
			fade > 0.0f ? lerp(chaser.steps[previous], chaser.steps[current], mix) : chaser.steps[current];

		auto it = states.find(fixtureId);
		states[fixtureId] = it != states.end() ? blend(it->second, value, effect.blend) : value;
	}
}

void EffectRunner::applyStrobe(const Effect &effect, const Patch &patch, Clock::time_point now,
			       std::unordered_map<std::string, LightState> &states)
{
	const auto &strobe = effect.strobe;
	const float hz = std::clamp(strobe.hz, 0.1f, 25.0f);

	const double elapsedMs = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
	const double periodMs = 1000.0 / hz;
	const bool lit = std::fmod(elapsedMs, periodMs) < periodMs * clamp01(strobe.dutyCycle);

	for (const std::string &fixtureId : effect.fixtureIds) {
		const Fixture *fixture = patch.find(fixtureId);
		if (!fixture)
			continue;

		auto it = states.find(fixtureId);
		const LightState base = it != states.end() ? it->second : LightState::black();

		LightState value = strobe.useBaseColor ? base : strobe.color;

		const FixtureMode *mode = patch.modeOf(*fixture);
		const bool hardware = strobe.preferHardware && mode && mode->hasRole(ChannelRole::Strobe);

		if (hardware) {
			// The fixture generates the flashing itself: far crisper than
			// modulating at 40 Hz, and free of aliasing.
			value.strobeHz = hz;
			if (strobe.useBaseColor)
				value.intensity = base.intensity;
		} else {
			value.strobeHz = 0.0f;
			value.intensity = lit ? (strobe.useBaseColor ? base.intensity : strobe.color.intensity) : 0.0f;
		}

		states[fixtureId] = it != states.end() ? blend(base, value, effect.blend) : value;
	}
}

void EffectRunner::applySound(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio,
			      std::unordered_map<std::string, LightState> &states)
{
	const auto &sound = effect.sound;
	const int band = std::clamp(sound.band, 0, 2);

	const float level = clamp01((audio.bands[band] - sound.threshold) * sound.sensitivity);
	const bool beat = audio.beatCount != runtime.lastBeat;
	if (beat)
		runtime.lastBeat = audio.beatCount;

	for (const std::string &fixtureId : effect.fixtureIds) {
		auto it = states.find(fixtureId);
		const LightState base = it != states.end() ? it->second : LightState::black();

		// The colour comes from the programme or from the effect, as the user
		// chooses. Without that choice, a fixture missing from the base
		// inherited an arbitrary hue that nothing could change.
		LightState value = sound.useBaseColor ? base : sound.color;

		switch (sound.target) {
		case SoundTarget::Intensity:
			value.intensity = level;
			break;

		case SoundTarget::Hue:
			// The three bands become a position on the colour circle:
			// bass towards red, treble towards blue.
			value.colorMix = 1.0f;
			value.hue = std::fmod(audio.bands[0] * 60.0f + audio.bands[1] * 180.0f +
						      audio.bands[2] * 300.0f,
					      360.0f);
			value.saturation = 1.0f;
			value.intensity = base.intensity;
			break;

		case SoundTarget::FlashOnBeat:
			// The flash lasts one frame; that is short, but at 40 Hz the
			// eye catches it, and it saves keeping a timer here.
			value.intensity = beat ? 1.0f : 0.0f;
			break;

		}

		states[fixtureId] = blend(base, value, effect.blend);
	}
}

std::vector<std::pair<int, uint8_t>> builtinFxChannels(const FixtureMode &mode, const BuiltinFxSettings &settings)
{
	std::vector<std::pair<int, uint8_t>> values;

	// Direct entry: the profile does not describe this fixture's effects, so
	// the user copies its channel chart.
	if (settings.useManual) {
		for (const auto &entry : settings.manual) {
			// Manufacturer numbering starts at 1; internally channels are
			// indexed from 0.
			const int index = entry.channel - 1;

			// Writing past the fixture's footprint would drive its
			// neighbour: refuse rather than cause damage at a
			// distance.
			if (index >= 0 && index < static_cast<int>(mode.channelCount()))
				values.emplace_back(index, entry.value);
		}
		return values;
	}

	const auto effect = std::find_if(mode.effects.begin(), mode.effects.end(),
					 [&settings](const BuiltinEffect &e) { return e.id == settings.effectId; });
	if (effect == mode.effects.end())
		return values;

	const int selectChannel = mode.findRole(ChannelRole::FxSelect);
	if (selectChannel < 0)
		return values;
	values.emplace_back(selectChannel, effect->selectValue);

	// The control channel starts the loop. High values mean "stop", so we stay
	// in the low range.
	if (const int control = mode.findRole(ChannelRole::FxControl); control >= 0)
		values.emplace_back(control, 0);

	if (effect->hasFrequency && effect->frequencyChannel >= 0 &&
	    effect->frequencyChannel < static_cast<int>(mode.channelCount())) {
		// Rates 1 to 10 occupy ten-value slices; the random setting, where it
		// exists, occupies the next slice.
		const int slot = settings.frequency == 0 && effect->hasRandomFrequency
					 ? 10
					 : std::clamp(settings.frequency, 1, 10) - 1;
		values.emplace_back(effect->frequencyChannel, static_cast<uint8_t>(slot * 10 + 5));
	}

	return values;
}

} // namespace obsdmx
