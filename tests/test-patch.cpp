#include "test-harness.h"

#include "core/fixture-library.h"
#include "core/patch.h"

using namespace obsdmx;

namespace {

FixtureLibrary buildLibrary()
{
	FixtureLibrary library;
	std::string error;

	const bool ok = library.loadJson(R"({
		"id": "test-par",
		"manufacturer": "Test",
		"model": "PAR",
		"default_mode": "4ch",
		"modes": [
			{"id": "4ch", "label": "4 canaux", "channels": [
				{"role": "dimmer"}, {"role": "red"}, {"role": "green"}, {"role": "blue"}]},
			{"id": "8ch", "label": "8 canaux", "channels": [
				{"role": "dimmer"}, {"role": "red"}, {"role": "green"}, {"role": "blue"},
				{"role": "white"}, {"role": "strobe"}, {"role": "unused"}, {"role": "unused"}]}
		]
	})", error);

	if (!ok)
		std::fprintf(stderr, "  (loading the test profile: %s)\n", error.c_str());
	return library;
}

Fixture makeFixture(const std::string &id, int address, const std::string &mode = "4ch")
{
	Fixture fixture;
	fixture.id = id;
	fixture.name = id;
	fixture.profileId = "test-par";
	fixture.modeId = mode;
	fixture.address = address;
	return fixture;
}

} // namespace

TEST(the_library_reads_a_profile_and_its_modes)
{
	const auto library = buildLibrary();
	CHECK_EQ(library.profiles().size(), size_t(1));

	const auto *profile = library.find("test-par");
	CHECK(profile != nullptr);
	CHECK_EQ(profile->modes.size(), size_t(2));
	CHECK(profile->displayName() == "Test PAR");

	// The default mode is the declared one, not just the first.
	CHECK(profile->preferredMode()->id == "4ch");
	CHECK_EQ(profile->findMode("8ch")->channelCount(), size_t(8));
}

TEST(the_library_rejects_a_profile_without_an_id)
{
	FixtureLibrary library;
	std::string error;
	CHECK(!library.loadJson(R"({"model": "sans id", "modes": []})", error));
	CHECK(!error.empty());
	CHECK(library.empty());
}

TEST(the_library_rejects_broken_json)
{
	FixtureLibrary library;
	std::string error;
	CHECK(!library.loadJson("{ ceci n'est pas du json", error));
	CHECK(library.empty());
}

TEST(the_library_replaces_a_reloaded_profile)
{
	FixtureLibrary library;
	std::string error;
	const char *doc = R"({"id":"x","model":"A","modes":[{"id":"m","channels":[{"role":"dimmer"}]}]})";
	CHECK(library.loadJson(doc, error));
	CHECK(library.loadJson(doc, error));
	// Reloading must not duplicate the entry.
	CHECK_EQ(library.profiles().size(), size_t(1));
}

TEST(the_patch_suggests_the_first_free_address)
{
	const auto library = buildLibrary();
	Patch patch(library);

	CHECK_EQ(patch.suggestAddress(0, 4), 1);

	patch.add(makeFixture("a", 1));
	CHECK_EQ(patch.suggestAddress(0, 4), 5);

	patch.add(makeFixture("b", 5));
	CHECK_EQ(patch.suggestAddress(0, 4), 9);

	// A gap left by a deletion must be reused.
	patch.remove("a");
	CHECK_EQ(patch.suggestAddress(0, 4), 1);
}

TEST(the_patch_suggests_nothing_when_the_universe_is_full)
{
	const auto library = buildLibrary();
	Patch patch(library);

	// 128 four-channel fixtures fill the 512 slots exactly.
	for (int i = 0; i < 128; ++i)
		patch.add(makeFixture("f" + std::to_string(i), 1 + i * 4));

	CHECK_EQ(patch.suggestAddress(0, 4), 0);
	CHECK(patch.conflicts().empty());
}

TEST(the_patch_detects_address_overlaps)
{
	const auto library = buildLibrary();
	Patch patch(library);

	patch.add(makeFixture("a", 1, "8ch")); // occupe 1 a 8
	patch.add(makeFixture("b", 5));        // occupe 5 a 8 : chevauchement

	const auto conflicts = patch.conflicts();
	CHECK_EQ(conflicts.size(), size_t(1));
	CHECK(conflicts[0].firstFixtureId == "a");
	CHECK(conflicts[0].secondFixtureId == "b");
}

TEST(the_patch_ignores_overlaps_across_different_universes)
{
	const auto library = buildLibrary();
	Patch patch(library);

	auto a = makeFixture("a", 1);
	auto b = makeFixture("b", 1);
	b.universe = 1;
	patch.add(a);
	patch.add(b);

	CHECK(patch.conflicts().empty());
}

TEST(the_patch_writes_at_the_right_address)
{
	const auto library = buildLibrary();
	Patch patch(library);
	patch.add(makeFixture("a", 10));

	std::vector<Universe> universes{Universe(0)};

	LightState state;
	state.intensity = 1.0f;
	state.hue = 0.0f;
	state.saturation = 1.0f;

	patch.renderFixture(*patch.find("a"), state, universes);

	CHECK_EQ(universes[0].get(10), 255); // intensite
	CHECK_EQ(universes[0].get(11), 255); // rouge
	CHECK_EQ(universes[0].get(12), 0);
	CHECK_EQ(universes[0].get(13), 0);
	// Nothing must spill before or after.
	CHECK_EQ(universes[0].get(9), 0);
	CHECK_EQ(universes[0].get(14), 0);
}

TEST(the_patch_skips_a_fixture_whose_profile_is_gone)
{
	const auto library = buildLibrary();
	Patch patch(library);

	auto orphan = makeFixture("orphelin", 1);
	orphan.profileId = "profil-inexistant";
	patch.add(orphan);

	CHECK_EQ(patch.footprintOf(*patch.find("orphelin")), size_t(0));
	CHECK(patch.conflicts().empty());

	// The render must neither crash nor write nonsense.
	std::vector<Universe> universes{Universe(0)};
	patch.renderFixture(*patch.find("orphelin"), LightState(), universes);
	CHECK_EQ(universes[0].get(1), 0);
}

TEST(the_patch_falls_back_to_the_default_mode_if_the_mode_is_gone)
{
	const auto library = buildLibrary();
	Patch patch(library);

	auto fixture = makeFixture("a", 1, "mode-supprime");
	patch.add(fixture);

	// Rather than making the fixture vanish, we fall back on the profile's
	// preferred mode.
	CHECK_EQ(patch.footprintOf(*patch.find("a")), size_t(4));
}
