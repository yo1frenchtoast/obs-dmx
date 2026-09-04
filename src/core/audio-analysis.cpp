#include "core/audio-analysis.h"

#include <algorithm>
#include <cmath>

namespace obsdmx {

namespace {

constexpr float kPi = 3.14159265358979f;

/// Butterworth Q. Two cascaded sections give a fourth-order Linkwitz-Riley, so
/// the three bands recombine without a bump or a dip at the crossover
/// frequencies.
constexpr float kButterworthQ = 0.70710678f;

/// A music signal never reaches full scale within a single band. This gain
/// brings an ordinary mix to around half scale, so the sensitivity control
/// starts somewhere usable.
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

	// Bass: low-pass below the first crossover.
	bands_[0].sectionCount = 2;
	for (int i = 0; i < 2; ++i)
		bands_[0].sections[static_cast<size_t>(i)].setLowpass(sampleRate_, kLowCrossover, kButterworthQ);

	// Mids: between the two crossovers.
	bands_[1].sectionCount = 4;
	for (int i = 0; i < 2; ++i)
		bands_[1].sections[static_cast<size_t>(i)].setHighpass(sampleRate_, kLowCrossover, kButterworthQ);
	for (int i = 2; i < 4; ++i)
		bands_[1].sections[static_cast<size_t>(i)].setLowpass(sampleRate_, kHighCrossover, kButterworthQ);

	// Treble: high-pass above the second crossover.
	bands_[2].sectionCount = 2;
	for (int i = 0; i < 2; ++i)
		bands_[2].sections[static_cast<size_t>(i)].setHighpass(sampleRate_, kHighCrossover, kButterworthQ);

	// Instant attack, 120 ms release: the light must snap on the attack and
	// fall back gently, not follow every oscillation.
	releaseCoeff_ = std::exp(-1.0f / (sampleRate_ * 0.120f));
	// Long average for the adaptive threshold: about one second.
	averageCoeff_ = std::exp(-1.0f / (sampleRate_ * 1.0f));
	// 250 ms of guard: past 240 beats per minute we are no longer talking about
	// tempo, and it avoids counting the same hit twice.
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

	// Adaptive threshold: what matters is how far the level rises above what
	// the track usually does, not an absolute level that would depend on how
	// the desk is set.
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
