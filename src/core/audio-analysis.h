#pragma once

#include "core/effect.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace obsdmx {

/// Second-order biquad filter, transposed direct form I.
class Biquad {
public:
	void setLowpass(float sampleRate, float frequency, float q);
	void setHighpass(float sampleRate, float frequency, float q);

	float process(float input)
	{
		const float output = b0_ * input + z1_;
		z1_ = b1_ * input - a1_ * output + z2_;
		z2_ = b2_ * input - a2_ * output;
		return output;
	}

	void reset()
	{
		z1_ = 0.0f;
		z2_ = 0.0f;
	}

private:
	float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
	float a1_ = 0.0f, a2_ = 0.0f;
	float z1_ = 0.0f, z2_ = 0.0f;
};

/// Analyses the sound into three band envelopes and a beat detector.
///
/// The split is made the way a loudspeaker crossover does it: bass below 200 Hz,
/// treble above 2 kHz, mids in between. Three narrow band-pass filters would
/// have left gaps in the spectrum, and a track whose energy fell in a gap would
/// have lit nothing.
///
/// A filter bank rather than a Fourier transform: three bands are enough for
/// lighting, the cost is negligible, there is no windowing to get right and the
/// latency is nil.
///
/// process() is called from the audio thread, which is real time: it allocates
/// nothing, takes no lock and writes only atomics. snapshot() is called from the
/// render thread.
class AudioAnalyzer {
public:
	static constexpr int kBandCount = 3;
	/// Crossover frequencies between the three bands.
	static constexpr float kLowCrossover = 200.0f;
	static constexpr float kHighCrossover = 2000.0f;

	AudioAnalyzer() { prepare(48000.0f); }

	void prepare(float sampleRate);

	/// samples: a single channel, in floating point.
	void process(const float *samples, size_t count);

	AudioSnapshot snapshot() const;

	/// How far above the recent spread a rise in bass must go for a beat to be
	/// announced. Lower hears more; higher is stricter.
	///
	/// Written from the interface while the audio thread reads it, hence the
	/// atomic. Relaxed ordering is enough: nothing else depends on when the new
	/// value lands, only that no torn read is possible.
	void setBeatSensitivity(float factor) { beatFactor_.store(factor, std::memory_order_relaxed); }
	float beatSensitivity() const { return beatFactor_.load(std::memory_order_relaxed); }

	/// The default, and what the interface calls the middle of its range.
	static constexpr float kDefaultBeatSensitivity = 1.5f;

	void reset();

private:
	/// One band: a few cascaded sections.
	struct Band {
		std::array<Biquad, 4> sections;
		int sectionCount = 0;

		float process(float input)
		{
			for (int i = 0; i < sectionCount; ++i)
				input = sections[static_cast<size_t>(i)].process(input);
			return input;
		}

		void reset()
		{
			for (auto &section : sections)
				section.reset();
		}
	};

	void updateBeat(float lowLevel);

	float sampleRate_ = 48000.0f;

	Band bands_[kBandCount];
	float envelopes_[kBandCount] = {0.0f, 0.0f, 0.0f};
	float releaseCoeff_ = 0.999f;

	// --- beat detection ---
	//
	// Onsets are found from the *rise* in bass energy, not from its level. A
	// club master is limited hard enough that the level barely moves between
	// kicks, so a level-against-average test finds almost nothing; the rise at
	// each kick survives that limiting.
	static constexpr int kBlockSize = 512;   ///< about 11 ms at 48 kHz
	static constexpr int kFluxHistory = 64;  ///< about 0.7 s of recent rises

	float blockSum_ = 0.0f;
	int blockFill_ = 0;
	float previousBlockLevel_ = 0.0f;
	float previousFlux_ = 0.0f;

	/// Recent rises, kept to work out what counts as unusual right now.
	float fluxHistory_[kFluxHistory] = {};
	int fluxCursor_ = 0;
	int fluxFilled_ = 0;

	std::atomic<float> beatFactor_{kDefaultBeatSensitivity};
	int refractory_ = 0;
	int refractorySamples_ = 0;

	// Shared with the render thread.
	std::atomic<float> published_[kBandCount] = {};
	std::atomic<uint64_t> beatCount_{0};
};

} // namespace obsdmx
