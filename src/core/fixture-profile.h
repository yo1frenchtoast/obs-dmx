#pragma once

#include "core/channel-role.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace obsdmx {

/// One channel within a given mode.
///
/// Ranges are not always 0-255: the T4c strobe channel is off from 0 to 19 and
/// then covers 1 to 25 Hz from 20 to 255, and its green/magenta channel has a
/// neutral band in the middle. Describing those ranges in the profile keeps one
/// manufacturer's quirks out of the engine.
struct ChannelSpec {
	ChannelRole role = ChannelRole::Unused;
	/// Shown to the user, taken from the manufacturer's chart.
	std::string label;
	/// Sent when the engine does not drive this channel.
	uint8_t defaultValue = 0;

	/// Useful range of the channel.
	uint8_t rangeMin = 0;
	uint8_t rangeMax = 255;
	/// Value meaning "no effect", for channels such as strobe.
	uint8_t offValue = 0;
	/// Midpoint, for bipolar channels such as green/magenta.
	uint8_t neutralValue = 128;

	/// Physical quantity matching rangeMin and rangeMax: kelvins for colour
	/// temperature, hertz for strobe.
	float physicalMin = 0.0f;
	float physicalMax = 0.0f;
};

/// An effect built into the fixture, such as the T4c's FX mode.
struct BuiltinEffect {
	std::string id;
	std::string label;
	/// Value to write to the FxSelect channel.
	uint8_t selectValue = 0;
	/// The effect takes a rate from 1 to 10.
	bool hasFrequency = false;
	/// The rate also accepts a random setting.
	bool hasRandomFrequency = false;
	/// Index, within the mode, of the channel carrying the rate.
	int frequencyChannel = -1;
};

/// A DMX mode. This is chosen on the fixture's own screen; it is not something
/// that can be imposed over DMX.
struct FixtureMode {
	std::string id;
	std::string label;
	std::vector<ChannelSpec> channels;

	/// Built-in effects available in this mode, if it is an FX mode.
	std::vector<BuiltinEffect> effects;

	size_t channelCount() const { return channels.size(); }

	/// Index of the first channel with this role, or -1.
	int findRole(ChannelRole role) const;
	bool hasRole(ChannelRole role) const { return findRole(role) >= 0; }
};

/// A fixture model, with all of its modes.
struct FixtureProfile {
	std::string id;
	std::string manufacturer;
	std::string model;
	/// Fixture-specific warning, shown when adding one.
	std::string note;
	/// Mode offered by default when adding one.
	std::string defaultMode;
	std::vector<FixtureMode> modes;

	std::string displayName() const
	{
		return manufacturer.empty() ? model : manufacturer + " " + model;
	}

	const FixtureMode *findMode(const std::string &id) const;
	const FixtureMode *preferredMode() const;
};

} // namespace obsdmx
