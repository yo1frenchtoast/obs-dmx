#pragma once

#include <cstdint>
#include <string_view>

namespace obsdmx {

/// What a DMX channel does, independently of the fixture model.
///
/// This typing is what lets the interface offer a colour wheel rather than
/// numbered sliders, and lets the engine translate an intent ("blue at 40%")
/// onto whatever channels the fixture actually has.
enum class ChannelRole : uint8_t {
	/// Ignored by the engine, left at its default value.
	Unused = 0,

	Dimmer,

	// Plain additive mixing.
	Red,
	Green,
	Blue,
	White,
	Amber,
	UltraViolet,

	// Hue / saturation model, native to amaran and many recent LED fixtures.
	Hue,
	Saturation,

	// Variable white.
	Cct,
	GreenMagenta,

	/// Crossfade between the white engine and the colour engine. Without it a
	/// T4c in mode 3 stays white whatever is written to Hue.
	ColorMix,

	Strobe,

	// Mechanics.
	Pan,
	PanFine,
	Tilt,
	TiltFine,
	Gobo,
	ColorWheel,
	Fog,

	/// Selects an effect built into the fixture.
	FxSelect,
	/// Parameters of that effect: rate, colour variant, and so on.
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

/// Returns Unused for an unknown name: a profile may name a channel this
/// version does not know yet, which is no reason to reject the whole file.
ChannelRole roleFromString(std::string_view name);

} // namespace obsdmx
