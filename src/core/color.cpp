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

/// Place une grandeur physique dans les bornes utiles du canal.
uint8_t mapPhysical(const ChannelSpec &spec, float value)
{
	if (spec.physicalMax <= spec.physicalMin)
		return spec.rangeMin;

	const float t = clamp01((value - spec.physicalMin) / (spec.physicalMax - spec.physicalMin));
	const float span = static_cast<float>(spec.rangeMax) - static_cast<float>(spec.rangeMin);
	return static_cast<uint8_t>(std::lround(spec.rangeMin + t * span));
}

/// Canal bipolaire : -1 vers rangeMin, 0 vers la valeur neutre, +1 vers rangeMax.
/// Deux segments lineaires, pour que la bande neutre du constructeur soit
/// respectee au lieu d'etre traversee.
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

	// Ramene la teinte dans [0, 360) sans supposer qu'elle y etait deja :
	// un effet qui fait tourner la teinte deborde naturellement.
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
	// Approximation usuelle du corps noir, suffisante pour de l'eclairage :
	// on cherche une teinte credible, pas une mesure colorimetrique.
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

	const bool wantsTint = state.mode == ColorMode::Tint;

	// Un appareil sans gradateur doit voir l'intensite se reporter sur ses
	// canaux de couleur, sinon il reste allume a pleine puissance.
	const bool hasDimmer = mode.hasRole(ChannelRole::Dimmer);
	const float colorScale = hasDimmer ? 1.0f : clamp01(state.intensity);

	// Ce que l'appareil doit afficher, en RVB, si l'on doit passer par la.
	const Rgb rgb = wantsTint ? hsToRgb(state.hue, state.saturation) : cctToRgb(state.cct);
	const float saturation = wantsTint ? clamp01(state.saturation) : 0.0f;

	// Pour un appareil RGBW, on sort le blanc du melange plutot que de le
	// fabriquer avec les trois couleurs : c'est plus lumineux et plus propre.
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
			out = toByte(wantsTint ? state.saturation : 0.0f);
			break;

		case ChannelRole::Cct:
			out = mapPhysical(spec, state.cct);
			break;
		case ChannelRole::GreenMagenta:
			out = mapBipolar(spec, state.greenMagenta);
			break;

		case ChannelRole::ColorMix:
			// Sans ce canal a fond, un appareil dote de deux moteurs
			// reste sur son blanc et ignore la teinte demandee.
			out = wantsTint ? spec.rangeMax : spec.rangeMin;
			break;

		case ChannelRole::Strobe:
			out = state.strobeHz > 0.0f ? mapPhysical(spec, state.strobeHz) : spec.offValue;
			break;

		// Ces canaux ne sont pas pilotes par une intention lumineuse : ils
		// gardent la valeur prevue par le profil.
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

} // namespace obsdmx
