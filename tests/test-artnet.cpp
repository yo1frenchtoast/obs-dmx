#include "test-harness.h"

#include "output/artnet-output.h"

#include <cstring>

using obsdmx::ArtnetOutput;
using obsdmx::Universe;

TEST(artnet_entete_conforme)
{
	uint8_t slots[512] = {};
	const auto packet = ArtnetOutput::buildPacket(0, 1, slots, sizeof(slots));

	CHECK_EQ(packet.size(), size_t(18 + 512));
	CHECK(std::memcmp(packet.data(), "Art-Net\0", 8) == 0);

	// OpOutput / ArtDMX = 0x5000, en petit-boutiste.
	CHECK_EQ(packet[8], 0x00);
	CHECK_EQ(packet[9], 0x50);

	// Version de protocole 14, en gros-boutiste.
	CHECK_EQ(packet[10], 0);
	CHECK_EQ(packet[11], 14);

	CHECK_EQ(packet[12], 1); // sequence
	CHECK_EQ(packet[13], 0); // physical

	// Longueur : gros-boutiste, 512 = 0x0200.
	CHECK_EQ(packet[16], 0x02);
	CHECK_EQ(packet[17], 0x00);
}

TEST(artnet_decompose_univers_en_net_et_subuni)
{
	uint8_t slots[512] = {};

	const auto simple = ArtnetOutput::buildPacket(3, 1, slots, sizeof(slots));
	CHECK_EQ(simple[14], 3); // SubUni
	CHECK_EQ(simple[15], 0); // Net

	// 0x0105 : SubUni 0x05, Net 0x01.
	const auto grand = ArtnetOutput::buildPacket(0x0105, 1, slots, sizeof(slots));
	CHECK_EQ(grand[14], 0x05);
	CHECK_EQ(grand[15], 0x01);

	// Le bit de poids fort du Net est reserve et doit etre masque.
	const auto masque = ArtnetOutput::buildPacket(0xFF00, 1, slots, sizeof(slots));
	CHECK_EQ(masque[15], 0x7F);
}

TEST(artnet_impose_un_nombre_pair_de_slots)
{
	uint8_t slots[512] = {};

	const auto impair = ArtnetOutput::buildPacket(0, 1, slots, 7);
	CHECK_EQ(impair.size(), size_t(18 + 8));

	// Art-Net n'admet pas moins de 2 emplacements.
	const auto minuscule = ArtnetOutput::buildPacket(0, 1, slots, 1);
	CHECK_EQ(minuscule.size(), size_t(18 + 2));

	// Ni plus de 512.
	const auto enorme = ArtnetOutput::buildPacket(0, 1, slots, 4096);
	CHECK_EQ(enorme.size(), size_t(18 + 512));
}

TEST(artnet_transporte_les_valeurs_dmx)
{
	Universe universe(0);
	universe.set(1, 255);
	universe.set(2, 128);
	universe.set(512, 42);

	const auto packet = ArtnetOutput::buildPacket(universe.id(), 1, universe.data(), Universe::size());

	// L'adresse DMX 1 est le premier octet de donnees, soit l'offset 18.
	CHECK_EQ(packet[18], 255);
	CHECK_EQ(packet[19], 128);
	CHECK_EQ(packet[18 + 511], 42);
}
