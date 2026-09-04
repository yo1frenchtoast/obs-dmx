#include "test-harness.h"

#include "core/dmx-engine.h"

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

TEST(the_engine_calls_the_render_at_about_40_hz)
{
	DmxEngine engine;

	engine.setRenderFn([](std::vector<Universe> &universes, std::chrono::steady_clock::time_point) {
		universes[0].set(1, 255);
	});

	engine.start();
	CHECK(engine.running());

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	engine.stop();

	// 500 ms at 40 Hz is 20 frames. The tolerance is wide: the test machine may
	// be busy, and we check the order of magnitude, not the measurement.
	const uint64_t frames = engine.framesSent();
	CHECK(frames >= 12);
	CHECK(frames <= 28);

	CHECK_EQ(engine.snapshot()[0].get(1), 255);
}

TEST(blackout_skips_the_render)
{
	DmxEngine engine;
	engine.setRenderFn([](std::vector<Universe> &universes, std::chrono::steady_clock::time_point) {
		universes[0].set(1, 255);
	});

	engine.setBlackout(true);
	engine.start();
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	engine.stop();

	CHECK_EQ(engine.snapshot()[0].get(1), 0);
	CHECK(engine.framesSent() > 0);
}

TEST(stopping_without_starting_does_nothing)
{
	DmxEngine engine;
	engine.stop();
	CHECK(!engine.running());
	CHECK_EQ(engine.framesSent(), uint64_t(0));
}
