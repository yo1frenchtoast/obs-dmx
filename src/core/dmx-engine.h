#pragma once

#include "core/universe.h"
#include "output/dmx-output.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace obsdmx {

/// Cadence de rafraichissement DMX. 40 Hz est la valeur usuelle : au-dela le
/// gain est nul, en deca les fondus deviennent visiblement saccades.
inline constexpr auto kTickPeriod = std::chrono::milliseconds(25);

/// Le moteur : un thread a cadence fixe qui, a chaque tick, fait rendre les
/// programmes dans les univers puis pousse ceux-ci vers les sorties.
///
/// Le thread audio d'OBS n'appelle jamais rien d'ici : il se contente d'ecrire
/// des atomiques que le rendu vient lire.
class DmxEngine {
public:
	/// Rendu d'une trame. Recoit les univers remis a zero, a charge pour la
	/// fonction de les remplir. now sert aux effets temporels.
	using RenderFn = std::function<void(std::vector<Universe> &, std::chrono::steady_clock::time_point now)>;

	DmxEngine();
	~DmxEngine();

	DmxEngine(const DmxEngine &) = delete;
	DmxEngine &operator=(const DmxEngine &) = delete;

	void start();
	void stop();
	bool running() const { return running_.load(std::memory_order_relaxed); }

	/// Nombre d'univers gerés. Redimensionne a chaud.
	void setUniverseCount(size_t count);
	size_t universeCount() const;

	void setRenderFn(RenderFn fn);

	void addOutput(std::shared_ptr<DmxOutput> output);
	void clearOutputs();

	/// Coupe tout : les univers sont emis a zero, le rendu est ignore.
	void setBlackout(bool on) { blackout_.store(on, std::memory_order_relaxed); }
	bool blackout() const { return blackout_.load(std::memory_order_relaxed); }

	/// Copie de l'etat courant des univers, pour l'affichage.
	std::vector<Universe> snapshot() const;

	/// Nombre de trames emises depuis le demarrage, pour verifier la cadence.
	uint64_t framesSent() const { return frames_.load(std::memory_order_relaxed); }

private:
	void run();
	void tick(std::chrono::steady_clock::time_point now);

	mutable std::mutex mutex_;
	std::condition_variable cv_;
	std::thread thread_;

	std::atomic<bool> running_{false};
	std::atomic<bool> blackout_{false};
	std::atomic<uint64_t> frames_{0};

	std::vector<Universe> universes_;
	std::vector<std::shared_ptr<DmxOutput>> outputs_;
	RenderFn render_;
};

} // namespace obsdmx
