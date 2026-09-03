#include "test-harness.h"

#include "core/fixture-library.h"
#include "core/patch.h"

using namespace obsdmx;

#ifndef OBS_DMX_FIXTURES_DIR
#define OBS_DMX_FIXTURES_DIR "data/fixtures"
#endif

namespace {

FixtureLibrary loadShipped(std::vector<std::string> &warnings)
{
	FixtureLibrary library;
	library.loadDirectory(OBS_DMX_FIXTURES_DIR, warnings);
	return library;
}

} // namespace

TEST(profils_livres_se_chargent_tous_sans_avertissement)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	for (const auto &warning : warnings)
		std::fprintf(stderr, "    avertissement : %s\n", warning.c_str());

	CHECK(warnings.empty());
	CHECK(library.profiles().size() >= 10);
}

TEST(profil_t4c_couvre_les_sept_modes_du_constructeur)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	const auto *t4c = library.find("aputure-amaran-t4c");
	CHECK(t4c != nullptr);
	if (!t4c)
		return;

	CHECK_EQ(t4c->modes.size(), size_t(7));

	// Nombres de canaux repris du document Aputure, micrologiciel V1.4.
	CHECK_EQ(t4c->findMode("mode1")->channelCount(), size_t(10));
	CHECK_EQ(t4c->findMode("mode2")->channelCount(), size_t(6));
	CHECK_EQ(t4c->findMode("mode3")->channelCount(), size_t(9));
	CHECK_EQ(t4c->findMode("mode4")->channelCount(), size_t(7));
	CHECK_EQ(t4c->findMode("mode5")->channelCount(), size_t(6));
	CHECK_EQ(t4c->findMode("mode6")->channelCount(), size_t(8));
	CHECK_EQ(t4c->findMode("mode7")->channelCount(), size_t(9));

	// Le mode 3 est celui propose par defaut : c'est le seul qui offre a la
	// fois la teinte et la temperature de couleur.
	CHECK(t4c->preferredMode()->id == "mode3");
}

TEST(profil_t4c_mode_3_a_les_roles_attendus)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode = library.find("aputure-amaran-t4c")->findMode("mode3");

	CHECK_EQ(mode->findRole(ChannelRole::Dimmer), 0);
	CHECK_EQ(mode->findRole(ChannelRole::Cct), 1);
	CHECK_EQ(mode->findRole(ChannelRole::GreenMagenta), 2);
	CHECK_EQ(mode->findRole(ChannelRole::ColorMix), 3);
	CHECK_EQ(mode->findRole(ChannelRole::Hue), 4);
	CHECK_EQ(mode->findRole(ChannelRole::Saturation), 5);
	CHECK_EQ(mode->findRole(ChannelRole::Strobe), 8);

	// Les plages particulieres du constructeur doivent avoir survecu au JSON.
	CHECK_EQ(mode->channels[2].neutralValue, 132);
	CHECK_EQ(mode->channels[2].rangeMin, 21);
	CHECK_EQ(mode->channels[8].rangeMin, 20);
	CHECK_EQ(mode->channels[1].physicalMin, 2500.0f);
	CHECK_EQ(mode->channels[1].physicalMax, 7500.0f);
}

TEST(profil_t4c_expose_les_neuf_effets_embarques)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	CHECK_EQ(fx->effects.size(), size_t(9));
	CHECK_EQ(fx->findRole(ChannelRole::FxSelect), 2);

	// Chaque effet doit designer un canal de frequence qui existe.
	for (const auto &effect : fx->effects) {
		CHECK(!effect.id.empty());
		CHECK(effect.hasFrequency);
		CHECK(effect.frequencyChannel >= 0);
		CHECK(effect.frequencyChannel < static_cast<int>(fx->channelCount()));
	}

	// Les valeurs de selection doivent etre distinctes, sans quoi deux effets
	// se confondraient.
	for (size_t i = 0; i < fx->effects.size(); ++i)
		for (size_t j = i + 1; j < fx->effects.size(); ++j)
			CHECK(fx->effects[i].selectValue != fx->effects[j].selectValue);
}

TEST(aucun_profil_livre_ne_depasse_un_univers)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	for (const auto &profile : library.profiles())
		for (const auto &mode : profile.modes) {
			CHECK(mode.channelCount() >= 1);
			CHECK(mode.channelCount() <= size_t(kSlotsPerUniverse));
		}
}

TEST(chaque_profil_livre_a_un_mode_par_defaut_valide)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	for (const auto &profile : library.profiles()) {
		CHECK(profile.preferredMode() != nullptr);
		// Un default_mode qui designe un mode inexistant est une faute de
		// frappe silencieuse : on la fait remonter.
		if (!profile.defaultMode.empty())
			CHECK(profile.findMode(profile.defaultMode) != nullptr);
	}
}
