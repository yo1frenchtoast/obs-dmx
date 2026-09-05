#pragma once

#include "core/effect.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace obsdmx {

class Patch;

/// Applies a programme's effects on top of its base.
///
/// State that must persist between frames (a chase's position, the last beat
/// seen) lives here rather than in the effect, so that a programme's description
/// stays inert data: serialisable and comparable.
class EffectRunner {
public:
	using Clock = std::chrono::steady_clock;

	/// Modifies states in place. The patch tells whether a fixture has a
	/// hardware strobe channel.
	void apply(const std::vector<Effect> &effects, const Patch &patch, const AudioSnapshot &audio,
		   Clock::time_point now, std::unordered_map<std::string, LightState> &states);

	/// Forgets the running positions. Call it when the programme changes, so a
	/// chase restarts from its first step.
	void reset();

private:
	struct Runtime {
		Clock::time_point started{};
		/// Current step, advanced by time or by musical beats.
		int step = 0;
		int lastStepIndex = 0;
		bool ascending = true;
		uint64_t lastBeat = 0;
		/// Steps taken on beats, when the chase is driven by the audio.
		int beatStep = 0;
		Clock::time_point lastBeatAt{};
		/// Interval between the last two beats, which becomes the step
		/// duration a fade can be measured against.
		double beatIntervalMs = 0.0;
		uint32_t randomState = 0x9e3779b9u;
	};

	Runtime &runtimeFor(const Effect &effect, Clock::time_point now);

	void applyChaser(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio, Clock::time_point now,
			 std::unordered_map<std::string, LightState> &states);
	void applyStrobe(const Effect &effect, const Patch &patch, Clock::time_point now,
			 std::unordered_map<std::string, LightState> &states);
	void applySound(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio,
			std::unordered_map<std::string, LightState> &states);

	std::unordered_map<std::string, Runtime> runtimes_;
};

/// Raw DMX values to force for an effect built into the fixture.
/// Returns an empty vector if the effect does not exist in this mode.
std::vector<std::pair<int, uint8_t>> builtinFxChannels(const FixtureMode &mode, const BuiltinFxSettings &settings);

} // namespace obsdmx
