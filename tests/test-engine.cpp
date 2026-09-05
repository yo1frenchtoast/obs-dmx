#include "test-harness.h"

#include "core/dmx-engine.h"

#include <atomic>
#include <thread>

using obsdmx::DmxEngine;
using obsdmx::Universe;

TEST(a_universe_is_addressed_from_1_to_512)
{
	Universe universe;

	universe.set(1, 10);
	CHECK_EQ(universe.get(1), 10);
	CHECK_EQ(universe.data()[0], 10);

	universe.set(512, 20);
	CHECK_EQ(universe.data()[511], 20);

	// Out of range: silently ignored rather than overflowing.
	universe.set(0, 99);
	universe.set(513, 99);
	CHECK_EQ(universe.get(0), 0);
	CHECK_EQ(universe.get(513), 0);
}

TEST(htp_merging_keeps_the_higher_value)
{
	Universe universe;

	universe.mergeHtp(1, 100);
	CHECK_EQ(universe.get(1), 100);

	universe.mergeHtp(1, 50);
	CHECK_EQ(universe.get(1), 100);

	universe.mergeHtp(1, 200);
	CHECK_EQ(universe.get(1), 200);
}

TEST(the_tick_period_is_the_one_the_design_calls_for)
{
	// The rate itself cannot be asserted against a wall clock on shared
	// hardware, but the constant behind it can, and that is where a regression
	// would actually come from. The real cadence is measured on the wire by
	// tools/e2e, which reads it off the Art-Net frames.
	CHECK_EQ(obsdmx::kTickPeriod.count(), 25);
}

TEST(the_engine_paces_itself_instead_of_free_running)
{
	// Timing on shared hardware can honestly be asserted in one direction only:
	// a busy machine makes things slower, never faster. So we check that a
	// handful of frames took at least about as long as they should, which is
	// exactly what a broken wait would violate.
	constexpr int kFrames = 5;

	DmxEngine engine;
	std::atomic<int> renders{0};
	engine.setRenderFn([&renders](std::vector<Universe> &universes, std::chrono::steady_clock::time_point) {
		universes[0].set(1, 255);
		renders.fetch_add(1, std::memory_order_relaxed);
	});

	const auto started = std::chrono::steady_clock::now();
	engine.start();
	CHECK(engine.running());

	// Wait for frames rather than for a duration: a starved thread would
	// otherwise fail a test about the engine's own logic.
	const auto deadline = started + std::chrono::seconds(10);
	while (engine.framesSent() < kFrames && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(2));

	const auto elapsed = std::chrono::steady_clock::now() - started;
	engine.stop();

	CHECK(engine.framesSent() >= kFrames);
	CHECK_EQ(renders.load(), static_cast<int>(engine.framesSent()));

	// Five frames at 25 ms is 125 ms. Half of that leaves room for the first
	// frame landing early, and still rules out a free-running loop by three
	// orders of magnitude.
	const auto floorMs = kFrames * obsdmx::kTickPeriod.count() / 2;
	CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= floorMs);

	CHECK_EQ(engine.snapshot()[0].get(1), 255);
}

TEST(blackout_skips_the_render)
{
	DmxEngine engine;

	std::atomic<int> renders{0};
	engine.setRenderFn([&renders](std::vector<Universe> &universes, std::chrono::steady_clock::time_point) {
		universes[0].set(1, 255);
		renders.fetch_add(1, std::memory_order_relaxed);
	});

	engine.setBlackout(true);
	engine.start();

	// Wait for the engine to have ticked rather than for a fixed delay, so a
	// busy machine cannot turn this into a failure.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (engine.framesSent() < 2 && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	engine.stop();

	CHECK(engine.framesSent() >= 2);
	// Frames went out, but the render was never called and the universe stayed
	// dark.
	CHECK_EQ(renders.load(), 0);
	CHECK_EQ(engine.snapshot()[0].get(1), 0);
}

TEST(stopping_without_starting_does_nothing)
{
	DmxEngine engine;
	engine.stop();
	CHECK(!engine.running());
	CHECK_EQ(engine.framesSent(), uint64_t(0));
}
