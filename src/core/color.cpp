#include "core/color.h"

#include <algorithm>
#include <cmath>

namespace obsdmx {

namespace {

float clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

uint8_t toByte(float value01)
{
	return static_cast<uint8_t>(std::lround(clamp01(value01) * 255.0f));
}

/// Maps a physical quantity into the channel's useful range.
uint8_t mapPhysical(const ChannelSpec &spec, float value)
{
	if (spec.physicalMax <= spec.physicalMin)
		return spec.rangeMin;

	const float t = clamp01((value - spec.physicalMin) / (spec.physicalMax - spec.physicalMin));
	const float span = static_cast<float>(spec.rangeMax) - static_cast<float>(spec.rangeMin);
	return static_cast<uint8_t>(std::lround(spec.rangeMin + t * span));
}

/// Bipolar channel: -1 to rangeMin, 0 to the neutral value, +1 to rangeMax.
/// Two linear segments, so the manufacturer's neutral band is honoured instead
/// of being driven straight through.
uint8_t mapBipolar(const ChannelSpec &spec, float value)
{
	const float amount = std::clamp(value, -1.0f, 1.0f);
	const float neutral = static_cast<float>(spec.neutralValue);

	const float result = amount < 0.0f ? neutral + amount * (neutral - static_cast<float>(spec.rangeMin))
					   : neutral + amount * (static_cast<float>(spec.rangeMax) - neutral);
	return static_cast<uint8_t>(std::lround(std::clamp(result, 0.0f, 255.0f)));
}

} // namespace

Rgb hsToRgb(float hueDegrees, float saturation)
{
	const float s = clamp01(saturation);

	// Wraps the hue into [0, 360) without assuming it already was: an effect
	// that rotates the hue naturally runs past the end.
	float h = std::fmod(hueDegrees, 360.0f);
	if (h < 0.0f)
		h += 360.0f;

	const float sector = h / 60.0f;
	const int index = static_cast<int>(std::floor(sector)) % 6;
	const float f = sector - std::floor(sector);

	const float p = 1.0f - s;
	const float q = 1.0f - s * f;
	const float t = 1.0f - s * (1.0f - f);

	switch (index) {
	case 0: return {1.0f, t, p};
	case 1: return {q, 1.0f, p};
	case 2: return {p, 1.0f, t};
	case 3: return {p, q, 1.0f};
	case 4: return {t, p, 1.0f};
	default: return {1.0f, p, q};
	}
}

Rgb cctToRgb(float kelvin)
{
	// The usual black-body approximation, good enough for lighting: we want a
	// believable tint, not a colorimetric measurement.
	const float t = std::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;

	float r, g, b;

	if (t <= 66.0f) {
		r = 1.0f;
		g = clamp01((99.4708025861f * std::log(t) - 161.1195681661f) / 255.0f);
		b = t <= 19.0f ? 0.0f : clamp01((138.5177312231f * std::log(t - 10.0f) - 305.0447927307f) / 255.0f);
	} else {
		r = clamp01(329.698727446f * std::pow(t - 60.0f, -0.1332047592f) / 255.0f);
		g = clamp01(288.1221695283f * std::pow(t - 60.0f, -0.0755148492f) / 255.0f);
		b = 1.0f;
	}

	return {r, g, b};
}

std::vector<uint8_t> renderState(const FixtureMode &mode, const LightState &state)
{
	std::vector<uint8_t> values(mode.channelCount());
	for (size_t i = 0; i < mode.channelCount(); ++i)
		values[i] = mode.channels[i].defaultValue;

	const float mix = clamp01(state.colorMix);

	// A fixture without a dimmer must have the intensity folded into its
	// colour channels, otherwise it stays at full output.
	const bool hasDimmer = mode.hasRole(ChannelRole::Dimmer);
	const float colorScale = hasDimmer ? 1.0f : clamp01(state.intensity);

	// What the fixture should show, in RGB, if we have to go through it.
	// Fixtures without a crossfade channel get the blend done here.
	const Rgb tint = hsToRgb(state.hue, state.saturation);
	const Rgb whiteRgb = cctToRgb(state.cct);
	const Rgb rgb = {whiteRgb.r + (tint.r - whiteRgb.r) * mix, whiteRgb.g + (tint.g - whiteRgb.g) * mix,
			 whiteRgb.b + (tint.b - whiteRgb.b) * mix};

	// Effective saturation falls to zero as we return towards white.
	const float saturation = clamp01(state.saturation) * mix;

	// On an RGBW fixture, pull the white out of the mix rather than making it
	// from the three colours: it is brighter and cleaner.
	const bool hasWhite = mode.hasRole(ChannelRole::White);
	const float white = hasWhite ? (1.0f - saturation) : 0.0f;
	const float whiteRemoval = hasWhite ? white : 0.0f;

	for (size_t i = 0; i < mode.channelCount(); ++i) {
		const ChannelSpec &spec = mode.channels[i];
		uint8_t &out = values[i];

		switch (spec.role) {
		case ChannelRole::Dimmer:
			out = toByte(state.intensity);
			break;

		case ChannelRole::Red:
			out = toByte((rgb.r - whiteRemoval) * colorScale);
			break;
		case ChannelRole::Green:
			out = toByte((rgb.g - whiteRemoval) * colorScale);
			break;
		case ChannelRole::Blue:
			out = toByte((rgb.b - whiteRemoval) * colorScale);
			break;
		case ChannelRole::White:
			out = toByte(white * colorScale);
			break;

		case ChannelRole::Hue:
			out = toByte(std::fmod(std::fmod(state.hue, 360.0f) + 360.0f, 360.0f) / 360.0f);
			break;
		case ChannelRole::Saturation:
			out = toByte(saturation);
			break;

		case ChannelRole::Cct:
			out = mapPhysical(spec, state.cct);
			break;
		case ChannelRole::GreenMagenta:
			out = mapBipolar(spec, state.greenMagenta);
			break;

		case ChannelRole::ColorMix: {
			// Without this channel opened, a fixture with two engines
			// stays on its white and ignores the requested hue.
			const float span = static_cast<float>(spec.rangeMax) - static_cast<float>(spec.rangeMin);
			out = static_cast<uint8_t>(std::lround(spec.rangeMin + mix * span));
			break;
		}

		case ChannelRole::Strobe:
			out = state.strobeHz > 0.0f ? mapPhysical(spec, state.strobeHz) : spec.offValue;
			break;

		// These channels are not driven by a lighting intent: they keep
		// whatever the profile decided.
		case ChannelRole::Amber:
		case ChannelRole::UltraViolet:
		case ChannelRole::Pan:
		case ChannelRole::PanFine:
		case ChannelRole::Tilt:
		case ChannelRole::TiltFine:
		case ChannelRole::Gobo:
		case ChannelRole::ColorWheel:
		case ChannelRole::Fog:
		case ChannelRole::FxSelect:
		case ChannelRole::FxControl:
		case ChannelRole::FxParameter:
		case ChannelRole::Unused:
			break;
		}
	}

	return values;
}

LightState lerp(const LightState &from, const LightState &to, float t)
{
	const float k = clamp01(t);

	LightState state;
	state.intensity = from.intensity + (to.intensity - from.intensity) * k;
	state.colorMix = from.colorMix + (to.colorMix - from.colorMix) * k;
	state.saturation = from.saturation + (to.saturation - from.saturation) * k;
	state.cct = from.cct + (to.cct - from.cct) * k;
	state.greenMagenta = from.greenMagenta + (to.greenMagenta - from.greenMagenta) * k;

	// Strobe does not fade: an in-between rate is meaningless and would give an
	// erratic flicker during the transition. It switches at the halfway
	// point.
	state.strobeHz = k < 0.5f ? from.strobeHz : to.strobeHz;

	// Shortest way round the colour circle: a fade from red to orange must not
	// sweep the whole spectrum.
	float delta = std::fmod(to.hue - from.hue, 360.0f);
	if (delta > 180.0f)
		delta -= 360.0f;
	else if (delta < -180.0f)
		delta += 360.0f;
	state.hue = from.hue + delta * k;

	return state;
}

} // namespace obsdmx
