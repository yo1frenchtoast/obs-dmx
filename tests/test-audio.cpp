#include "test-harness.h"

#include "core/audio-analysis.h"

#include <cmath>
#include <vector>

using namespace obsdmx;

namespace {

constexpr float kSampleRate = 48000.0f;

std::vector<float> sine(float frequency, float seconds, float amplitude = 1.0f)
{
	const size_t count = static_cast<size_t>(kSampleRate * seconds);
	std::vector<float> samples(count);
	for (size_t n = 0; n < count; ++n)
		samples[n] = amplitude * std::sin(2.0f * 3.14159265f * frequency * float(n) / kSampleRate);
	return samples;
}

AudioSnapshot analyze(const std::vector<float> &samples)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);
	analyzer.process(samples.data(), samples.size());
	return analyzer.snapshot();
}

} // namespace

TEST(le_silence_ne_produit_aucun_niveau)
{
	const std::vector<float> silence(4800, 0.0f);
	const auto snap = analyze(silence);

	for (float band : snap.bands)
		CHECK_EQ(band, 0.0f);
	CHECK_EQ(snap.beatCount, uint64_t(0));
}

TEST(chaque_bande_repond_a_sa_plage_de_frequences)
{
	const auto grave = analyze(sine(80.0f, 0.3f));
	CHECK(grave.bands[0] > 0.3f);
	// Une basse ne doit quasiment rien mettre dans les aigus.
	CHECK(grave.bands[2] < grave.bands[0] * 0.05f);

	const auto aigu = analyze(sine(5000.0f, 0.3f));
	CHECK(aigu.bands[2] > 0.3f);
	CHECK(aigu.bands[0] < aigu.bands[2] * 0.05f);

	const auto medium = analyze(sine(700.0f, 0.3f));
	CHECK(medium.bands[1] > medium.bands[0] * 10.0f);
	CHECK(medium.bands[1] > medium.bands[2] * 10.0f);
}

TEST(les_trois_voies_couvrent_le_spectre_sans_trou)
{
	// Trois passe-bande etroits laisseraient des creux entre eux : un morceau
	// dont l'essentiel y tombe n'allumerait rien. Une separation en voies,
	// elle, se recombine a l'unite partout.
	for (float frequency : {50.0f, 150.0f, 300.0f, 600.0f, 1200.0f, 3000.0f, 8000.0f}) {
		const auto snap = analyze(sine(frequency, 0.3f));
		const float somme = snap.bands[0] + snap.bands[1] + snap.bands[2];
		CHECK(somme > 0.5f);
	}
}

TEST(les_coupures_partagent_l_energie_entre_deux_voies)
{
	// A la frequence de coupure, les deux voies voisines doivent se partager
	// le signal a peu pres a egalite.
	const auto basse = analyze(sine(AudioAnalyzer::kLowCrossover, 0.3f));
	CHECK(basse.bands[0] > 0.1f);
	CHECK(basse.bands[1] > 0.1f);
	CHECK(std::abs(basse.bands[0] - basse.bands[1]) < 0.25f);

	const auto haute = analyze(sine(AudioAnalyzer::kHighCrossover, 0.3f));
	CHECK(haute.bands[1] > 0.1f);
	CHECK(haute.bands[2] > 0.1f);
	CHECK(std::abs(haute.bands[1] - haute.bands[2]) < 0.25f);
}

TEST(les_niveaux_restent_bornes_a_un)
{
	// Un signal sature ne doit pas faire deborder l'echelle.
	const auto snap = analyze(sine(80.0f, 0.3f, 4.0f));
	for (float band : snap.bands)
		CHECK(band <= 1.0f);
}

TEST(l_enveloppe_retombe_apres_le_son)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	const auto tone = sine(80.0f, 0.3f);
	analyzer.process(tone.data(), tone.size());
	const float pendant = analyzer.snapshot().bands[0];
	CHECK(pendant > 0.3f);

	// Une demi-seconde de silence : la retombee est de 120 ms, il ne doit
	// rester presque rien.
	const std::vector<float> silence(static_cast<size_t>(kSampleRate * 0.5f), 0.0f);
	analyzer.process(silence.data(), silence.size());
	CHECK(analyzer.snapshot().bands[0] < pendant * 0.05f);
}

TEST(les_temps_sont_comptes_sur_une_pulsation_reguliere)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	// Huit frappes de grosse caisse a 120 temps par minute, soit une toutes
	// les 500 ms.
	const size_t periode = static_cast<size_t>(kSampleRate * 0.5f);
	const size_t frappe = static_cast<size_t>(kSampleRate * 0.05f);

	std::vector<float> piste(periode * 8, 0.0f);
	for (int beat = 0; beat < 8; ++beat)
		for (size_t n = 0; n < frappe; ++n) {
			const float enveloppe = 1.0f - float(n) / float(frappe);
			piste[beat * periode + n] =
				enveloppe * std::sin(2.0f * 3.14159265f * 60.0f * float(n) / kSampleRate);
		}

	analyzer.process(piste.data(), piste.size());
	const uint64_t temps = analyzer.snapshot().beatCount;

	// On veut le bon ordre de grandeur, pas le compte exact : une detection
	// par energie manque parfois la premiere frappe, le temps que la moyenne
	// glissante s'etablisse.
	CHECK(temps >= 6);
	CHECK(temps <= 9);
}

TEST(la_periode_refractaire_empeche_de_compter_deux_fois_la_meme_frappe)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	// Une seule frappe, longue : sans garde, chaque echantillon au-dessus du
	// seuil compterait pour un temps.
	std::vector<float> piste(static_cast<size_t>(kSampleRate * 0.2f));
	for (size_t n = 0; n < piste.size(); ++n)
		piste[n] = std::sin(2.0f * 3.14159265f * 60.0f * float(n) / kSampleRate);

	analyzer.process(piste.data(), piste.size());
	CHECK(analyzer.snapshot().beatCount <= 1);
}

TEST(un_niveau_continu_ne_bat_pas)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	// Un bourdon constant : le seuil adaptatif doit le rejoindre et cesser de
	// declencher, sinon la lumiere clignoterait sur une nappe.
	const auto bourdon = sine(80.0f, 3.0f);
	analyzer.process(bourdon.data(), bourdon.size());
	const uint64_t apresTroisSecondes = analyzer.snapshot().beatCount;

	analyzer.process(bourdon.data(), bourdon.size());
	// Les trois secondes suivantes ne doivent presque rien ajouter.
	CHECK(analyzer.snapshot().beatCount - apresTroisSecondes <= 1);
}

TEST(un_appel_vide_est_sans_effet)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);
	analyzer.process(nullptr, 100);
	analyzer.process(nullptr, 0);
	CHECK_EQ(analyzer.snapshot().beatCount, uint64_t(0));
}
