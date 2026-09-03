#pragma once

#include "core/effect.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace obsdmx {

class Patch;

/// Applique les effets d'un programme par-dessus sa base.
///
/// L'etat qui doit persister d'une trame a l'autre (position d'un chaser,
/// dernier temps vu) vit ici plutot que dans l'effet, pour que la description
/// d'un programme reste une donnee inerte, serialisable et comparable.
class EffectRunner {
public:
	using Clock = std::chrono::steady_clock;

	/// Modifie states en place. mode sert a savoir si un projecteur dispose
	/// d'un canal de strobe materiel.
	void apply(const std::vector<Effect> &effects, const Patch &patch, const AudioSnapshot &audio,
		   Clock::time_point now, std::unordered_map<std::string, LightState> &states);

	/// Oublie les positions en cours. A appeler quand le programme change,
	/// pour qu'un chaser reparte de son premier pas.
	void reset();

private:
	struct Runtime {
		Clock::time_point started{};
		/// Pas courant, avance par le temps ou par les temps musicaux.
		int step = 0;
		int lastStepIndex = 0;
		bool ascending = true;
		uint64_t lastBeat = 0;
		uint32_t randomState = 0x9e3779b9u;
	};

	Runtime &runtimeFor(const Effect &effect, Clock::time_point now);

	void applyChaser(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio, Clock::time_point now,
			 std::unordered_map<std::string, LightState> &states);
	void applyStrobe(const Effect &effect, const Patch &patch, Clock::time_point now,
			 std::unordered_map<std::string, LightState> &states);
	void applySound(const Effect &effect, Runtime &runtime, const AudioSnapshot &audio,
			std::unordered_map<std::string, LightState> &states);

	std::unordered_map<std::string, Runtime> runtimes_;
};

/// Valeurs DMX brutes a forcer pour un effet embarque dans l'appareil.
/// Renvoie un vecteur vide si l'effet n'existe pas dans ce mode.
std::vector<std::pair<int, uint8_t>> builtinFxChannels(const FixtureMode &mode, const BuiltinFxSettings &settings);

} // namespace obsdmx
