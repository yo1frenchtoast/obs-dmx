#include "core/audio-analysis.h"

#include <algorithm>
#include <cmath>

namespace obsdmx {

namespace {

constexpr float kPi = 3.14159265358979f;

/// Facteur de qualite de Butterworth. Deux sections en cascade donnent un
/// Linkwitz-Riley d'ordre 4 : les trois voies se recombinent alors sans bosse
/// ni creux aux frequences de coupure.
constexpr float kButterworthQ = 0.70710678f;

/// Un signal de musique ne monte jamais a pleine echelle dans une seule bande.
/// Ce gain amene un mix ordinaire autour de la moitie de l'echelle, pour que
/// le reglage de sensibilite parte d'un endroit utilisable.
constexpr float kBandGain = 2.5f;

} // namespace

void Biquad::setLowpass(float sampleRate, float frequency, float q)
{
	const float w0 = 2.0f * kPi * frequency / sampleRate;
	const float cosw0 = std::cos(w0);
	const float alpha = std::sin(w0) / (2.0f * q);
	const float a0 = 1.0f + alpha;

	b0_ = (1.0f - cosw0) / 2.0f / a0;
	b1_ = (1.0f - cosw0) / a0;
	b2_ = b0_;
	a1_ = -2.0f * cosw0 / a0;
	a2_ = (1.0f - alpha) / a0;
	reset();
}

void Biquad::setHighpass(float sampleRate, float frequency, float q)
{
	const float w0 = 2.0f * kPi * frequency / sampleRate;
	const float cosw0 = std::cos(w0);
	const float alpha = std::sin(w0) / (2.0f * q);
	const float a0 = 1.0f + alpha;

	b0_ = (1.0f + cosw0) / 2.0f / a0;
	b1_ = -(1.0f + cosw0) / a0;
	b2_ = b0_;
	a1_ = -2.0f * cosw0 / a0;
	a2_ = (1.0f - alpha) / a0;
	reset();
}

void AudioAnalyzer::prepare(float sampleRate)
{
	sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;

	// Grave : passe-bas sous la premiere coupure.
	bands_[0].sectionCount = 2;
	for (int i = 0; i < 2; ++i)
		bands_[0].sections[static_cast<size_t>(i)].setLowpass(sampleRate_, kLowCrossover, kButterworthQ);

	// Medium : entre les deux coupures.
	bands_[1].sectionCount = 4;
	for (int i = 0; i < 2; ++i)
		bands_[1].sections[static_cast<size_t>(i)].setHighpass(sampleRate_, kLowCrossover, kButterworthQ);
	for (int i = 2; i < 4; ++i)
		bands_[1].sections[static_cast<size_t>(i)].setLowpass(sampleRate_, kHighCrossover, kButterworthQ);

	// Aigu : passe-haut au-dessus de la seconde coupure.
	bands_[2].sectionCount = 2;
	for (int i = 0; i < 2; ++i)
		bands_[2].sections[static_cast<size_t>(i)].setHighpass(sampleRate_, kHighCrossover, kButterworthQ);

	// Attaque instantanee, retombee en 120 ms : la lumiere doit claquer sur
	// l'attaque et retomber doucement, pas suivre chaque oscillation.
	releaseCoeff_ = std::exp(-1.0f / (sampleRate_ * 0.120f));
	// Moyenne longue pour le seuil adaptatif : environ une seconde.
	averageCoeff_ = std::exp(-1.0f / (sampleRate_ * 1.0f));
	// 250 ms de garde : au-dela de 240 temps par minute on ne parle plus de
	// tempo, et cela evite de compter deux fois la meme frappe.
	refractorySamples_ = static_cast<int>(sampleRate_ * 0.25f);

	reset();
}

void AudioAnalyzer::reset()
{
	for (int i = 0; i < kBandCount; ++i) {
		bands_[i].reset();
		envelopes_[i] = 0.0f;
		published_[i].store(0.0f, std::memory_order_relaxed);
	}
	energyAverage_ = 0.0f;
	refractory_ = 0;
}

void AudioAnalyzer::process(const float *samples, size_t count)
{
	if (!samples || count == 0)
		return;

	for (size_t n = 0; n < count; ++n) {
		const float input = samples[n];

		for (int band = 0; band < kBandCount; ++band) {
			const float level = std::fabs(bands_[band].process(input)) * kBandGain;
			envelopes_[band] = level > envelopes_[band] ? level : envelopes_[band] * releaseCoeff_;
		}

		updateBeat(envelopes_[0]);
	}

	for (int band = 0; band < kBandCount; ++band)
		published_[band].store(std::min(envelopes_[band], 1.0f), std::memory_order_relaxed);
}

void AudioAnalyzer::updateBeat(float lowEnergy)
{
	if (refractory_ > 0)
		--refractory_;

	// Seuil adaptatif : ce qui compte est le depassement par rapport a ce que
	// le morceau fait d'habitude, et non un niveau absolu qui dependrait du
	// reglage de la console.
	const bool loudEnough = lowEnergy > 0.05f;
	if (refractory_ == 0 && loudEnough && lowEnergy > energyAverage_ * beatFactor_) {
		beatCount_.fetch_add(1, std::memory_order_relaxed);
		refractory_ = refractorySamples_;
	}

	energyAverage_ = energyAverage_ * averageCoeff_ + lowEnergy * (1.0f - averageCoeff_);
}

AudioSnapshot AudioAnalyzer::snapshot() const
{
	AudioSnapshot snap;
	for (int band = 0; band < kBandCount; ++band)
		snap.bands[band] = published_[band].load(std::memory_order_relaxed);
	snap.beatCount = beatCount_.load(std::memory_order_relaxed);
	return snap;
}

} // namespace obsdmx
