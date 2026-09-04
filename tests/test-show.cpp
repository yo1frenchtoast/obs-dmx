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

TEST(programme_allume_les_projecteurs_qu_il_cite)
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
	// "b" n'est pas cite : il reste eteint plutot que de garder son etat.
	CHECK_EQ(universes[0].get(5), 0);
}

TEST(fondu_progresse_dans_le_temps)
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

	// Au-dela du fondu, la valeur reste stable.
	CHECK_EQ(renderAt(show, start + std::chrono::seconds(5))[0].get(1), 255);
}

TEST(un_fondu_interrompu_repart_de_l_image_courante)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));
	show.addProgram(makeProgram("p2", "a", 0.0f, 0.0f));

	const auto start = Clock::now();
	show.activateProgram("p1", 1000, start);

	// A mi-parcours on bascule : le depart du nouveau fondu doit etre la
	// valeur affichee a cet instant, pas zero.
	const auto middle = start + std::chrono::milliseconds(500);
	show.activateProgram("p2", 1000, middle);

	const int atSwitch = renderAt(show, middle)[0].get(1);
	CHECK(std::abs(atSwitch - 128) <= 3);

	// Puis la descente continue sans saut.
	CHECK(renderAt(show, middle + std::chrono::milliseconds(500))[0].get(1) < atSwitch);
	CHECK_EQ(renderAt(show, middle + std::chrono::milliseconds(1000))[0].get(1), 0);
}

TEST(scene_sans_programme_eteint_la_lumiere)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	const auto start = Clock::now();
	show.bindScene("uuid-1", "Camera", "p1", 0);
	show.activateScene("uuid-1", start);
	CHECK_EQ(renderAt(show, start)[0].get(1), 255);

	// Une scene inconnue eteint, plutot que de laisser la precedente allumee.
	show.activateScene("uuid-inconnu", start);
	CHECK_EQ(renderAt(show, start + std::chrono::seconds(2))[0].get(1), 0);
}

TEST(association_par_identifiant_survit_au_renommage)
{
	const auto library = buildLibrary();
	Show show(library);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	show.bindScene("uuid-1", "Ancien nom", "p1", 300);
	show.bindScene("uuid-1", "Nouveau nom", "p1", 300);

	// Le renommage met a jour l'entree existante, il n'en cree pas une seconde.
	CHECK_EQ(show.bindings().size(), size_t(1));
	CHECK(show.bindingFor("uuid-1")->sceneName == "Nouveau nom");
	CHECK(show.bindingFor("uuid-1")->programId == "p1");
}

TEST(supprimer_un_programme_delie_les_scenes)
{
	const auto library = buildLibrary();
	Show show(library);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));
	show.bindScene("uuid-1", "Camera", "p1", 300);

	CHECK(show.removeProgram("p1"));

	// L'association subsiste mais ne pointe plus dans le vide.
	CHECK_EQ(show.bindings().size(), size_t(1));
	CHECK(show.bindingFor("uuid-1")->programId.empty());
	CHECK(show.activeProgramId().empty());
}

TEST(apercu_prend_la_main_sans_attendre_de_fondu)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 0.0f, 0.0f));

	const auto now = Clock::now();
	show.activateProgram("p1", 0, now);
	CHECK_EQ(renderAt(show, now)[0].get(1), 0);

	// L'utilisateur regle une couleur dans l'editeur : il doit la voir tout
	// de suite, pas au bout d'un fondu.
	show.setPreview(makeProgram("edition", "a", 1.0f, 120.0f));
	CHECK(show.hasPreview());
	CHECK_EQ(renderAt(show, now)[0].get(1), 255);
	CHECK_EQ(renderAt(show, now)[0].get(3), 255); // vert

	// A la fermeture de l'editeur, le programme actif reprend la main.
	show.setPreview(std::nullopt);
	CHECK_EQ(renderAt(show, now)[0].get(1), 0);
}

TEST(identifiants_generes_ne_rejouent_pas_ceux_charges)
{
	const auto library = buildLibrary();
	Show show(library);

	std::vector<Program> loaded;
	loaded.push_back(makeProgram("program-7", "a", 1.0f, 0.0f));
	show.setPrograms(loaded);

	Program fresh;
	fresh.name = "nouveau";
	const std::string id = show.addProgram(fresh);

	// Sans cette precaution, le nouveau programme ecraserait le program-1
	// puis entrerait en collision avec le program-7 charge.
	CHECK(id != "program-7");
	CHECK(show.program(id).has_value());
	CHECK(show.program("program-7").has_value());
}

TEST(associer_une_scene_ne_l_active_pas_a_soi_seul)
{
	const auto library = buildLibrary();
	Show show(library);
	addFixture(show, "a", 1);
	show.addProgram(makeProgram("p1", "a", 1.0f, 0.0f));

	const auto now = Clock::now();

	// Associer la scene courante n'allume rien : l'activation est declenchee
	// par un evenement d'OBS, qui ne survient pas quand on associe la scene
	// deja affichee. C'est a l'appelant de rejouer la scene.
	show.bindScene("uuid-1", "Camera", "p1", 0);
	CHECK(show.activeProgramId().empty());
	CHECK_EQ(renderAt(show, now)[0].get(1), 0);

	// Rejouer la scene suffit alors.
	show.activateScene("uuid-1", now);
	CHECK(show.activeProgramId() == "p1");
	CHECK_EQ(renderAt(show, now)[0].get(1), 255);
}
