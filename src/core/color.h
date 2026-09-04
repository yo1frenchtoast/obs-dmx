#pragma once

#include "core/fixture-profile.h"

#include <cstdint>
#include <vector>

namespace obsdmx {

/// The lighting intent, expressed independently of any hardware.
///
/// This is what programmes and effects manipulate; the translation onto channels
/// happens once, here, according to what the fixture can actually do.
struct LightState {
	float intensity = 0.0f; ///< 0 a 1

	/// Balance between the white engine and the colour engine: 0 for a white
	/// defined by its temperature, 1 for a hue.
	///
	/// This is a continuous quantity rather than a binary choice, because the
	/// hardware treats it that way: the T4c devotes a crossfade channel to it.
	/// It also makes transitions between programmes interpolable without a
	/// discontinuity.
	float colorMix = 1.0f;

	float hue = 0.0f;        ///< degres, 0 a 360
	float saturation = 1.0f; ///< 0 a 1

	float cct = 5600.0f;       ///< kelvins
	float greenMagenta = 0.0f; ///< -1 (plein moins vert) a +1 (plein plus vert)

	float strobeHz = 0.0f; ///< 0 : pas de strobe

	static LightState black()
	{
		LightState state;
		state.intensity = 0.0f;
		return state;
	}

	/// White at the given temperature.
	static LightState white(float kelvin)
	{
		LightState state;
		state.colorMix = 0.0f;
		state.cct = kelvin;
		return state;
	}
};

/// Interpolates between two states, for fades between programmes.
///
/// Hue takes the shortest way round the circle: without that, a fade from red
/// (350 degrees) to orange (10 degrees) would sweep the whole spectrum.
LightState lerp(const LightState &from, const LightState &to, float t);

struct Rgb {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
};

/// Hue and saturation to RGB at full brightness: intensity is carried
/// separately by the dimmer.
Rgb hsToRgb(float hueDegrees, float saturation);

/// Black-body approximation, for fixtures without a colour temperature
/// channel.
Rgb cctToRgb(float kelvin);

/// Translates an intent into DMX values for a given mode.
/// The returned vector has exactly mode.channelCount() elements.
std::vector<uint8_t> renderState(const FixtureMode &mode, const LightState &state);

} // namespace obsdmx
