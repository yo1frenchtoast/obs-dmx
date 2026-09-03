#pragma once

#include "core/effect.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace obsdmx {

/// Filtre biquad du second ordre, en forme directe I transposee.
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

/// Analyse le son pour en tirer trois enveloppes de bande et une detection de
/// temps.
///
/// La separation est faite comme dans un filtre d'enceinte : un grave sous
/// 200 Hz, un aigu au-dessus de 2 kHz, un medium entre les deux. Trois
/// passe-bande etroits auraient laisse des trous dans le spectre, et un
/// morceau dont l'essentiel tombe dans un trou n'aurait rien allume.
///
/// Un banc de filtres plutot qu'une transformee de Fourier : trois bandes
/// suffisent pour de l'eclairage, le cout est negligeable, il n'y a pas de
/// fenetrage a gerer et la latence est nulle.
///
/// process() est appele depuis le thread audio, qui est temps reel : il
/// n'alloue pas, ne prend aucun verrou et n'ecrit que des atomiques.
/// snapshot() est appele depuis le thread de rendu.
class AudioAnalyzer {
public:
	static constexpr int kBandCount = 3;
	/// Frequences de coupure entre les trois voies.
	static constexpr float kLowCrossover = 200.0f;
	static constexpr float kHighCrossover = 2000.0f;

	AudioAnalyzer() { prepare(48000.0f); }

	void prepare(float sampleRate);

	/// samples : un seul canal, en virgule flottante.
	void process(const float *samples, size_t count);

	AudioSnapshot snapshot() const;

	/// Depassement exige par rapport a l'energie habituelle pour qu'un temps
	/// soit annonce.
	void setBeatSensitivity(float factor) { beatFactor_ = factor; }

	void reset();

private:
	/// Une voie : quelques sections en cascade.
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

	void updateBeat(float lowEnergy);

	float sampleRate_ = 48000.0f;

	Band bands_[kBandCount];
	float envelopes_[kBandCount] = {0.0f, 0.0f, 0.0f};
	float releaseCoeff_ = 0.999f;

	// --- detection de temps ---
	float energyAverage_ = 0.0f;
	float averageCoeff_ = 0.9995f;
	float beatFactor_ = 1.6f;
	int refractory_ = 0;
	int refractorySamples_ = 0;

	// Partages avec le thread de rendu.
	std::atomic<float> published_[kBandCount] = {};
	std::atomic<uint64_t> beatCount_{0};
};

} // namespace obsdmx
