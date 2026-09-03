#include "test-harness.h"

#include "core/dmx-engine.h"

#include <thread>

using obsdmx::DmxEngine;
using obsdmx::Universe;

TEST(univers_adresse_de_1_a_512)
{
	Universe universe;

	universe.set(1, 10);
	CHECK_EQ(universe.get(1), 10);
	CHECK_EQ(universe.data()[0], 10);

	universe.set(512, 20);
	CHECK_EQ(universe.data()[511], 20);

	// Hors bornes : ignore silencieusement plutot que de deborder.
	universe.set(0, 99);
	universe.set(513, 99);
	CHECK_EQ(universe.get(0), 0);
	CHECK_EQ(universe.get(513), 0);
}

TEST(fusion_htp_garde_la_plus_forte_valeur)
{
	Universe universe;

	universe.mergeHtp(1, 100);
	CHECK_EQ(universe.get(1), 100);

	universe.mergeHtp(1, 50);
	CHECK_EQ(universe.get(1), 100);

	universe.mergeHtp(1, 200);
	CHECK_EQ(universe.get(1), 200);
}

TEST(moteur_appelle_le_rendu_a_environ_40_hz)
{
	DmxEngine engine;

	engine.setRenderFn([](std::vector<Universe> &universes, std::chrono::steady_clock::time_point) {
		universes[0].set(1, 255);
	});

	engine.start();
	CHECK(engine.running());

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	engine.stop();

	// 500 ms a 40 Hz font 20 trames. On tolere largement : la machine de
	// test peut etre chargee, on verifie l'ordre de grandeur, pas la mesure.
	const uint64_t frames = engine.framesSent();
	CHECK(frames >= 12);
	CHECK(frames <= 28);

	CHECK_EQ(engine.snapshot()[0].get(1), 255);
}

TEST(blackout_ignore_le_rendu)
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

TEST(arret_sans_demarrage_est_sans_effet)
{
	DmxEngine engine;
	engine.stop();
	CHECK(!engine.running());
	CHECK_EQ(engine.framesSent(), uint64_t(0));
}
