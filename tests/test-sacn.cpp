#include "test-harness.h"

#include "output/sacn-output.h"

#include <cstring>

using obsdmx::SacnOutput;
using obsdmx::Universe;

namespace {

std::array<uint8_t, 16> testCid()
{
	std::array<uint8_t, 16> cid{};
	for (size_t i = 0; i < cid.size(); ++i)
		cid[i] = static_cast<uint8_t>(i + 1);
	return cid;
}

uint16_t read16(const std::vector<uint8_t> &packet, size_t offset)
{
	return static_cast<uint16_t>((packet[offset] << 8) | packet[offset + 1]);
}

} // namespace

TEST(sacn_size_and_root_layer)
{
	uint8_t slots[512] = {};
	const auto packet = SacnOutput::buildPacket(1, 7, 100, testCid(), "obs-dmx", slots, sizeof(slots));

	// 126 bytes of stacked headers + 512 slots.
	CHECK_EQ(packet.size(), size_t(638));

	CHECK_EQ(read16(packet, 0), 0x0010); // preambule
	CHECK_EQ(read16(packet, 2), 0x0000); // post-ambule
	CHECK(std::memcmp(packet.data() + 4, "ASC-E1.17\0\0", 12) == 0);

	// PDU length: 0x7 marker on the top 4 bits, length on the other 12.
	CHECK_EQ(read16(packet, 16), uint16_t(0x7000 | (638 - 16)));
	CHECK_EQ(packet[21], 0x04); // VECTOR_ROOT_E131_DATA

	CHECK(std::memcmp(packet.data() + 22, testCid().data(), 16) == 0);
}

TEST(sacn_framing_layer)
{
	uint8_t slots[512] = {};
	const auto packet = SacnOutput::buildPacket(0x0102, 7, 42, testCid(), "obs-dmx", slots, sizeof(slots));

	CHECK_EQ(read16(packet, 38), uint16_t(0x7000 | (638 - 38)));
	CHECK_EQ(packet[43], 0x02); // VECTOR_E131_DATA_PACKET

	// Source name: 64 bytes, null-terminated.
	CHECK(std::memcmp(packet.data() + 44, "obs-dmx", 7) == 0);
	CHECK_EQ(packet[44 + 7], 0);

	CHECK_EQ(packet[108], 42);            // priorite
	CHECK_EQ(read16(packet, 109), 0);     // adresse de synchronisation
	CHECK_EQ(packet[111], 7);             // sequence
	CHECK_EQ(packet[112], 0);             // options
	CHECK_EQ(read16(packet, 113), 0x0102); // univers, gros-boutiste
}

TEST(sacn_dmp_layer_and_data)
{
	Universe universe(1);
	universe.set(1, 255);
	universe.set(512, 42);

	const auto packet =
		SacnOutput::buildPacket(1, 1, 100, testCid(), "obs-dmx", universe.data(), Universe::size());

	CHECK_EQ(read16(packet, 115), uint16_t(0x7000 | (638 - 115)));
	CHECK_EQ(packet[117], 0x02); // VECTOR_DMP_SET_PROPERTY
	CHECK_EQ(packet[118], 0xA1); // type d'adresse et de donnee
	CHECK_EQ(read16(packet, 119), 0x0000);
	CHECK_EQ(read16(packet, 121), 0x0001);

	// 513 = the start code plus the 512 slots.
	CHECK_EQ(read16(packet, 123), 513);
	CHECK_EQ(packet[125], 0x00); // code de depart DMX

	CHECK_EQ(packet[126], 255);       // adresse DMX 1
	CHECK_EQ(packet[126 + 511], 42);  // adresse DMX 512
}

TEST(sacn_multicast_address_derives_from_the_universe)
{
	CHECK(SacnOutput::multicastAddress(1) == "239.255.0.1");
	CHECK(SacnOutput::multicastAddress(255) == "239.255.0.255");
	CHECK(SacnOutput::multicastAddress(256) == "239.255.1.0");
	CHECK(SacnOutput::multicastAddress(0x0102) == "239.255.1.2");
}

TEST(an_over_long_sacn_source_name_is_truncated)
{
	uint8_t slots[512] = {};
	const std::string tresLong(200, 'x');
	const auto packet = SacnOutput::buildPacket(1, 1, 100, testCid(), tresLong, slots, sizeof(slots));

	CHECK_EQ(packet.size(), size_t(638));
	// 63 characters at most, then a terminating zero: the field must not spill
	// onto the priority that follows it.
	CHECK_EQ(packet[44 + 63], 0);
	CHECK_EQ(packet[108], 100);
}

TEST(the_sacn_cid_is_a_uuid_v4)
{
	const auto cid = SacnOutput::generateCid();
	CHECK_EQ(cid[6] & 0xF0, 0x40); // version 4
	CHECK_EQ(cid[8] & 0xC0, 0x80); // variante RFC 4122

	// Two draws must not coincide.
	CHECK(SacnOutput::generateCid() != cid);
}
