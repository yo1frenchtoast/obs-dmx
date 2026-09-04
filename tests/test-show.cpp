#include "test-harness.h"

#include "core/show.h"

#include <cmath>

using namespace obsdmx;
using Clock = Show::Clock;

namespace {

FixtureLibrary buildLibrary()
{
	FixtureLibrary library;
	std::string error;
	library.loadJson(R"({
		"id": "par", "model": "PAR", "default_mode": "4ch",
		"modes": [{"id": "4ch", "channels": [
			{"role":"dimmer"},{"role":"red"},{"role":"green"},{"role":"blue"}]}]
	})", error);
	return library;
}

void addFixture(Show &show, const std::string &id, int address)
{
	show.withPatch([&](Patch &patch) {
		Fixture fixture;
		fixture.id = id;
		fixture.profileId = "par";
		fixture.modeId = "4ch";
		fixture.address = address;
		patch.add(fixture);
	});
}

Program makeProgram(const std::string &id, const std::string &fixtureId, float intensity, float hue)
{
	LightState state;
	state.intensity = intensity;
	state.colorMix = 1.0f;
	state.hue = hue;
	state.saturation = 1.0f;

	Program program;
	program.id = id;
	program.name = id;
	program.looks.push_back({fixtureId, state});
	return program;
}

std::vector<Universe> renderAt(Show &show, Clock::time_point now)
{
	std::vector<Universe> universes{Universe(0)};
	show.render(universes, now, AudioSnapshot{});
	return universes;
}

} // namespace

TEST(a_programme_lights_the_fixtures_it_names)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	addFixture(show, "b", 5);

	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	const auto now = Clock::now();
	show.activateProgram("p1", 0, now);

	const auto universes = renderAt(show, now);
	CHECK_EQ(universes[0].get(1), 255); // "a" allume
	CHECK_EQ(universes[0].get(2), 255); // rouge
	// "b" is not named: it stays dark rather than keeping its state.
	CHECK_EQ(universes[0].get(5), 0);
}

TEST(a_fade_progresses_over_time)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	const auto start = Clock::now();
	show.activateProgram("p1", 1000, start);

	CHECK_EQ(renderAt(show, start)[0].get(1), 0);
	CHECK(std::abs(renderAt(show, start + std::chrono::milliseconds(500))[0].get(1) - 128) <= 2);
	CHECK_EQ(renderAt(show, start + std::chrono::milliseconds(1000))[0].get(1), 255);

	// Past the fade, the value stays put.
	CHECK_EQ(renderAt(show, start + std::chrono::seconds(5))[0].get(1), 255);
}

TEST(an_interrupted_fade_restarts_from_the_current_picture)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));
	show.addProgram(makeProgram("p2", "a", 0.0f, 0.0f));

	const auto start = Clock::now();
	show.activateProgram("p1", 1000, start);

	// Switching halfway: the new fade must start from the value showing at that
	// instant, not from zero.
	const auto middle = start + std::chrono::milliseconds(500);
	show.activateProgram("p2", 1000, middle);

	const int atSwitch = renderAt(show, middle)[0].get(1);
	CHECK(std::abs(atSwitch - 128) <= 3);

	// Then the descent continues without a jump.
	CHECK(renderAt(show, middle + std::chrono::milliseconds(500))[0].get(1) < atSwitch);
	CHECK_EQ(renderAt(show, middle + std::chrono::milliseconds(1000))[0].get(1), 0);
}

TEST(a_scene_with_no_programme_puts_the_lights_out)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	const auto start = Clock::now();
	show.bindScene("uuid-1", "Camera", "p1", 0);
	show.activateScene("uuid-1", start);
	CHECK_EQ(renderAt(show, start)[0].get(1), 255);

	// An unknown scene puts the lights out rather than leaving the previous one lit.
	show.activateScene("uuid-inconnu", start);
	CHECK_EQ(renderAt(show, start + std::chrono::seconds(2))[0].get(1), 0);
}

TEST(attachment_by_identifier_survives_a_rename)
{
	const auto library = buildLibrary();
	Show show(library);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	show.bindScene("uuid-1", "Ancien nom", "p1", 300);
	show.bindScene("uuid-1", "Nouveau nom", "p1", 300);

	// A rename updates the existing entry; it does not create a second one.
	CHECK_EQ(show.bindings().size(), size_t(1));
	CHECK(show.bindingFor("uuid-1")->sceneName == "Nouveau nom");
	CHECK(show.bindingFor("uuid-1")->programId == "p1");
}

TEST(deleting_a_programme_detaches_its_scenes)
{
	const auto library = buildLibrary();
	Show show(library);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));
	show.bindScene("uuid-1", "Camera", "p1", 300);

	CHECK(show.removeProgram("p1"));

	// The attachment survives but no longer points at nothing.
	CHECK_EQ(show.bindings().size(), size_t(1));
	CHECK(show.bindingFor("uuid-1")->programId.empty());
	CHECK(show.activeProgramId().empty());
}

TEST(the_preview_takes_over_without_waiting_for_a_fade)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 0.0f, 0.0f));

	const auto now = Clock::now();
	show.activateProgram("p1", 0, now);
	CHECK_EQ(renderAt(show, now)[0].get(1), 0);

	// The user sets a colour in the editor: they must see it at once, not at the
	// end of a fade.
	show.setPreview(makeProgram("edition", "a", 1.0f, 120.0f));
	CHECK(show.hasPreview());
	CHECK_EQ(renderAt(show, now)[0].get(1), 255);
	CHECK_EQ(renderAt(show, now)[0].get(3), 255); // vert

	// When the editor closes, the active programme takes over again.
	show.setPreview(std::nullopt);
	CHECK_EQ(renderAt(show, now)[0].get(1), 0);
}

TEST(generated_identifiers_never_replay_loaded_ones)
{
	const auto library = buildLibrary();
	Show show(library);

	std::vector<Program> loaded;
	loaded.push_back(makeProgram("program-7", "a", 1.0f, 0.0f));
	show.setPrograms(loaded);

	Program fresh;
	fresh.name = "nouveau";
	const std::string id = show.addProgram(fresh);

	// Without this precaution the new programme would overwrite program-1 and
	// then collide with the loaded program-7.
	CHECK(id != "program-7");
	CHECK(show.program(id).has_value());
	CHECK(show.program("program-7").has_value());
}

TEST(attaching_a_scene_does_not_activate_it_by_itself)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	const auto now = Clock::now();

	// Attaching the current scene lights nothing: activation is driven by an OBS
	// event, which does not happen when attaching the scene already on air. It
	// is up to the caller to replay the scene.
	show.bindScene("uuid-1", "Camera", "p1", 0);
	CHECK(show.activeProgramId().empty());
	CHECK_EQ(renderAt(show, now)[0].get(1), 0);

	// Replaying the scene is then enough.
	show.activateScene("uuid-1", now);
	CHECK(show.activeProgramId() == "p1");
	CHECK_EQ(renderAt(show, now)[0].get(1), 255);
}
