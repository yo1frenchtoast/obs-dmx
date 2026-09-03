#pragma once

#include "core/fixture-profile.h"

#include <cstdint>
#include <vector>

namespace obsdmx {

/// Comment l'utilisateur a defini la lumiere : une teinte, ou un blanc.
enum class ColorMode : uint8_t {
	Tint,  ///< teinte et saturation
	White, ///< temperature de couleur
};

/// L'intention lumineuse, exprimee independamment de tout materiel.
///
/// C'est ce que manipulent les programmes et les effets ; la traduction vers
/// les canaux est faite une seule fois, ici, en fonction de ce que l'appareil
/// sait faire.
struct LightState {
	float intensity = 0.0f; ///< 0 a 1
	ColorMode mode = ColorMode::Tint;

	float hue = 0.0f;        ///< degres, 0 a 360
	float saturation = 1.0f; ///< 0 a 1

	float cct = 5600.0f;         ///< kelvins
	float greenMagenta = 0.0f;   ///< -1 (plein moins vert) a +1 (plein plus vert)

	float strobeHz = 0.0f; ///< 0 : pas de strobe

	static LightState black()
	{
		LightState state;
		state.intensity = 0.0f;
		return state;
	}
};

struct Rgb {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
};

/// Teinte et saturation vers RVB, a luminosite maximale : l'intensite est
/// portee separement par le gradateur.
Rgb hsToRgb(float hueDegrees, float saturation);

/// Approximation du rayonnement du corps noir, pour les appareils qui n'ont pas
/// de canal de temperature de couleur.
Rgb cctToRgb(float kelvin);

/// Traduit une intention en valeurs DMX pour un mode donne.
/// Le vecteur renvoye a exactement mode.channelCount() elements.
std::vector<uint8_t> renderState(const FixtureMode &mode, const LightState &state);

} // namespace obsdmx
