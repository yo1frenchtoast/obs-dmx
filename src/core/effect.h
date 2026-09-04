#pragma once

#include "core/color.h"
#include "core/fixture-profile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

enum class EffectType : uint8_t {
	Chaser,
	Strobe,
	Sound,
	BuiltinFx,
};

/// How an effect combines with what is already there.
enum class BlendMode : uint8_t {
	/// The effect replaces the base on the fixtures it targets.
	Replace,
	/// Brightest wins, as on a lighting desk. A strobe can then flash over a
	/// coloured background without erasing it between flashes.
	Htp,
};

enum class ChaserDirection : uint8_t {
	Forward,
	Backward,
	PingPong,
	Random,
};

/// A chase runs a sequence of colours along the targeted fixtures, in the order
/// they are listed. It is the classic running-light model, and it is set up with
/// two numbers instead of a table.
struct ChaserSettings {
	std::vector<LightState> steps;
	/// Duration of one step. Ignored when tempo sync is on.
	int stepMs = 500;
	bool useBpm = false;
	float bpm = 120.0f;
	/// Share of each step spent fading: 0 for a hard cut, 1 for a continuous
	/// fade.
	float fadeRatio = 0.0f;
	ChaserDirection direction = ChaserDirection::Forward;
};

struct StrobeSettings {
	float hz = 8.0f;
	/// Share of each cycle the fixture stays lit.
	float dutyCycle = 0.5f;
	/// Take the programme's colour instead of imposing its own.
	bool useBaseColor = true;
	LightState color;
	/// Use the fixture's own strobe channel when it has one: at a 40 Hz refresh
	/// rate, a software strobe aliases above roughly ten hertz.
	bool preferHardware = true;
};

/// What the sound-reactive effect drives.
enum class SoundTarget : uint8_t {
	Intensity,  ///< l'intensite suit le volume
	Hue,        ///< la teinte suit le contenu frequentiel
	StepOnBeat,  ///< the paired chase advances one step per beat
	FlashOnBeat,///< un eclat a chaque temps
};

struct SoundSettings {
	SoundTarget target = SoundTarget::Intensity;
	/// Band listened to: 0 bass, 1 mids, 2 treble.
	int band = 0;
	float sensitivity = 1.0f;
	float threshold = 0.05f;
	/// Smoothing constant, in milliseconds, for the decay.
	float smoothingMs = 120.0f;

	/// Take the programme's colour instead of imposing its own.
	bool useBaseColor = true;
	LightState color;
};

/// A channel forced by hand.
///
/// The number is the one in the manufacturer's chart: 1 is the fixture's first
/// channel, not an absolute DMX address. That is how channel tables are written,
/// and it follows the fixture if it is readdressed.
struct ManualChannel {
	int channel = 1;
	uint8_t value = 0;
};

/// An effect built into the fixture, such as the T4c's FX mode.
struct BuiltinFxSettings {
	std::string effectId;
	/// 1 to 10, or 0 for the random setting where the effect allows it.
	int frequency = 5;
	/// Variant of the effect: colour combination, temperature range.
	int variant = 0;

	/// Direct channel entry, for fixtures whose profile does not describe their
	/// effects. The user then copies the manufacturer's chart.
	bool useManual = false;
	std::vector<ManualChannel> manual;
};

struct Effect {
	std::string id;
	std::string name;
	EffectType type = EffectType::Chaser;
	bool enabled = true;
	BlendMode blend = BlendMode::Htp;

	/// Targeted fixtures, in order: that order is what gives a chase its
	/// direction.
	std::vector<std::string> fixtureIds;

	ChaserSettings chaser;
	StrobeSettings strobe;
	SoundSettings sound;
	BuiltinFxSettings builtin;
};

/// What the engine knows about the audio at a given instant. Filled by the audio
/// thread through atomics, read by the render thread.
struct AudioSnapshot {
	/// Per-band envelopes, 0 to 1: bass, mids, treble.
	float bands[3] = {0.0f, 0.0f, 0.0f};
	/// Beats detected since start-up. A counter rather than a flag, so the
	/// render cannot miss a beat that fell between two frames.
	uint64_t beatCount = 0;
};

/// Combines two states according to the blend mode.
LightState blend(const LightState &base, const LightState &overlay, BlendMode mode);

} // namespace obsdmx
