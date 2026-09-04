#include "test-harness.h"

#include "output/artnet-output.h"

#include <cstring>

using obsdmx::ArtnetOutput;
using obsdmx::Universe;

TEST(artnet_header_is_conformant)
{
	uint8_t slots[512] = {};
	const auto packet = ArtnetOutput::buildPacket(0, 1, slots, sizeof(slots));

	CHECK_EQ(packet.size(), size_t(18 + 512));
	CHECK(std::memcmp(packet.data(), "Art-Net\0", 8) == 0);

	// OpOutput / ArtDMX = 0x5000, little-endian.
	CHECK_EQ(packet[8], 0x00);
	CHECK_EQ(packet[9], 0x50);

	// Protocol version 14, big-endian.
	CHECK_EQ(packet[10], 0);
	CHECK_EQ(packet[11], 14);

	CHECK_EQ(packet[12], 1); // sequence
	CHECK_EQ(packet[13], 0); // physical

	// Length: big-endian, 512 = 0x0200.
	CHECK_EQ(packet[16], 0x02);
	CHECK_EQ(packet[17], 0x00);
}

TEST(artnet_splits_the_universe_into_net_and_subuni)
{
	uint8_t slots[512] = {};

	const auto simple = ArtnetOutput::buildPacket(3, 1, slots, sizeof(slots));
	CHECK_EQ(simple[14], 3); // SubUni
	CHECK_EQ(simple[15], 0); // Net

	// 0x0105: SubUni 0x05, Net 0x01.
	const auto grand = ArtnetOutput::buildPacket(0x0105, 1, slots, sizeof(slots));
	CHECK_EQ(grand[14], 0x05);
	CHECK_EQ(grand[15], 0x01);

	// The top bit of Net is reserved and must be masked off.
	const auto masque = ArtnetOutput::buildPacket(0xFF00, 1, slots, sizeof(slots));
	CHECK_EQ(masque[15], 0x7F);
}

TEST(artnet_requires_an_even_slot_count)
{
	uint8_t slots[512] = {};

	const auto impair = ArtnetOutput::buildPacket(0, 1, slots, 7);
	CHECK_EQ(impair.size(), size_t(18 + 8));

	// Art-Net allows no fewer than 2 slots.
	const auto minuscule = ArtnetOutput::buildPacket(0, 1, slots, 1);
	CHECK_EQ(minuscule.size(), size_t(18 + 2));

	// Nor more than 512.
	const auto enorme = ArtnetOutput::buildPacket(0, 1, slots, 4096);
	CHECK_EQ(enorme.size(), size_t(18 + 512));
}

TEST(artnet_carries_the_dmx_values)
{
	Universe universe(0);
	universe.set(1, 255);
	universe.set(2, 128);
	universe.set(512, 42);

	const auto packet = ArtnetOutput::buildPacket(universe.id(), 1, universe.data(), Universe::size());

	// DMX address 1 is the first data byte, at offset 18.
	CHECK_EQ(packet[18], 255);
	CHECK_EQ(packet[19], 128);
	CHECK_EQ(packet[18 + 511], 42);
}
