#include "core/fixture-profile.h"

#include <algorithm>
#include <array>

namespace obsdmx {

namespace {

struct RoleName {
	ChannelRole role;
	std::string_view name;
};

/// One table, walked in both directions, so that writing and reading a profile
/// cannot drift apart.
constexpr std::array kRoleNames = {
	RoleName{ChannelRole::Dimmer, "dimmer"},
	RoleName{ChannelRole::Red, "red"},
	RoleName{ChannelRole::Green, "green"},
	RoleName{ChannelRole::Blue, "blue"},
	RoleName{ChannelRole::White, "white"},
	RoleName{ChannelRole::Amber, "amber"},
	RoleName{ChannelRole::UltraViolet, "uv"},
	RoleName{ChannelRole::Hue, "hue"},
	RoleName{ChannelRole::Saturation, "saturation"},
	RoleName{ChannelRole::Cct, "cct"},
	RoleName{ChannelRole::GreenMagenta, "green_magenta"},
	RoleName{ChannelRole::ColorMix, "color_mix"},
	RoleName{ChannelRole::Strobe, "strobe"},
	RoleName{ChannelRole::Pan, "pan"},
	RoleName{ChannelRole::PanFine, "pan_fine"},
	RoleName{ChannelRole::Tilt, "tilt"},
	RoleName{ChannelRole::TiltFine, "tilt_fine"},
	RoleName{ChannelRole::Gobo, "gobo"},
	RoleName{ChannelRole::ColorWheel, "color_wheel"},
	RoleName{ChannelRole::Fog, "fog"},
	RoleName{ChannelRole::FxSelect, "fx_select"},
	RoleName{ChannelRole::FxControl, "fx_control"},
	RoleName{ChannelRole::FxParameter, "fx_parameter"},
};

} // namespace

ChannelRole roleFromString(std::string_view name)
{
	const auto it = std::find_if(kRoleNames.begin(), kRoleNames.end(),
				     [name](const RoleName &entry) { return entry.name == name; });
	return it != kRoleNames.end() ? it->role : ChannelRole::Unused;
}

int FixtureMode::findRole(ChannelRole role) const
{
	const auto it = std::find_if(channels.begin(), channels.end(),
				     [role](const ChannelSpec &spec) { return spec.role == role; });
	return it != channels.end() ? static_cast<int>(std::distance(channels.begin(), it)) : -1;
}

const FixtureMode *FixtureProfile::findMode(const std::string &modeId) const
{
	const auto it = std::find_if(modes.begin(), modes.end(),
				     [&modeId](const FixtureMode &mode) { return mode.id == modeId; });
	return it != modes.end() ? &*it : nullptr;
}

const FixtureMode *FixtureProfile::preferredMode() const
{
	if (!defaultMode.empty())
		if (const auto *mode = findMode(defaultMode))
			return mode;
	return modes.empty() ? nullptr : &modes.front();
}

} // namespace obsdmx
