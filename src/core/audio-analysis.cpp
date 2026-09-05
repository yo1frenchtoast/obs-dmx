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
	// 110 ms of guard, so a kick every 120 ms still registers -- roughly 500
	// beats per minute, well past any dance floor. The old quarter-second
	// capped detection at 240 BPM, which fast techno reaches. A shorter guard
	// is safe now that the detector reads rises rather than levels: a decaying
	// kick produces no rise and cannot trigger twice.
	refractorySamples_ = static_cast<int>(sampleRate_ * 0.110f);

	reset();
}

void AudioAnalyzer::reset()
{
	for (int i = 0; i < kBandCount; ++i) {
		bands_[i].reset();
		envelopes_[i] = 0.0f;
		published_[i].store(0.0f, std::memory_order_relaxed);
	}
	blockSum_ = 0.0f;
	blockFill_ = 0;
	previousBlockLevel_ = 0.0f;
	previousFlux_ = 0.0f;
	for (float &value : fluxHistory_)
		value = 0.0f;
	fluxCursor_ = 0;
	fluxFilled_ = 0;
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

		// The bass envelope feeds the detector. A shorter one was tried, on the
		// theory that the 120 ms release would smear close kicks together; it
		// measured no better and slightly worse at extreme tempos, because the
		// onset function is a difference and both envelopes rise just as fast.
		updateBeat(envelopes_[0]);
	}

	for (int band = 0; band < kBandCount; ++band)
		published_[band].store(std::min(envelopes_[band], 1.0f), std::memory_order_relaxed);
}

void AudioAnalyzer::updateBeat(float lowLevel)
{
	if (refractory_ > 0)
		--refractory_;

	// Gather a short block before deciding anything: a single sample says
	// nothing about an onset, and blocks keep the cost per sample trivial.
	blockSum_ += lowLevel;
	if (++blockFill_ < kBlockSize)
		return;

	const float level = blockSum_ / static_cast<float>(kBlockSize);
	blockSum_ = 0.0f;
	blockFill_ = 0;

	// The onset function: how much the bass rose since the previous block.
	// Only rises count -- a decay is not an attack.
	const float flux = std::max(0.0f, level - previousBlockLevel_);
	previousBlockLevel_ = level;

	// What counts as a large rise depends on the track. A mean alone is not
	// enough: a noisy master has a high mean *and* a wide spread, and comparing
	// against the mean lets every noise burst through. The spread has to enter
	// the threshold, so a busy track demands a correspondingly bigger rise.
	float sum = 0.0f;
	float sumOfSquares = 0.0f;
	for (int i = 0; i < fluxFilled_; ++i) {
		sum += fluxHistory_[i];
		sumOfSquares += fluxHistory_[i] * fluxHistory_[i];
	}
	const float mean = fluxFilled_ > 0 ? sum / static_cast<float>(fluxFilled_) : 0.0f;
	const float variance =
		fluxFilled_ > 0 ? std::max(0.0f, sumOfSquares / static_cast<float>(fluxFilled_) - mean * mean)
				: 0.0f;
	const float deviation = std::sqrt(variance);

	fluxHistory_[fluxCursor_] = flux;
	fluxCursor_ = (fluxCursor_ + 1) % kFluxHistory;
	fluxFilled_ = std::min(fluxFilled_ + 1, kFluxHistory);

	// The floor has to scale with the signal. An absolute one lets a loud
	// sustained note through: its envelope still ripples slightly, and on a
	// strong signal that ripple alone clears any fixed number. Demanding a rise
	// worth a few percent of the current level rejects that ripple while
	// leaving a kick, whose rise is a large fraction of its own level, well
	// clear.
	const float floor = std::max(0.0015f, level * 0.05f);
	const float threshold = mean + beatFactor_ * deviation + floor;

	// Only a peak counts. Without this the same attack fires on each block of
	// its rising edge, which reads as a double beat.
	const bool isPeak = flux > previousFlux_;
	const float wasFlux = previousFlux_;
	previousFlux_ = flux;

	if (refractory_ == 0 && fluxFilled_ >= kFluxHistory / 4 && isPeak && wasFlux >= 0.0f &&
	    flux > threshold) {
		beatCount_.fetch_add(1, std::memory_order_relaxed);
		refractory_ = refractorySamples_;
	}
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
