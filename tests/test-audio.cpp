#include "test-harness.h"

#include "core/audio-analysis.h"

#include <cmath>
#include <cstdio>
#include <random>
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

TEST(silence_produces_no_level)
{
	const std::vector<float> silence(4800, 0.0f);
	const auto snap = analyze(silence);

	for (float band : snap.bands)
		CHECK_EQ(band, 0.0f);
	CHECK_EQ(snap.beatCount, uint64_t(0));
}

TEST(each_band_answers_its_own_frequency_range)
{
	const auto grave = analyze(sine(80.0f, 0.3f));
	CHECK(grave.bands[0] > 0.3f);
	// A bass tone must put almost nothing into the treble band.
	CHECK(grave.bands[2] < grave.bands[0] * 0.05f);

	const auto aigu = analyze(sine(5000.0f, 0.3f));
	CHECK(aigu.bands[2] > 0.3f);
	CHECK(aigu.bands[0] < aigu.bands[2] * 0.05f);

	const auto medium = analyze(sine(700.0f, 0.3f));
	CHECK(medium.bands[1] > medium.bands[0] * 10.0f);
	CHECK(medium.bands[1] > medium.bands[2] * 10.0f);
}

TEST(the_three_bands_cover_the_spectrum_without_gaps)
{
	// Three narrow band-pass filters would leave dips between them, and a track
	// whose energy fell in one would light nothing. A crossover split, by
	// contrast, recombines to unity everywhere.
	for (float frequency : {50.0f, 150.0f, 300.0f, 600.0f, 1200.0f, 3000.0f, 8000.0f}) {
		const auto snap = analyze(sine(frequency, 0.3f));
		const float somme = snap.bands[0] + snap.bands[1] + snap.bands[2];
		CHECK(somme > 0.5f);
	}
}

TEST(crossovers_split_the_energy_between_two_bands)
{
	// At the crossover frequency the two neighbouring bands should share the
	// signal roughly equally.
	const auto basse = analyze(sine(AudioAnalyzer::kLowCrossover, 0.3f));
	CHECK(basse.bands[0] > 0.1f);
	CHECK(basse.bands[1] > 0.1f);
	CHECK(std::abs(basse.bands[0] - basse.bands[1]) < 0.25f);

	const auto haute = analyze(sine(AudioAnalyzer::kHighCrossover, 0.3f));
	CHECK(haute.bands[1] > 0.1f);
	CHECK(haute.bands[2] > 0.1f);
	CHECK(std::abs(haute.bands[1] - haute.bands[2]) < 0.25f);
}

TEST(levels_stay_bounded_at_one)
{
	// A clipped signal must not push the scale past its limit.
	const auto snap = analyze(sine(80.0f, 0.3f, 4.0f));
	for (float band : snap.bands)
		CHECK(band <= 1.0f);
}

TEST(the_envelope_falls_back_after_the_sound)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	const auto tone = sine(80.0f, 0.3f);
	analyzer.process(tone.data(), tone.size());
	const float pendant = analyzer.snapshot().bands[0];
	CHECK(pendant > 0.3f);

	// Half a second of silence: the release is 120 ms, so almost nothing should
	// be left.
	const std::vector<float> silence(static_cast<size_t>(kSampleRate * 0.5f), 0.0f);
	analyzer.process(silence.data(), silence.size());
	CHECK(analyzer.snapshot().bands[0] < pendant * 0.05f);
}

TEST(beats_are_counted_on_a_steady_pulse)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	// Eight kick drum hits at 120 beats per minute, one every 500 ms.
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

	// We want the right order of magnitude, not an exact count: energy-based
	// detection sometimes misses the first hit while the running average
	// settles.
	CHECK(temps >= 6);
	CHECK(temps <= 9);
}

TEST(the_refractory_period_stops_one_hit_counting_twice)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	// A single, long hit: without a guard, every sample above the threshold
	// would count as a beat.
	std::vector<float> piste(static_cast<size_t>(kSampleRate * 0.2f));
	for (size_t n = 0; n < piste.size(); ++n)
		piste[n] = std::sin(2.0f * 3.14159265f * 60.0f * float(n) / kSampleRate);

	analyzer.process(piste.data(), piste.size());
	CHECK(analyzer.snapshot().beatCount <= 1);
}

TEST(a_steady_level_produces_no_beat)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);

	// A constant drone: the adaptive threshold must catch up and stop firing,
	// otherwise the light would flicker on a sustained pad.
	const auto bourdon = sine(80.0f, 3.0f);
	analyzer.process(bourdon.data(), bourdon.size());
	const uint64_t apresTroisSecondes = analyzer.snapshot().beatCount;

	analyzer.process(bourdon.data(), bourdon.size());
	// The next three seconds should add next to nothing.
	CHECK(analyzer.snapshot().beatCount - apresTroisSecondes <= 1);
}

TEST(an_empty_call_does_nothing)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);
	analyzer.process(nullptr, 100);
	analyzer.process(nullptr, 0);
	CHECK_EQ(analyzer.snapshot().beatCount, uint64_t(0));
}

namespace {

/// A synthetic dance track: a kick at the given tempo over a sustained bass note
/// and noise, then limited the way a club master is.
///
/// The limiting is the point. It is what flattens the attacks, and it is why a
/// detector that compares the bass *level* against its average finds almost
/// nothing on this material.
std::vector<float> danceTrack(float bpm, float sustain, float noise, float ceiling, float seconds)
{
	std::mt19937 rng(1234);
	std::normal_distribution<float> gauss(0.0f, 1.0f);

	const size_t count = static_cast<size_t>(kSampleRate * seconds);
	const size_t period = static_cast<size_t>(kSampleRate * 60.0f / bpm);

	std::vector<float> samples(count);
	for (size_t n = 0; n < count; ++n) {
		const float intoBeat = static_cast<float>(n % period) / kSampleRate;
		const float kick = std::exp(-intoBeat * 28.0f) *
				   std::sin(2.0f * 3.14159265f * 55.0f * intoBeat);
		const float bass = sustain * std::sin(2.0f * 3.14159265f * 82.0f * float(n) / kSampleRate);
		samples[n] = std::tanh((kick + bass + noise * gauss(rng)) / ceiling) * ceiling;
	}
	return samples;
}

uint64_t beatsIn(const std::vector<float> &samples)
{
	AudioAnalyzer analyzer;
	analyzer.prepare(kSampleRate);
	// Fed in blocks, the way OBS delivers audio.
	for (size_t i = 0; i < samples.size(); i += 480)
		analyzer.process(samples.data() + i, std::min<size_t>(480, samples.size() - i));
	return analyzer.snapshot().beatCount;
}

} // namespace

TEST(beats_are_found_in_a_dense_compressed_master)
{
	// The case that sent this detector back to the drawing board: hard techno,
	// heavily limited, with a sustained bass and plenty of noise. Comparing the
	// bass level against its running average found seven beats out of
	// twenty-five here.
	const float seconds = 10.0f;
	const auto track = danceTrack(155.0f, 0.60f, 0.30f, 0.6f, seconds);

	const int expected = static_cast<int>(155.0f * seconds / 60.0f);
	const int found = static_cast<int>(beatsIn(track));

	CHECK(std::abs(found - expected) <= expected / 5);
}

TEST(beats_are_found_across_the_tempo_range)
{
	// Fast tempos were the other complaint. The guard between beats used to cap
	// detection at 240 per minute, and the display envelope's long release
	// smeared close kicks into one.
	const float seconds = 10.0f;
	for (float bpm : {124.0f, 150.0f, 174.0f, 190.0f, 220.0f}) {
		const auto track = danceTrack(bpm, 0.45f, 0.20f, 0.8f, seconds);
		const int expected = static_cast<int>(bpm * seconds / 60.0f);
		const int found = static_cast<int>(beatsIn(track));

		if (std::abs(found - expected) > expected / 5)
			std::fprintf(stderr, "    %.0f BPM: expected about %d, found %d\n", bpm, expected,
				     found);
		CHECK(std::abs(found - expected) <= expected / 5);
	}
}
