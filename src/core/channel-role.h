#pragma once

#include <cstdint>
#include <string_view>

namespace obsdmx {

/// Ce que fait un canal DMX, independamment du modele de projecteur.
///
/// C'est ce typage qui permet a l'interface de proposer une roue de couleur
/// plutot que des curseurs numerotes, et au moteur de traduire une intention
/// ("bleu a 40 %") vers les canaux dont l'appareil dispose reellement.
enum class ChannelRole : uint8_t {
	/// Canal ignore par le moteur, laisse a sa valeur par defaut.
	Unused = 0,

	Dimmer,

	// Melange additif classique.
	Red,
	Green,
	Blue,
	White,
	Amber,
	UltraViolet,

	// Modele teinte / saturation, natif chez amaran et beaucoup de LED recentes.
	Hue,
	Saturation,

	// Blanc variable.
	Cct,
	GreenMagenta,

	/// Fondu enchaine entre le moteur blanc et le moteur couleur. Sans lui,
	/// un T4c en mode 3 reste blanc quoi qu'on ecrive dans Hue.
	ColorMix,

	Strobe,

	// Machinerie.
	Pan,
	PanFine,
	Tilt,
	TiltFine,
	Gobo,
	ColorWheel,
	Fog,

	/// Selection d'un effet embarque dans l'appareil.
	FxSelect,
	/// Parametres de cet effet : frequence, variante de couleur, etc.
	FxControl,
	FxParameter,
};

constexpr std::string_view toString(ChannelRole role)
{
	switch (role) {
	case ChannelRole::Unused: return "unused";
	case ChannelRole::Dimmer: return "dimmer";
	case ChannelRole::Red: return "red";
	case ChannelRole::Green: return "green";
	case ChannelRole::Blue: return "blue";
	case ChannelRole::White: return "white";
	case ChannelRole::Amber: return "amber";
	case ChannelRole::UltraViolet: return "uv";
	case ChannelRole::Hue: return "hue";
	case ChannelRole::Saturation: return "saturation";
	case ChannelRole::Cct: return "cct";
	case ChannelRole::GreenMagenta: return "green_magenta";
	case ChannelRole::ColorMix: return "color_mix";
	case ChannelRole::Strobe: return "strobe";
	case ChannelRole::Pan: return "pan";
	case ChannelRole::PanFine: return "pan_fine";
	case ChannelRole::Tilt: return "tilt";
	case ChannelRole::TiltFine: return "tilt_fine";
	case ChannelRole::Gobo: return "gobo";
	case ChannelRole::ColorWheel: return "color_wheel";
	case ChannelRole::Fog: return "fog";
	case ChannelRole::FxSelect: return "fx_select";
	case ChannelRole::FxControl: return "fx_control";
	case ChannelRole::FxParameter: return "fx_parameter";
	}
	return "unused";
}

/// Renvoie Unused si le nom est inconnu : un profil peut nommer un canal que
/// cette version ne connait pas encore, ce n'est pas une raison pour le rejeter.
ChannelRole roleFromString(std::string_view name);

} // namespace obsdmx
