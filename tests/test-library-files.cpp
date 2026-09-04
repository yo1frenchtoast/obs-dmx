#include "test-harness.h"

#include "core/effect-runner.h"
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

TEST(effets_embarques_du_t4c_produisent_les_bons_canaux)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	BuiltinFxSettings settings;
	settings.effectId = "lightning";
	settings.frequency = 3;

	const auto channels = builtinFxChannels(*fx, settings);

	// Canal 3 du mode FX : choix de l'effet. L'orage occupe 10 a 19.
	bool foundSelect = false, foundFrequency = false, foundControl = false;
	for (const auto &[index, value] : channels) {
		if (index == 2) { foundSelect = true; CHECK_EQ(value, 15); }
		if (index == 1) { foundControl = true; CHECK(value < 10); } // en boucle, pas a l'arret
		// La frequence 3 tombe dans la tranche 20-29.
		if (index == 5) { foundFrequency = true; CHECK(value >= 20 && value <= 29); }
	}
	CHECK(foundSelect);
	CHECK(foundControl);
	CHECK(foundFrequency);
}

TEST(frequence_aleatoire_n_est_offerte_que_par_les_effets_qui_l_acceptent)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	// L'orage accepte l'aleatoire : il occupe la tranche 100-109.
	BuiltinFxSettings orage;
	orage.effectId = "lightning";
	orage.frequency = 0;
	for (const auto &[index, value] : builtinFxChannels(*fx, orage))
		if (index == 5)
			CHECK(value >= 100 && value <= 109);

	// Le gyrophare ne l'accepte pas : on retombe sur une frequence valide
	// plutot que d'ecrire une valeur reservee.
	BuiltinFxSettings gyrophare;
	gyrophare.effectId = "cop_car";
	gyrophare.frequency = 0;
	for (const auto &[index, value] : builtinFxChannels(*fx, gyrophare))
		if (index == 4)
			CHECK(value < 100);
}

TEST(un_effet_embarque_inconnu_ne_produit_aucun_canal)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	BuiltinFxSettings settings;
	settings.effectId = "effet-inexistant";
	CHECK(builtinFxChannels(*fx, settings).empty());

	// Et un mode sans effets non plus, meme avec un identifiant valide.
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");
	settings.effectId = "lightning";
	CHECK(builtinFxChannels(*mode3, settings).empty());
}

TEST(saisie_manuelle_force_les_canaux_demandes)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	// Le mode 3 ne declare aucun effet : c'est justement le cas ou la saisie
	// manuelle sert.
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");

	BuiltinFxSettings settings;
	settings.useManual = true;
	settings.manual = {{1, 255}, {5, 128}, {9, 42}};

	const auto channels = builtinFxChannels(*mode3, settings);
	CHECK_EQ(channels.size(), size_t(3));

	// Les numeros du constructeur commencent a 1, les indices internes a 0.
	CHECK_EQ(channels[0].first, 0);
	CHECK_EQ(channels[0].second, 255);
	CHECK_EQ(channels[1].first, 4);
	CHECK_EQ(channels[1].second, 128);
	CHECK_EQ(channels[2].first, 8);
	CHECK_EQ(channels[2].second, 42);
}

TEST(saisie_manuelle_refuse_de_deborder_sur_le_projecteur_voisin)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");
	CHECK_EQ(mode3->channelCount(), size_t(9));

	BuiltinFxSettings settings;
	settings.useManual = true;
	// 10 depasse les 9 canaux de l'appareil : l'ecrire piloterait son voisin.
	settings.manual = {{9, 10}, {10, 20}, {100, 30}, {0, 40}, {-1, 50}};

	const auto channels = builtinFxChannels(*mode3, settings);
	CHECK_EQ(channels.size(), size_t(1));
	CHECK_EQ(channels[0].first, 8);
	CHECK_EQ(channels[0].second, 10);
}

TEST(saisie_manuelle_ignore_la_bibliotheque_d_effets)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *fx = library.find("aputure-amaran-t4c")->findMode("mode7");

	// Meme sur un mode qui connait des effets, la saisie manuelle prend la
	// main : c'est le point de sortie quand la bibliotheque se trompe.
	BuiltinFxSettings settings;
	settings.useManual = true;
	settings.effectId = "lightning";
	settings.manual = {{3, 77}};

	const auto channels = builtinFxChannels(*fx, settings);
	CHECK_EQ(channels.size(), size_t(1));
	CHECK_EQ(channels[0].first, 2);
	CHECK_EQ(channels[0].second, 77);
}

TEST(saisie_manuelle_vide_n_ecrit_rien)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode3 = library.find("aputure-amaran-t4c")->findMode("mode3");

	BuiltinFxSettings settings;
	settings.useManual = true;
	CHECK(builtinFxChannels(*mode3, settings).empty());
}

TEST(profil_wild_wash_rgb_reprend_les_modes_de_la_notice)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	const auto *ww = library.find("stairville-wild-wash-rgb");
	CHECK(ww != nullptr);
	if (!ww)
		return;

	// Huit modes, tels que listes dans le menu de l'appareil.
	CHECK_EQ(ww->modes.size(), size_t(8));
	CHECK_EQ(ww->findMode("1ch")->channelCount(), size_t(1));
	CHECK_EQ(ww->findMode("2ch1")->channelCount(), size_t(2));
	CHECK_EQ(ww->findMode("2ch2")->channelCount(), size_t(2));
	CHECK_EQ(ww->findMode("3ch1")->channelCount(), size_t(3));
	CHECK_EQ(ww->findMode("3ch2")->channelCount(), size_t(3));
	CHECK_EQ(ww->findMode("3ch3")->channelCount(), size_t(3));
	CHECK_EQ(ww->findMode("4ch")->channelCount(), size_t(4));
	CHECK_EQ(ww->findMode("6ch")->channelCount(), size_t(6));

	// Le 6Ch est le seul qui offre a la fois gradateur, strobe et RVB.
	CHECK(ww->preferredMode()->id == "6ch");
}

TEST(wild_wash_6ch_pilote_bien_le_rouge_le_vert_et_le_bleu)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *mode = library.find("stairville-wild-wash-rgb")->findMode("6ch");

	LightState state;
	state.intensity = 1.0f;
	state.colorMix = 1.0f;
	state.hue = 120.0f; // vert
	state.saturation = 1.0f;

	const auto values = renderState(*mode, state);
	CHECK_EQ(values.size(), size_t(6));
	CHECK_EQ(values[0], 255); // intensite
	CHECK_EQ(values[2], 0);   // rouge
	CHECK_EQ(values[3], 255); // vert
	CHECK_EQ(values[4], 0);   // bleu
	CHECK_EQ(values[5], 0);   // commande sonore de l'appareil : laissee coupee
}

TEST(wild_wash_le_strobe_ne_commence_pas_au_meme_endroit_selon_le_mode)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *ww = library.find("stairville-wild-wash-rgb");

	LightState state;
	state.intensity = 1.0f;

	// Sans strobe, les deux modes laissent les diodes allumees (0-5), et non
	// dans la zone de noir (6-10).
	CHECK_EQ(renderState(*ww->findMode("6ch"), state)[1], 0);
	CHECK_EQ(renderState(*ww->findMode("3ch2"), state)[1], 0);

	state.strobeHz = 0.1f;

	// En 3Ch2 la plage de strobe demarre a 11.
	const int simple = renderState(*ww->findMode("3ch2"), state)[1];
	CHECK(simple >= 11 && simple <= 14);

	// En 6Ch, les valeurs 11 a 127 sont prises par les effets aleatoires de
	// l'appareil : le strobe regulier ne commence qu'a 128. Confondre les deux
	// declencherait un effet aleatoire au lieu d'un strobe.
	const int etendu = renderState(*ww->findMode("6ch"), state)[1];
	CHECK(etendu >= 128 && etendu <= 131);

	// A pleine vitesse, les deux plafonnent a 250, pas a 255 : au-dela
	// l'appareil repasse en eclairage fixe.
	state.strobeHz = 30.0f;
	CHECK_EQ(renderState(*ww->findMode("3ch2"), state)[1], 250);
	CHECK_EQ(renderState(*ww->findMode("6ch"), state)[1], 250);
}

TEST(wild_wash_la_macro_de_couleurs_par_defaut_ne_laisse_pas_l_appareil_noir)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);
	const auto *ww = library.find("stairville-wild-wash-rgb");

	LightState state;
	state.intensity = 1.0f;

	// Le canal de macro n'est pas pilote par une intention lumineuse. A zero
	// il vaut « noir » : le projecteur resterait eteint quoi qu'on fasse du
	// gradateur. Le profil le laisse donc sur le blanc.
	for (const char *modeId : {"2ch1", "3ch2", "4ch"}) {
		const auto *mode = ww->findMode(modeId);
		const int macro = mode->findRole(ChannelRole::ColorWheel);
		CHECK(macro >= 0);
		if (macro >= 0) {
			const int value = renderState(*mode, state)[static_cast<size_t>(macro)];
			CHECK(value >= 110 && value <= 117); // plage « White » de la notice
		}
	}
}

TEST(profil_wild_wash_blanc_n_expose_ni_couleur_ni_temperature)
{
	std::vector<std::string> warnings;
	const auto library = loadShipped(warnings);

	const auto *ww = library.find("stairville-wild-wash-132-white");
	CHECK(ww != nullptr);
	if (!ww)
		return;

	CHECK_EQ(ww->modes.size(), size_t(3));
	for (const auto &mode : ww->modes) {
		// L'appareil est en blanc froid fixe : rien a piloter de ce cote.
		CHECK(!mode.hasRole(ChannelRole::Red));
		CHECK(!mode.hasRole(ChannelRole::Hue));
		CHECK(!mode.hasRole(ChannelRole::Cct));
	}

	CHECK(ww->findMode("2ch")->hasRole(ChannelRole::Dimmer));
	CHECK(ww->findMode("2ch")->hasRole(ChannelRole::Strobe));
}
