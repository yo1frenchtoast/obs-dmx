#include "test-harness.h"

#include "output/enttec-output.h"

using obsdmx::EnttecOutput;
using obsdmx::Universe;

TEST(enttec_message_framing)
{
	Universe universe;
	universe.set(1, 255);
	universe.set(512, 42);

	const auto message = EnttecOutput::buildMessage(universe.data(), Universe::size());

	// 0x7E, label, 2 length bytes, start code, 512 values, 0xE7.
	CHECK_EQ(message.size(), size_t(5 + 512 + 1));

	CHECK_EQ(message[0], 0x7E);
	CHECK_EQ(message[1], 6); // etiquette : emission DMX

	// Length little-endian: 513 = start code + 512 slots.
	CHECK_EQ(message[2], uint8_t(513 & 0xFF));
	CHECK_EQ(message[3], uint8_t(513 >> 8));

	CHECK_EQ(message[4], 0x00); // code de depart DMX

	CHECK_EQ(message[5], 255);       // adresse DMX 1
	CHECK_EQ(message[5 + 511], 42);  // adresse DMX 512

	CHECK_EQ(message.back(), 0xE7);
}

TEST(enttec_honours_the_24_slot_minimum)
{
	uint8_t slots[512] = {};

	// The firmware rejects shorter frames, so we pad.
	const auto court = EnttecOutput::buildMessage(slots, 4);
	CHECK_EQ(court.size(), size_t(5 + 24 + 1));
	CHECK_EQ(court[2], uint8_t(25)); // 24 emplacements + code de depart

	const auto enorme = EnttecOutput::buildMessage(slots, 4096);
	CHECK_EQ(enorme.size(), size_t(5 + 512 + 1));
}

TEST(opening_a_missing_enttec_port_is_reported)
{
	EnttecOutput output("/dev/ttyUSB-inexistant");
	std::string error;
	CHECK(!output.open(error));
	CHECK(!error.empty());
}
