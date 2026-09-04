#include "test-harness.h"

#include "core/color.h"
#include "core/fixture-library.h"

#include <cmath>

using namespace obsdmx;

namespace {

/// Reproduces the T4c's mode 3, the one chosen by default.
FixtureMode t4cMode3()
{
	FixtureMode mode;
	mode.id = "mode3";
	mode.channels = {
		{ChannelRole::Dimmer, "Intensite", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Cct, "CCT", 0, 0, 255, 0, 128, 2500, 7500},
		{ChannelRole::GreenMagenta, "G/M", 132, 21, 244, 0, 132, 0, 0},
		{ChannelRole::ColorMix, "Fondu", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Hue, "Teinte", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Saturation, "Saturation", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Unused, "Reserve", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Unused, "Reserve", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Strobe, "Strobe", 0, 20, 255, 0, 128, 1, 25},
	};
	return mode;
}

FixtureMode rgbwMode()
{
	FixtureMode mode;
	mode.channels = {
		{ChannelRole::Dimmer, "Intensite", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Red, "R", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Green, "V", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Blue, "B", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::White, "Blanc", 0, 0, 255, 0, 128, 0, 0},
	};
	return mode;
}

bool near(int actual, int expected, int tolerance = 2)
{
	return std::abs(actual - expected) <= tolerance;
}

} // namespace

TEST(t4c_a_saturated_colour_opens_the_crossfade)
{
	LightState state;
	state.intensity = 1.0f;
	state.colorMix = 1.0f;
	state.hue = 240.0f; // bleu
	state.saturation = 1.0f;

	const auto values = renderState(t4cMode3(), state);

	CHECK_EQ(values[0], 255);              // intensite
	CHECK(near(values[3], 255));           // fondu ouvert : sans lui la lampe reste blanche
	CHECK(near(values[4], 170));           // teinte : 240/360 * 255
	CHECK_EQ(values[5], 255);              // saturation
	CHECK_EQ(values[8], 0);                // strobe eteint
}

TEST(t4c_white_mode_closes_the_crossfade)
{
	LightState state;
	state.intensity = 0.5f;
	state.colorMix = 0.0f;
	state.cct = 5000.0f;

	const auto values = renderState(t4cMode3(), state);

	CHECK(near(values[0], 128));  // intensite a 50 %
	CHECK_EQ(values[3], 0);       // fondu referme : moteur blanc
	CHECK_EQ(values[5], 0);       // saturation nulle
	// 5000 K over the 2500-7500 range: halfway.
	CHECK(near(values[1], 128));
}

TEST(t4c_colour_temperature_at_its_limits)
{
	FixtureMode mode = t4cMode3();
	LightState state;
	state.colorMix = 0.0f;

	state.cct = 2500.0f;
	CHECK_EQ(renderState(mode, state)[1], 0);

	state.cct = 7500.0f;
	CHECK_EQ(renderState(mode, state)[1], 255);

	// Out of range: clamp instead of overflowing.
	state.cct = 1000.0f;
	CHECK_EQ(renderState(mode, state)[1], 0);
	state.cct = 20000.0f;
	CHECK_EQ(renderState(mode, state)[1], 255);
}

TEST(t4c_green_magenta_honours_the_neutral_band)
{
	FixtureMode mode = t4cMode3();
	LightState state;

	// The T4c puts its neutral at 132, not at the middle of 0-255.
	state.greenMagenta = 0.0f;
	CHECK_EQ(renderState(mode, state)[2], 132);

	state.greenMagenta = -1.0f;
	CHECK_EQ(renderState(mode, state)[2], 21);

	state.greenMagenta = 1.0f;
	CHECK_EQ(renderState(mode, state)[2], 244);

	// The slope differs either side of the neutral, which is not centred.
	state.greenMagenta = -0.5f;
	CHECK(near(renderState(mode, state)[2], 77));
	state.greenMagenta = 0.5f;
	CHECK(near(renderState(mode, state)[2], 188));
}

TEST(t4c_strobe_skips_the_off_zone)
{
	FixtureMode mode = t4cMode3();
	LightState state;

	// 0 Hz must land in the off zone, not at the lowest useful value.
	state.strobeHz = 0.0f;
	CHECK_EQ(renderState(mode, state)[8], 0);

	// 1 Hz is the first valid rate: it starts at 20.
	state.strobeHz = 1.0f;
	CHECK_EQ(renderState(mode, state)[8], 20);

	state.strobeHz = 25.0f;
	CHECK_EQ(renderState(mode, state)[8], 255);

	state.strobeHz = 13.0f;
	CHECK(near(renderState(mode, state)[8], 138));
}

TEST(rgbw_pulls_white_out_of_the_mix)
{
	LightState state;
	state.intensity = 1.0f;
	state.colorMix = 1.0f;
	state.hue = 0.0f;

	// Fully saturated: all red, no white.
	state.saturation = 1.0f;
	auto values = renderState(rgbwMode(), state);
	CHECK_EQ(values[1], 255); // rouge
	CHECK_EQ(values[2], 0);
	CHECK_EQ(values[3], 0);
	CHECK_EQ(values[4], 0);   // blanc

	// Desaturated: the white takes over rather than being made from the three
	// colours, which would be dimmer.
	state.saturation = 0.0f;
	values = renderState(rgbwMode(), state);
	CHECK_EQ(values[1], 0);
	CHECK_EQ(values[2], 0);
	CHECK_EQ(values[3], 0);
	CHECK_EQ(values[4], 255);
}

TEST(without_a_dimmer_intensity_folds_into_the_colours)
{
	FixtureMode mode;
	mode.channels = {
		{ChannelRole::Red, "R", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Green, "V", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Blue, "B", 0, 0, 255, 0, 128, 0, 0},
	};

	LightState state;
	state.intensity = 0.5f;
	state.hue = 0.0f;
	state.saturation = 1.0f;

	const auto values = renderState(mode, state);
	// Without that fold-in, the fixture would stay at full output.
	CHECK(near(values[0], 128));
}

TEST(an_out_of_range_hue_is_wrapped_into_the_circle)
{
	// An effect rotating the hue naturally runs past 360.
	const auto a = hsToRgb(30.0f, 1.0f);
	const auto b = hsToRgb(390.0f, 1.0f);
	const auto c = hsToRgb(-330.0f, 1.0f);

	CHECK(std::abs(a.r - b.r) < 0.001f);
	CHECK(std::abs(a.g - b.g) < 0.001f);
	CHECK(std::abs(a.r - c.r) < 0.001f);
	CHECK(std::abs(a.g - c.g) < 0.001f);
}

TEST(colour_temperature_to_rgb_runs_warm_to_cool)
{
	const auto chaud = cctToRgb(2500.0f);
	const auto froid = cctToRgb(7500.0f);

	// A warm light has more red than blue, a cool one the reverse.
	CHECK(chaud.r > chaud.b);
	CHECK(froid.b > chaud.b);
	CHECK(froid.b >= froid.r - 0.01f);
}

TEST(unknown_channels_keep_their_default_value)
{
	FixtureMode mode;
	mode.channels = {
		{ChannelRole::Dimmer, "Intensite", 0, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Gobo, "Gobos", 17, 0, 255, 0, 128, 0, 0},
		{ChannelRole::Unused, "Reset", 42, 0, 255, 0, 128, 0, 0},
	};

	LightState state;
	state.intensity = 1.0f;

	const auto values = renderState(mode, state);
	CHECK_EQ(values[0], 255);
	CHECK_EQ(values[1], 17); // le profil decide, pas le moteur
	CHECK_EQ(values[2], 42);
}


TEST(a_fade_takes_the_shortest_way_round_the_circle)
{
	LightState rouge;
	rouge.hue = 350.0f;
	LightState orange;
	orange.hue = 10.0f;

	// 350 to 10 is 20 degrees the short way round, not 340.
	const auto milieu = lerp(rouge, orange, 0.5f);
	const float h = std::fmod(std::fmod(milieu.hue, 360.0f) + 360.0f, 360.0f);
	CHECK(h > 359.0f || h < 1.0f);

	// And the other way round.
	const auto retour = lerp(orange, rouge, 0.5f);
	const float h2 = std::fmod(std::fmod(retour.hue, 360.0f) + 360.0f, 360.0f);
	CHECK(h2 > 359.0f || h2 < 1.0f);
}

TEST(a_fade_does_cross_the_spectrum_when_that_is_shortest)
{
	LightState rouge;
	rouge.hue = 0.0f;
	LightState cyan;
	cyan.hue = 180.0f;

	// At exactly 180 degrees there is no shorter way; we only want the result to
	// stay on the circle and to actually move.
	const auto milieu = lerp(rouge, cyan, 0.5f);
	CHECK(std::abs(std::abs(milieu.hue) - 90.0f) < 0.01f);
}

TEST(a_fade_switches_the_strobe_at_the_halfway_point)
{
	LightState calme;
	LightState strobe;
	strobe.strobeHz = 10.0f;

	// An in-between rate would give an erratic flicker.
	CHECK_EQ(lerp(calme, strobe, 0.4f).strobeHz, 0.0f);
	CHECK_EQ(lerp(calme, strobe, 0.6f).strobeHz, 10.0f);
}

TEST(a_fade_from_white_to_colour_is_gradual)
{
	auto blanc = LightState::white(3200.0f);
	blanc.intensity = 1.0f;

	LightState bleu;
	bleu.intensity = 1.0f;
	bleu.colorMix = 1.0f;
	bleu.hue = 240.0f;
	bleu.saturation = 1.0f;

	FixtureMode mode = t4cMode3();

	// The T4c's crossfade channel follows the progression instead of switching.
	CHECK_EQ(renderState(mode, lerp(blanc, bleu, 0.0f))[3], 0);
	CHECK(near(renderState(mode, lerp(blanc, bleu, 0.5f))[3], 128));
	CHECK_EQ(renderState(mode, lerp(blanc, bleu, 1.0f))[3], 255);

	// Saturation rises with the fade; it does not jump.
	CHECK_EQ(renderState(mode, lerp(blanc, bleu, 0.0f))[5], 0);
	CHECK(near(renderState(mode, lerp(blanc, bleu, 0.5f))[5], 128));
}
