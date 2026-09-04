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

/// DMX refresh rate. 40 Hz is the usual value: above it there is nothing to
/// gain, below it fades become visibly steppy.
inline constexpr auto kTickPeriod = std::chrono::milliseconds(25);

/// The engine: a fixed-rate thread that, on every tick, renders the programmes
/// into the universes and then pushes those to the outputs.
///
/// OBS's audio thread never calls anything here: it only writes atomics that the
/// render reads.
class DmxEngine {
public:
	/// Renders one frame. Receives the universes cleared to zero and is
	/// expected to fill them. now drives the time-based effects.
	using RenderFn = std::function<void(std::vector<Universe> &, std::chrono::steady_clock::time_point now)>;

	DmxEngine();
	~DmxEngine();

	DmxEngine(const DmxEngine &) = delete;
	DmxEngine &operator=(const DmxEngine &) = delete;

	void start();
	void stop();
	bool running() const { return running_.load(std::memory_order_relaxed); }

	/// Number of universes handled. Resizes on the fly.
	void setUniverseCount(size_t count);
	size_t universeCount() const;

	/// DMX number of a universe. This is what goes into the Art-Net header, so
	/// it must match what the node is configured for.
	void setUniverseId(size_t index, uint16_t id);

	void setRenderFn(RenderFn fn);

	void addOutput(std::shared_ptr<DmxOutput> output);
	void clearOutputs();

	/// Kills everything: universes are sent as zero and the render is skipped.
	void setBlackout(bool on) { blackout_.store(on, std::memory_order_relaxed); }
	bool blackout() const { return blackout_.load(std::memory_order_relaxed); }

	/// Copy of the current universes, for display.
	std::vector<Universe> snapshot() const;

	/// Frames sent since start-up, to check the actual rate.
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
