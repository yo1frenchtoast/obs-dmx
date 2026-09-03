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
		std::fprintf(stderr, "  (chargement du profil de test : %s)\n", error.c_str());
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

TEST(bibliotheque_lit_un_profil_et_ses_modes)
{
	const auto library = buildLibrary();
	CHECK_EQ(library.profiles().size(), size_t(1));

	const auto *profile = library.find("test-par");
	CHECK(profile != nullptr);
	CHECK_EQ(profile->modes.size(), size_t(2));
	CHECK(profile->displayName() == "Test PAR");

	// Le mode par defaut est celui declare, pas le premier venu.
	CHECK(profile->preferredMode()->id == "4ch");
	CHECK_EQ(profile->findMode("8ch")->channelCount(), size_t(8));
}

TEST(bibliotheque_refuse_un_profil_sans_identifiant)
{
	FixtureLibrary library;
	std::string error;
	CHECK(!library.loadJson(R"({"model": "sans id", "modes": []})", error));
	CHECK(!error.empty());
	CHECK(library.empty());
}

TEST(bibliotheque_refuse_un_json_casse)
{
	FixtureLibrary library;
	std::string error;
	CHECK(!library.loadJson("{ ceci n'est pas du json", error));
	CHECK(library.empty());
}

TEST(bibliotheque_remplace_un_profil_recharge)
{
	FixtureLibrary library;
	std::string error;
	const char *doc = R"({"id":"x","model":"A","modes":[{"id":"m","channels":[{"role":"dimmer"}]}]})";
	CHECK(library.loadJson(doc, error));
	CHECK(library.loadJson(doc, error));
	// Un rechargement ne doit pas doubler l'entree.
	CHECK_EQ(library.profiles().size(), size_t(1));
}

TEST(patch_propose_la_premiere_adresse_libre)
{
	const auto library = buildLibrary();
	Patch patch(library);

	CHECK_EQ(patch.suggestAddress(0, 4), 1);

	patch.add(makeFixture("a", 1));
	CHECK_EQ(patch.suggestAddress(0, 4), 5);

	patch.add(makeFixture("b", 5));
	CHECK_EQ(patch.suggestAddress(0, 4), 9);

	// Un trou laisse par une suppression doit etre reutilise.
	patch.remove("a");
	CHECK_EQ(patch.suggestAddress(0, 4), 1);
}

TEST(patch_ne_propose_rien_quand_l_univers_est_plein)
{
	const auto library = buildLibrary();
	Patch patch(library);

	// 128 appareils de 4 canaux remplissent exactement les 512 emplacements.
	for (int i = 0; i < 128; ++i)
		patch.add(makeFixture("f" + std::to_string(i), 1 + i * 4));

	CHECK_EQ(patch.suggestAddress(0, 4), 0);
	CHECK(patch.conflicts().empty());
}

TEST(patch_detecte_les_chevauchements_d_adresses)
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

TEST(patch_ignore_les_chevauchements_entre_univers_differents)
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

TEST(patch_ecrit_a_la_bonne_adresse)
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
	// Rien ne doit deborder avant ni apres.
	CHECK_EQ(universes[0].get(9), 0);
	CHECK_EQ(universes[0].get(14), 0);
}

TEST(patch_ignore_un_projecteur_dont_le_profil_a_disparu)
{
	const auto library = buildLibrary();
	Patch patch(library);

	auto orphan = makeFixture("orphelin", 1);
	orphan.profileId = "profil-inexistant";
	patch.add(orphan);

	CHECK_EQ(patch.footprintOf(*patch.find("orphelin")), size_t(0));
	CHECK(patch.conflicts().empty());

	// Le rendu ne doit ni planter ni ecrire n'importe quoi.
	std::vector<Universe> universes{Universe(0)};
	patch.renderFixture(*patch.find("orphelin"), LightState(), universes);
	CHECK_EQ(universes[0].get(1), 0);
}

TEST(patch_retombe_sur_le_mode_par_defaut_si_le_mode_a_disparu)
{
	const auto library = buildLibrary();
	Patch patch(library);

	auto fixture = makeFixture("a", 1, "mode-supprime");
	patch.add(fixture);

	// Plutot que de faire disparaitre le projecteur, on reprend le mode
	// prefere du profil.
	CHECK_EQ(patch.footprintOf(*patch.find("a")), size_t(4));
}
