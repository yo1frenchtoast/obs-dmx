#include "test-harness.h"

#include "output/enttec-output.h"

using obsdmx::EnttecOutput;
using obsdmx::Universe;

TEST(enttec_encapsulation_du_message)
{
	Universe universe;
	universe.set(1, 255);
	universe.set(512, 42);

	const auto message = EnttecOutput::buildMessage(universe.data(), Universe::size());

	// 0x7E, label, 2 octets de longueur, code de depart, 512 valeurs, 0xE7.
	CHECK_EQ(message.size(), size_t(5 + 512 + 1));

	CHECK_EQ(message[0], 0x7E);
	CHECK_EQ(message[1], 6); // etiquette : emission DMX

	// Longueur en petit-boutiste : 513 = code de depart + 512 emplacements.
	CHECK_EQ(message[2], uint8_t(513 & 0xFF));
	CHECK_EQ(message[3], uint8_t(513 >> 8));

	CHECK_EQ(message[4], 0x00); // code de depart DMX

	CHECK_EQ(message[5], 255);       // adresse DMX 1
	CHECK_EQ(message[5 + 511], 42);  // adresse DMX 512

	CHECK_EQ(message.back(), 0xE7);
}

TEST(enttec_respecte_le_minimum_de_24_emplacements)
{
	uint8_t slots[512] = {};

	// Le micrologiciel rejette les trames plus courtes : on complete.
	const auto court = EnttecOutput::buildMessage(slots, 4);
	CHECK_EQ(court.size(), size_t(5 + 24 + 1));
	CHECK_EQ(court[2], uint8_t(25)); // 24 emplacements + code de depart

	const auto enorme = EnttecOutput::buildMessage(slots, 4096);
	CHECK_EQ(enorme.size(), size_t(5 + 512 + 1));
}

TEST(enttec_ouverture_d_un_port_absent_est_signalee)
{
	EnttecOutput output("/dev/ttyUSB-inexistant");
	std::string error;
	CHECK(!output.open(error));
	CHECK(!error.empty());
}
