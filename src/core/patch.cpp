#include "core/patch.h"

#include <algorithm>

namespace obsdmx {

const Fixture &Patch::add(Fixture fixture)
{
	if (fixture.id.empty())
		fixture.id = "fixture-" + std::to_string(nextId_++);
	if (fixture.order == 0)
		fixture.order = static_cast<int>(fixtures_.size());

	fixtures_.push_back(std::move(fixture));
	return fixtures_.back();
}

bool Patch::remove(const std::string &fixtureId)
{
	const auto it = std::find_if(fixtures_.begin(), fixtures_.end(),
				     [&fixtureId](const Fixture &f) { return f.id == fixtureId; });
	if (it == fixtures_.end())
		return false;
	fixtures_.erase(it);
	return true;
}

Fixture *Patch::find(const std::string &fixtureId)
{
	const auto it = std::find_if(fixtures_.begin(), fixtures_.end(),
				     [&fixtureId](const Fixture &f) { return f.id == fixtureId; });
	return it != fixtures_.end() ? &*it : nullptr;
}

const Fixture *Patch::find(const std::string &fixtureId) const
{
	return const_cast<Patch *>(this)->find(fixtureId);
}

const FixtureMode *Patch::modeOf(const Fixture &fixture) const
{
	const FixtureProfile *profile = library_->find(fixture.profileId);
	if (!profile)
		return nullptr;

	// A profile may have lost a mode between versions: fall back on its
	// preferred mode rather than making the fixture vanish.
	if (const FixtureMode *mode = profile->findMode(fixture.modeId))
		return mode;
	return profile->preferredMode();
}

size_t Patch::footprintOf(const Fixture &fixture) const
{
	const FixtureMode *mode = modeOf(fixture);
	return mode ? mode->channelCount() : 0;
}

int Patch::suggestAddress(uint16_t universe, size_t channelCount) const
{
	if (channelCount == 0 || channelCount > kSlotsPerUniverse)
		return 0;

	// Slots already taken in this universe.
	std::vector<bool> taken(kSlotsPerUniverse + 1, false);
	for (const auto &fixture : fixtures_) {
		if (fixture.universe != universe)
			continue;
		const size_t span = footprintOf(fixture);
		for (size_t i = 0; i < span; ++i) {
			const int slot = fixture.address + static_cast<int>(i);
			if (slot >= 1 && slot <= kSlotsPerUniverse)
				taken[static_cast<size_t>(slot)] = true;
		}
	}

	for (int start = 1; start + static_cast<int>(channelCount) - 1 <= kSlotsPerUniverse; ++start) {
		bool free = true;
		for (size_t i = 0; i < channelCount && free; ++i)
			free = !taken[static_cast<size_t>(start) + i];
		if (free)
			return start;
	}
	return 0;
}

std::vector<AddressConflict> Patch::conflicts() const
{
	std::vector<AddressConflict> found;

	for (size_t i = 0; i < fixtures_.size(); ++i) {
		const Fixture &a = fixtures_[i];
		const int aSpan = static_cast<int>(footprintOf(a));
		if (aSpan == 0)
			continue;

		for (size_t j = i + 1; j < fixtures_.size(); ++j) {
			const Fixture &b = fixtures_[j];
			if (a.universe != b.universe)
				continue;

			const int bSpan = static_cast<int>(footprintOf(b));
			if (bSpan == 0)
				continue;

			// Two ranges overlap when each starts before the other
			// ends.
			if (a.address < b.address + bSpan && b.address < a.address + aSpan)
				found.push_back({a.id, b.id, a.universe, a.address, b.address});
		}
	}

	return found;
}

void Patch::renderFixture(const Fixture &fixture, const LightState &state, std::vector<Universe> &universes) const
{
	const FixtureMode *mode = modeOf(fixture);
	if (!mode)
		return;

	const auto it = std::find_if(universes.begin(), universes.end(),
				     [&fixture](const Universe &u) { return u.id() == fixture.universe; });
	if (it == universes.end())
		return;

	const auto values = renderState(*mode, state);
	for (size_t i = 0; i < values.size(); ++i)
		it->set(fixture.address + static_cast<int>(i), values[i]);
}

void Patch::writeChannel(const Fixture &fixture, int channelIndex, uint8_t value,
			 std::vector<Universe> &universes) const
{
	const FixtureMode *mode = modeOf(fixture);
	if (!mode || channelIndex < 0 || channelIndex >= static_cast<int>(mode->channelCount()))
		return;

	const auto it = std::find_if(universes.begin(), universes.end(),
				     [&fixture](const Universe &u) { return u.id() == fixture.universe; });
	if (it == universes.end())
		return;

	it->set(fixture.address + channelIndex, value);
}

} // namespace obsdmx
