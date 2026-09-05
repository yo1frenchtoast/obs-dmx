#include "test-harness.h"

#include "core/effect-runner.h"
#include "core/patch.h"

#include <cmath>

using namespace obsdmx;
using Clock = EffectRunner::Clock;

namespace {

FixtureLibrary buildLibrary()
{
	FixtureLibrary library;
	std::string error;
	// One fixture with a hardware strobe channel, one without.
	library.loadJson(R"({
		"id": "avec-strobe", "model": "Avec strobe", "default_mode": "m",
		"modes": [{"id": "m", "channels": [
			{"role":"dimmer"},{"role":"red"},{"role":"green"},{"role":"blue"},
			{"role":"strobe","range_min":20,"range_max":255,"off":0,"physical_min":1,"physical_max":25}]}]
	})", error);
	library.loadJson(R"({
		"id": "sans-strobe", "model": "Sans strobe", "default_mode": "m",
		"modes": [{"id": "m", "channels": [
			{"role":"dimmer"},{"role":"red"},{"role":"green"},{"role":"blue"}]}]
	})", error);
	return library;
}

Patch buildPatch(const FixtureLibrary &library, const std::string &profile = "avec-strobe")
{
	Patch patch(library);
	for (int i = 0; i < 4; ++i) {
		Fixture fixture;
		fixture.id = "f" + std::to_string(i);
		fixture.profileId = profile;
		fixture.modeId = "m";
		fixture.address = 1 + i * 10;
		patch.add(fixture);
	}
	return patch;
}

LightState colored(float hue, float intensity = 1.0f)
{
	LightState state;
	state.intensity = intensity;
	state.colorMix = 1.0f;
	state.hue = hue;
	state.saturation = 1.0f;
	return state;
}

Effect chaserOf(std::vector<LightState> steps, int stepMs, ChaserDirection direction = ChaserDirection::Forward)
{
	Effect effect;
	effect.id = "chaser";
	effect.type = EffectType::Chaser;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0", "f1", "f2", "f3"};
	effect.chaser.steps = std::move(steps);
	effect.chaser.stepMs = stepMs;
	effect.chaser.direction = direction;
	return effect;
}

std::unordered_map<std::string, LightState> emptyStates()
{
	return {};
}

} // namespace

TEST(htp_blending_keeps_the_brightest_state)
{
	const LightState faible = colored(0.0f, 0.2f);
	const LightState fort = colored(240.0f, 0.9f);

	// We take the winner's whole state: blending two hues would give a third
	// colour nobody asked for.
	CHECK_EQ(blend(faible, fort, BlendMode::Htp).hue, 240.0f);
	CHECK_EQ(blend(fort, faible, BlendMode::Htp).hue, 240.0f);

	// Replace mode ignores intensities.
	CHECK_EQ(blend(fort, faible, BlendMode::Replace).hue, 0.0f);
}

TEST(a_chase_offsets_the_pattern_along_the_fixtures)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	// Two steps: lit then dark. Every other fixture should be lit.
	const auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f)}, 1000);

	const auto start = Clock::now();
	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);

	CHECK_EQ(states["f0"].intensity, 1.0f);
	CHECK_EQ(states["f1"].intensity, 0.0f);
	CHECK_EQ(states["f2"].intensity, 1.0f);
	CHECK_EQ(states["f3"].intensity, 0.0f);
}

TEST(a_chase_advances_over_time)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	const auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f)}, 1000);
	const auto start = Clock::now();

	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);
	CHECK_EQ(states["f0"].intensity, 1.0f);

	// One step later, the pattern has moved along by one.
	states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(1000), states);
	CHECK_EQ(states["f0"].intensity, 0.0f);
	CHECK_EQ(states["f1"].intensity, 1.0f);
}

TEST(a_chase_on_tempo_derives_its_step_duration)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f)}, 1000);
	effect.chaser.timing = ChaserTiming::Bpm;
	effect.chaser.bpm = 120.0f; // 120 temps par minute, soit 500 ms par pas

	const auto start = Clock::now();
	// The effect starts its clock on its first pass, so it must be primed at
	// instant zero before jumping forward in time.
	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);

	states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(500), states);

	// At 500 ms the pattern has advanced by exactly one step.
	CHECK_EQ(states["f0"].intensity, 0.0f);
	CHECK_EQ(states["f1"].intensity, 1.0f);
}

TEST(a_ping_pong_chase_does_not_repeat_its_end_points)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	const auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.5f), colored(0.0f, 0.0f)}, 100,
				     ChaserDirection::PingPong);
	const auto start = Clock::now();

	// Over three steps, the back-and-forth has a period of four: 0,1,2,1.
	std::vector<int> offsets;
	for (int i = 0; i < 5; ++i) {
		auto states = emptyStates();
		runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(100 * i), states);
		offsets.push_back(static_cast<int>(std::lround(states["f0"].intensity * 2.0f)));
	}
	// Intensities 1.0, 0.5, 0.0 -> 2, 1, 0 once scaled.
	CHECK_EQ(offsets[0], 2);
	CHECK_EQ(offsets[1], 1);
	CHECK_EQ(offsets[2], 0);
	CHECK_EQ(offsets[3], 1);
	CHECK_EQ(offsets[4], 2); // revenu au depart, sans repeter le 0
}

TEST(a_chase_fades_between_steps_over_the_share_asked_for)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto effect = chaserOf({colored(0.0f, 0.0f), colored(0.0f, 1.0f)}, 1000);
	effect.chaser.fadeRatio = 1.0f; // fondu permanent

	const auto start = Clock::now();
	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);

	// Halfway through the step, f1 sits halfway between step 0 and step 1.
	states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(500), states);
	CHECK(std::abs(states["f1"].intensity - 0.5f) < 0.05f);
}

TEST(strobe_uses_the_hardware_channel_where_there_is_one)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "strobe";
	effect.type = EffectType::Strobe;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.strobe.hz = 15.0f;
	effect.strobe.useBaseColor = true;

	auto states = emptyStates();
	states["f0"] = colored(120.0f, 1.0f);
	runner.apply({effect}, patch, AudioSnapshot{}, Clock::now(), states);

	// At a 40 Hz refresh rate, modulating 15 Hz in software would alias: the
	// fixture has to handle it itself.
	CHECK_EQ(states["f0"].strobeHz, 15.0f);
	CHECK_EQ(states["f0"].intensity, 1.0f);
	// And it keeps the programme's colour.
	CHECK_EQ(states["f0"].hue, 120.0f);
}

TEST(strobe_modulates_intensity_when_there_is_no_hardware_channel)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library, "sans-strobe");
	EffectRunner runner;

	Effect effect;
	effect.id = "strobe";
	effect.type = EffectType::Strobe;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.strobe.hz = 4.0f;
	effect.strobe.dutyCycle = 0.5f;
	effect.strobe.useBaseColor = true;

	// Over a full period, the fixture must both light and go dark.
	bool seenLit = false, seenDark = false;
	for (int i = 0; i < 40; ++i) {
		auto states = emptyStates();
		states["f0"] = colored(0.0f, 1.0f);
		runner.apply({effect}, patch, AudioSnapshot{},
			     Clock::now() + std::chrono::milliseconds(i * 10), states);
		CHECK_EQ(states["f0"].strobeHz, 0.0f); // pas de canal materiel a piloter
		if (states["f0"].intensity > 0.5f) seenLit = true;
		else seenDark = true;
	}
	CHECK(seenLit);
	CHECK(seenDark);
}

TEST(an_htp_strobe_does_not_erase_the_background)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library, "sans-strobe");
	EffectRunner runner;

	Effect effect;
	effect.id = "strobe";
	effect.type = EffectType::Strobe;
	effect.blend = BlendMode::Htp;
	effect.fixtureIds = {"f0"};
	effect.strobe.hz = 4.0f;
	effect.strobe.useBaseColor = false;
	effect.strobe.color = colored(0.0f, 1.0f);

	// The background sits at half intensity: between flashes it must stay visible.
	for (int i = 0; i < 40; ++i) {
		auto states = emptyStates();
		states["f0"] = colored(240.0f, 0.5f);
		runner.apply({effect}, patch, AudioSnapshot{},
			     Clock::now() + std::chrono::milliseconds(i * 10), states);
		CHECK(states["f0"].intensity >= 0.5f);
	}
}

TEST(sound_reactive_follows_the_level)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "sound";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.band = 0;
	effect.sound.sensitivity = 1.0f;
	effect.sound.threshold = 0.1f;

	AudioSnapshot quiet;
	quiet.bands[0] = 0.05f; // sous le seuil
	auto states = emptyStates();
	states["f0"] = colored(0.0f, 1.0f);
	runner.apply({effect}, patch, quiet, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 0.0f);

	AudioSnapshot loud;
	loud.bands[0] = 0.9f;
	states = emptyStates();
	states["f0"] = colored(0.0f, 1.0f);
	runner.apply({effect}, patch, loud, Clock::now(), states);
	CHECK(std::abs(states["f0"].intensity - 0.8f) < 0.01f);
}

TEST(sound_reactive_never_misses_a_beat_between_frames)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "sound";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::FlashOnBeat;
	effect.sound.color = colored(0.0f, 1.0f);

	AudioSnapshot audio;
	audio.beatCount = 1;

	auto states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 1.0f); // premier temps vu

	// The same counter does not fire again.
	states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 0.0f);

	// A counter rather than a flag: even if several beats fell between two
	// frames, the change is still seen.
	audio.beatCount = 5;
	states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 1.0f);
}

TEST(a_disabled_or_targetless_effect_does_nothing)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto disabled = chaserOf({colored(0.0f, 1.0f)}, 100);
	disabled.enabled = false;

	auto targetless = chaserOf({colored(0.0f, 1.0f)}, 100);
	targetless.id = "vide";
	targetless.fixtureIds.clear();

	auto states = emptyStates();
	runner.apply({disabled, targetless}, patch, AudioSnapshot{}, Clock::now(), states);
	CHECK(states.empty());
}

TEST(under_htp_an_effect_can_only_brighten_never_dim)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Htp;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;

	// This is the fundamental limit of "brightest wins": on a programme that
	// already lights at full, the effect can take nothing away and so looks
	// inert. The interface must say so; the engine cannot invent it.
	AudioSnapshot faible;
	faible.bands[0] = 0.1f;
	auto states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f);
	runner.apply({effect}, patch, faible, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 1.0f);

	// Under Replace, the level gets through.
	effect.blend = BlendMode::Replace;
	states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f);
	runner.apply({effect}, patch, faible, Clock::now(), states);
	CHECK(std::abs(states["f0"].intensity - 0.1f) < 0.01f);
}

TEST(sound_keeps_the_programmes_colour_by_default)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;
	effect.sound.useBaseColor = true;

	AudioSnapshot audio;
	audio.bands[0] = 0.5f;

	auto states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f); // bleu
	runner.apply({effect}, patch, audio, Clock::now(), states);

	CHECK_EQ(states["f0"].hue, 240.0f);
	CHECK(std::abs(states["f0"].intensity - 0.5f) < 0.01f);
}

TEST(sound_can_impose_its_own_colour)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;
	effect.sound.useBaseColor = false;
	effect.sound.color = colored(120.0f, 1.0f); // vert

	AudioSnapshot audio;
	audio.bands[0] = 0.5f;

	auto states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f); // base bleue, ignoree
	runner.apply({effect}, patch, audio, Clock::now(), states);

	CHECK_EQ(states["f0"].hue, 120.0f);
	CHECK(std::abs(states["f0"].intensity - 0.5f) < 0.01f);
}

TEST(the_flash_on_beat_has_an_adjustable_colour)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::FlashOnBeat;
	effect.sound.useBaseColor = false;
	effect.sound.color = colored(60.0f, 1.0f); // jaune

	AudioSnapshot audio;
	audio.beatCount = 1;

	auto states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].hue, 60.0f);
	CHECK_EQ(states["f0"].intensity, 1.0f);
}

TEST(a_fixture_absent_from_the_programme_no_longer_takes_an_arbitrary_hue)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;
	effect.sound.useBaseColor = false;
	effect.sound.color = colored(300.0f, 1.0f);

	AudioSnapshot audio;
	audio.bands[0] = 0.8f;

	// No starting state: the fixture is not named by the programme.
	auto states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);

	// It takes the colour chosen for the effect, not whatever was lingering in
	// the "off" state.
	CHECK_EQ(states["f0"].hue, 300.0f);
}

TEST(a_chase_on_beat_waits_for_the_music_instead_of_its_own_clock)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f)}, 100);
	effect.chaser.timing = ChaserTiming::Beat;

	const auto start = Clock::now();
	AudioSnapshot silent;

	auto states = emptyStates();
	runner.apply({effect}, patch, silent, start, states);
	const float atStart = states["f0"].intensity;

	// Ten step durations go by with no beat. On its own clock the pattern would
	// have moved five times over; driven by the music it must not move at all.
	states = emptyStates();
	runner.apply({effect}, patch, silent, start + std::chrono::milliseconds(1000), states);
	CHECK_EQ(states["f0"].intensity, atStart);

	// One beat, one step.
	AudioSnapshot beat;
	beat.beatCount = 1;
	states = emptyStates();
	runner.apply({effect}, patch, beat, start + std::chrono::milliseconds(1100), states);
	CHECK(states["f0"].intensity != atStart);
}

TEST(a_chase_on_beat_advances_once_per_beat)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	// Four steps over four fixtures: the lit one walks along, one place a beat.
	auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f), colored(0.0f, 0.0f),
				colored(0.0f, 0.0f)},
			       100);
	effect.chaser.timing = ChaserTiming::Beat;

	const auto start = Clock::now();
	std::vector<int> litFixture;

	for (int beat = 0; beat <= 4; ++beat) {
		AudioSnapshot audio;
		audio.beatCount = static_cast<uint64_t>(beat);

		auto states = emptyStates();
		runner.apply({effect}, patch, audio, start + std::chrono::milliseconds(500 * beat), states);

		for (int i = 0; i < 4; ++i)
			if (states["f" + std::to_string(i)].intensity > 0.5f)
				litFixture.push_back(i);
	}

	CHECK_EQ(litFixture.size(), size_t(5));
	// The same fixture must not stay lit, and after four beats the pattern has
	// come full circle.
	CHECK(litFixture[0] != litFixture[1]);
	CHECK_EQ(litFixture[0], litFixture[4]);
}

TEST(a_chase_on_beat_fades_over_the_measured_beat_interval)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto effect = chaserOf({colored(0.0f, 0.0f), colored(0.0f, 1.0f)}, 100);
	effect.chaser.timing = ChaserTiming::Beat;
	effect.chaser.fadeRatio = 1.0f;

	const auto start = Clock::now();

	// Two beats 500 ms apart give the runner an interval to fade against.
	for (int beat = 1; beat <= 2; ++beat) {
		AudioSnapshot audio;
		audio.beatCount = static_cast<uint64_t>(beat);
		auto states = emptyStates();
		runner.apply({effect}, patch, audio, start + std::chrono::milliseconds(500 * beat), states);
	}

	// Halfway to the next beat, the fixture should sit halfway between steps.
	AudioSnapshot audio;
	audio.beatCount = 2;
	auto states = emptyStates();
	runner.apply({effect}, patch, audio, start + std::chrono::milliseconds(1250), states);
	CHECK(std::abs(states["f1"].intensity - 0.5f) < 0.1f);
}
