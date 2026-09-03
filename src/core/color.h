#pragma once

#include "core/fixture-profile.h"

#include <cstdint>
#include <vector>

namespace obsdmx {

/// L'intention lumineuse, exprimee independamment de tout materiel.
///
/// C'est ce que manipulent les programmes et les effets ; la traduction vers
/// les canaux est faite une seule fois, ici, en fonction de ce que l'appareil
/// sait faire.
struct LightState {
	float intensity = 0.0f; ///< 0 a 1

	/// Dosage entre le moteur blanc et le moteur couleur : 0 pour un blanc
	/// defini par sa temperature, 1 pour une teinte.
	///
	/// C'est une grandeur continue et non un choix binaire, parce que le
	/// materiel la traite ainsi : le T4c y consacre un canal de fondu. Cela
	/// rend aussi les transitions entre programmes interpolables sans
	/// discontinuite.
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

	/// Blanc a la temperature donnee.
	static LightState white(float kelvin)
	{
		LightState state;
		state.colorMix = 0.0f;
		state.cct = kelvin;
		return state;
	}
};

/// Interpolation entre deux etats, pour les fondus entre programmes.
///
/// La teinte suit le plus court chemin sur le cercle : sans cela, un fondu du
/// rouge (350 degres) vers l'orange (10 degres) traverserait tout le spectre.
LightState lerp(const LightState &from, const LightState &to, float t);

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
