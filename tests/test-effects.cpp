#include "test-harness.h"

#include "core/effect-runner.h"
#include "core/patch.h"

#include <cmath>

using namespace obsdmx;
using Clock = EffectRunner::Clock;

namespace {

FixtureLibrary buildLibrary()
{
	FixtureLibrary library;
	std::string error;
	// Un appareil avec canal de strobe materiel, un autre sans.
	library.loadJson(R"({
		"id": "avec-strobe", "model": "Avec strobe", "default_mode": "m",
		"modes": [{"id": "m", "channels": [
			{"role":"dimmer"},{"role":"red"},{"role":"green"},{"role":"blue"},
			{"role":"strobe","range_min":20,"range_max":255,"off":0,"physical_min":1,"physical_max":25}]}]
	})", error);
	library.loadJson(R"({
		"id": "sans-strobe", "model": "Sans strobe", "default_mode": "m",
		"modes": [{"id": "m", "channels": [
			{"role":"dimmer"},{"role":"red"},{"role":"green"},{"role":"blue"}]}]
	})", error);
	return library;
}

Patch buildPatch(const FixtureLibrary &library, const std::string &profile = "avec-strobe")
{
	Patch patch(library);
	for (int i = 0; i < 4; ++i) {
		Fixture fixture;
		fixture.id = "f" + std::to_string(i);
		fixture.profileId = profile;
		fixture.modeId = "m";
		fixture.address = 1 + i * 10;
		patch.add(fixture);
	}
	return patch;
}

LightState colored(float hue, float intensity = 1.0f)
{
	LightState state;
	state.intensity = intensity;
	state.colorMix = 1.0f;
	state.hue = hue;
	state.saturation = 1.0f;
	return state;
}

Effect chaserOf(std::vector<LightState> steps, int stepMs, ChaserDirection direction = ChaserDirection::Forward)
{
	Effect effect;
	effect.id = "chaser";
	effect.type = EffectType::Chaser;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0", "f1", "f2", "f3"};
	effect.chaser.steps = std::move(steps);
	effect.chaser.stepMs = stepMs;
	effect.chaser.direction = direction;
	return effect;
}

std::unordered_map<std::string, LightState> emptyStates()
{
	return {};
}

} // namespace

TEST(fusion_htp_garde_l_etat_le_plus_lumineux)
{
	const LightState faible = colored(0.0f, 0.2f);
	const LightState fort = colored(240.0f, 0.9f);

	// On prend l'etat entier du gagnant : melanger deux teintes donnerait une
	// troisieme couleur que personne n'a demandee.
	CHECK_EQ(blend(faible, fort, BlendMode::Htp).hue, 240.0f);
	CHECK_EQ(blend(fort, faible, BlendMode::Htp).hue, 240.0f);

	// Le mode remplacement ignore les intensites.
	CHECK_EQ(blend(fort, faible, BlendMode::Replace).hue, 0.0f);
}

TEST(chaser_decale_le_motif_le_long_des_projecteurs)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	// Deux pas : allume puis eteint. Un projecteur sur deux doit etre allume.
	const auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f)}, 1000);

	const auto start = Clock::now();
	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);

	CHECK_EQ(states["f0"].intensity, 1.0f);
	CHECK_EQ(states["f1"].intensity, 0.0f);
	CHECK_EQ(states["f2"].intensity, 1.0f);
	CHECK_EQ(states["f3"].intensity, 0.0f);
}

TEST(chaser_avance_avec_le_temps)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	const auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f)}, 1000);
	const auto start = Clock::now();

	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);
	CHECK_EQ(states["f0"].intensity, 1.0f);

	// Un pas plus tard, le motif s'est deplace d'un cran.
	states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(1000), states);
	CHECK_EQ(states["f0"].intensity, 0.0f);
	CHECK_EQ(states["f1"].intensity, 1.0f);
}

TEST(chaser_en_bpm_deduit_la_duree_du_pas)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.0f)}, 1000);
	effect.chaser.useBpm = true;
	effect.chaser.bpm = 120.0f; // 120 temps par minute, soit 500 ms par pas

	const auto start = Clock::now();
	// L'effet demarre son horloge a son premier passage : il faut l'amorcer
	// a l'instant zero avant de sauter dans le temps.
	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);

	states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(500), states);

	// A 500 ms le motif a avance d'exactement un pas.
	CHECK_EQ(states["f0"].intensity, 0.0f);
	CHECK_EQ(states["f1"].intensity, 1.0f);
}

TEST(chaser_aller_retour_ne_repete_pas_les_extremites)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	const auto effect = chaserOf({colored(0.0f, 1.0f), colored(0.0f, 0.5f), colored(0.0f, 0.0f)}, 100,
				     ChaserDirection::PingPong);
	const auto start = Clock::now();

	// Sur trois pas, l'aller-retour a une periode de quatre : 0,1,2,1.
	std::vector<int> offsets;
	for (int i = 0; i < 5; ++i) {
		auto states = emptyStates();
		runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(100 * i), states);
		offsets.push_back(static_cast<int>(std::lround(states["f0"].intensity * 2.0f)));
	}
	// Intensites 1.0, 0.5, 0.0 -> 2, 1, 0 apres mise a l'echelle.
	CHECK_EQ(offsets[0], 2);
	CHECK_EQ(offsets[1], 1);
	CHECK_EQ(offsets[2], 0);
	CHECK_EQ(offsets[3], 1);
	CHECK_EQ(offsets[4], 2); // revenu au depart, sans repeter le 0
}

TEST(chaser_fond_entre_les_pas_sur_la_part_demandee)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto effect = chaserOf({colored(0.0f, 0.0f), colored(0.0f, 1.0f)}, 1000);
	effect.chaser.fadeRatio = 1.0f; // fondu permanent

	const auto start = Clock::now();
	auto states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start, states);

	// A mi-pas, f1 est a mi-chemin entre le pas 0 et le pas 1.
	states = emptyStates();
	runner.apply({effect}, patch, AudioSnapshot{}, start + std::chrono::milliseconds(500), states);
	CHECK(std::abs(states["f1"].intensity - 0.5f) < 0.05f);
}

TEST(strobe_utilise_le_canal_materiel_quand_il_existe)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "strobe";
	effect.type = EffectType::Strobe;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.strobe.hz = 15.0f;
	effect.strobe.useBaseColor = true;

	auto states = emptyStates();
	states["f0"] = colored(120.0f, 1.0f);
	runner.apply({effect}, patch, AudioSnapshot{}, Clock::now(), states);

	// A 40 Hz de rafraichissement, moduler 15 Hz par logiciel crenellerait :
	// l'appareil doit s'en charger lui-meme.
	CHECK_EQ(states["f0"].strobeHz, 15.0f);
	CHECK_EQ(states["f0"].intensity, 1.0f);
	// Et il garde la couleur du programme.
	CHECK_EQ(states["f0"].hue, 120.0f);
}

TEST(strobe_module_l_intensite_faute_de_canal_materiel)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library, "sans-strobe");
	EffectRunner runner;

	Effect effect;
	effect.id = "strobe";
	effect.type = EffectType::Strobe;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.strobe.hz = 4.0f;
	effect.strobe.dutyCycle = 0.5f;
	effect.strobe.useBaseColor = true;

	// Sur une periode complete, l'appareil doit s'allumer et s'eteindre.
	bool seenLit = false, seenDark = false;
	for (int i = 0; i < 40; ++i) {
		auto states = emptyStates();
		states["f0"] = colored(0.0f, 1.0f);
		runner.apply({effect}, patch, AudioSnapshot{},
			     Clock::now() + std::chrono::milliseconds(i * 10), states);
		CHECK_EQ(states["f0"].strobeHz, 0.0f); // pas de canal materiel a piloter
		if (states["f0"].intensity > 0.5f) seenLit = true;
		else seenDark = true;
	}
	CHECK(seenLit);
	CHECK(seenDark);
}

TEST(strobe_en_htp_ne_fait_pas_disparaitre_le_fond)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library, "sans-strobe");
	EffectRunner runner;

	Effect effect;
	effect.id = "strobe";
	effect.type = EffectType::Strobe;
	effect.blend = BlendMode::Htp;
	effect.fixtureIds = {"f0"};
	effect.strobe.hz = 4.0f;
	effect.strobe.useBaseColor = false;
	effect.strobe.color = colored(0.0f, 1.0f);

	// Le fond est a mi-intensite : entre deux eclats il doit rester visible.
	for (int i = 0; i < 40; ++i) {
		auto states = emptyStates();
		states["f0"] = colored(240.0f, 0.5f);
		runner.apply({effect}, patch, AudioSnapshot{},
			     Clock::now() + std::chrono::milliseconds(i * 10), states);
		CHECK(states["f0"].intensity >= 0.5f);
	}
}

TEST(sound_reactive_suit_le_niveau)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "sound";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.band = 0;
	effect.sound.sensitivity = 1.0f;
	effect.sound.threshold = 0.1f;

	AudioSnapshot quiet;
	quiet.bands[0] = 0.05f; // sous le seuil
	auto states = emptyStates();
	states["f0"] = colored(0.0f, 1.0f);
	runner.apply({effect}, patch, quiet, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 0.0f);

	AudioSnapshot loud;
	loud.bands[0] = 0.9f;
	states = emptyStates();
	states["f0"] = colored(0.0f, 1.0f);
	runner.apply({effect}, patch, loud, Clock::now(), states);
	CHECK(std::abs(states["f0"].intensity - 0.8f) < 0.01f);
}

TEST(sound_reactive_ne_manque_pas_un_temps_entre_deux_trames)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "sound";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::FlashOnBeat;
	effect.sound.color = colored(0.0f, 1.0f);

	AudioSnapshot audio;
	audio.beatCount = 1;

	auto states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 1.0f); // premier temps vu

	// Le meme compteur ne redeclenche pas.
	states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 0.0f);

	// Un compteur, et non un booleen : meme si plusieurs temps sont tombes
	// entre deux trames, on en voit le changement.
	audio.beatCount = 5;
	states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 1.0f);
}

TEST(effet_desactive_ou_sans_cible_ne_fait_rien)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	auto disabled = chaserOf({colored(0.0f, 1.0f)}, 100);
	disabled.enabled = false;

	auto targetless = chaserOf({colored(0.0f, 1.0f)}, 100);
	targetless.id = "vide";
	targetless.fixtureIds.clear();

	auto states = emptyStates();
	runner.apply({disabled, targetless}, patch, AudioSnapshot{}, Clock::now(), states);
	CHECK(states.empty());
}

TEST(en_htp_un_effet_ne_peut_qu_eclaircir_jamais_baisser)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Htp;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;

	// C'est la limite de fond du mode « le plus lumineux gagne » : sur un
	// programme qui allume deja a fond, l'effet ne peut rien retirer et parait
	// donc inerte. L'interface doit le dire, le moteur ne peut pas l'inventer.
	AudioSnapshot faible;
	faible.bands[0] = 0.1f;
	auto states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f);
	runner.apply({effect}, patch, faible, Clock::now(), states);
	CHECK_EQ(states["f0"].intensity, 1.0f);

	// En remplacement, le niveau passe.
	effect.blend = BlendMode::Replace;
	states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f);
	runner.apply({effect}, patch, faible, Clock::now(), states);
	CHECK(std::abs(states["f0"].intensity - 0.1f) < 0.01f);
}

TEST(le_son_garde_la_couleur_du_programme_par_defaut)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;
	effect.sound.useBaseColor = true;

	AudioSnapshot audio;
	audio.bands[0] = 0.5f;

	auto states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f); // bleu
	runner.apply({effect}, patch, audio, Clock::now(), states);

	CHECK_EQ(states["f0"].hue, 240.0f);
	CHECK(std::abs(states["f0"].intensity - 0.5f) < 0.01f);
}

TEST(le_son_peut_imposer_sa_propre_couleur)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;
	effect.sound.useBaseColor = false;
	effect.sound.color = colored(120.0f, 1.0f); // vert

	AudioSnapshot audio;
	audio.bands[0] = 0.5f;

	auto states = emptyStates();
	states["f0"] = colored(240.0f, 1.0f); // base bleue, ignoree
	runner.apply({effect}, patch, audio, Clock::now(), states);

	CHECK_EQ(states["f0"].hue, 120.0f);
	CHECK(std::abs(states["f0"].intensity - 0.5f) < 0.01f);
}

TEST(l_eclat_sur_le_temps_a_une_couleur_reglable)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::FlashOnBeat;
	effect.sound.useBaseColor = false;
	effect.sound.color = colored(60.0f, 1.0f); // jaune

	AudioSnapshot audio;
	audio.beatCount = 1;

	auto states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);
	CHECK_EQ(states["f0"].hue, 60.0f);
	CHECK_EQ(states["f0"].intensity, 1.0f);
}

TEST(un_projecteur_absent_du_programme_ne_prend_plus_une_teinte_arbitraire)
{
	const auto library = buildLibrary();
	const auto patch = buildPatch(library);
	EffectRunner runner;

	Effect effect;
	effect.id = "son";
	effect.type = EffectType::Sound;
	effect.blend = BlendMode::Replace;
	effect.fixtureIds = {"f0"};
	effect.sound.target = SoundTarget::Intensity;
	effect.sound.threshold = 0.0f;
	effect.sound.useBaseColor = false;
	effect.sound.color = colored(300.0f, 1.0f);

	AudioSnapshot audio;
	audio.bands[0] = 0.8f;

	// Aucun etat de depart : le projecteur n'est pas cite par le programme.
	auto states = emptyStates();
	runner.apply({effect}, patch, audio, Clock::now(), states);

	// Il prend la couleur choisie pour l'effet, et non celle qui trainait dans
	// l'etat « eteint ».
	CHECK_EQ(states["f0"].hue, 300.0f);
}
