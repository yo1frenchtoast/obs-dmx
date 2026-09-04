#pragma once

#include "core/color.h"
#include "core/fixture-library.h"
#include "core/universe.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// A fixture declared by the user.
struct Fixture {
	/// Stable identifier, independent of the name: programmes refer to it.
	std::string id;
	std::string name;

	std::string profileId;
	/// Chosen DMX mode. It must match the setting made on the fixture itself,
	/// which nothing here can verify.
	std::string modeId;

	uint16_t universe = 0;
	/// Start address, 1 to 512.
	int address = 1;

	/// Rank in the selection list, so fixtures appear in the order they are
	/// rigged in the room.
	int order = 0;
};

/// An address overlap between two fixtures.
struct AddressConflict {
	std::string firstFixtureId;
	std::string secondFixtureId;
	uint16_t universe = 0;
	int firstAddress = 0;
	int secondAddress = 0;
};

/// Every declared fixture, and what can be worked out from them.
class Patch {
public:
	explicit Patch(const FixtureLibrary &library) : library_(&library) {}

	const std::vector<Fixture> &fixtures() const { return fixtures_; }

	/// Adds a fixture. The identifier is generated if left empty.
	const Fixture &add(Fixture fixture);
	bool remove(const std::string &fixtureId);
	void clear() { fixtures_.clear(); }

	Fixture *find(const std::string &fixtureId);
	const Fixture *find(const std::string &fixtureId) const;

	/// Effective DMX mode of a fixture, or nullptr if its profile is gone.
	const FixtureMode *modeOf(const Fixture &fixture) const;

	/// Channels occupied, 0 if the profile cannot be found.
	size_t footprintOf(const Fixture &fixture) const;

	/// First free address able to hold a fixture of this size.
	/// Returns 0 if the universe is full.
	int suggestAddress(uint16_t universe, size_t channelCount) const;

	/// Address overlaps. Empty when all is well.
	std::vector<AddressConflict> conflicts() const;

	/// Writes a fixture's lighting state into the matching universe.
	/// Fixtures whose profile cannot be found are skipped.
	void renderFixture(const Fixture &fixture, const LightState &state, std::vector<Universe> &universes) const;

	/// Writes a raw value to one of the fixture's channels, index 0 relative to
	/// its address. Used by built-in effects, which do not go through a
	/// lighting intent.
	void writeChannel(const Fixture &fixture, int channelIndex, uint8_t value,
			  std::vector<Universe> &universes) const;

private:
	const FixtureLibrary *library_;
	std::vector<Fixture> fixtures_;
	int nextId_ = 1;
};

} // namespace obsdmx
