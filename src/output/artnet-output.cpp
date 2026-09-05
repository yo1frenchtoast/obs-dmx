#include "output/artnet-output.h"

#include <cstring>

namespace obsdmx {

namespace {

constexpr char kArtnetId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0'};
constexpr uint16_t kOpDmx = 0x5000;
constexpr uint8_t kProtocolVersion = 14;
constexpr size_t kHeaderSize = 18;

} // namespace

ArtnetOutput::ArtnetOutput(std::string host, uint16_t port) : host_(std::move(host)), port_(port) {}

ArtnetOutput::~ArtnetOutput()
{
	close();
}

std::string ArtnetOutput::name() const
{
	return "Art-Net " + host_;
}

std::vector<uint8_t> ArtnetOutput::buildPacket(uint16_t universeId, uint8_t sequence, const uint8_t *slots,
					       size_t slotCount)
{
	// Art-Net requires an even slot count, between 2 and 512.
	if (slotCount > kSlotsPerUniverse)
		slotCount = kSlotsPerUniverse;
	if (slotCount < 2)
		slotCount = 2;
	if (slotCount % 2 != 0)
		++slotCount;

	std::vector<uint8_t> packet(kHeaderSize + slotCount, 0);

	std::memcpy(packet.data(), kArtnetId, sizeof(kArtnetId));

	// OpCode: little-endian, unlike the rest of the header.
	packet[8] = static_cast<uint8_t>(kOpDmx & 0xFF);
	packet[9] = static_cast<uint8_t>(kOpDmx >> 8);

	packet[10] = 0; // ProtVerHi
	packet[11] = kProtocolVersion;
	packet[12] = sequence;
	packet[13] = 0; // Physical, purement informatif

	packet[14] = static_cast<uint8_t>(universeId & 0xFF);        // SubUni
	packet[15] = static_cast<uint8_t>((universeId >> 8) & 0x7F); // Net

	// Length: big-endian.
	packet[16] = static_cast<uint8_t>(slotCount >> 8);
	packet[17] = static_cast<uint8_t>(slotCount & 0xFF);

	if (slots)
		std::memcpy(packet.data() + kHeaderSize, slots, slotCount);

	return packet;
}

bool ArtnetOutput::open(std::string &error)
{
	if (!socket_.open(error))
		return false;

	// Allow broadcast: it is the usual way to reach a node whose address is
	// unknown.
	if (!socket_.allowBroadcast(error)) {
		socket_.close();
		return false;
	}

	// Connected socket: avoids repeating the address on every frame, and lets
	// the kernel report ICMP errors back to us.
	if (!socket_.connectTo(host_, port_, error)) {
		socket_.close();
		return false;
	}

	sequence_ = 0;
	return true;
}

void ArtnetOutput::close()
{
	socket_.close();
}

void ArtnetOutput::send(const Universe &universe)
{
	if (!socket_.isOpen())
		return;

	// The sequence lets the receiver spot out-of-order frames. Value 0 means
	// "not in use", so we skip it when wrapping.
	if (++sequence_ == 0)
		sequence_ = 1;

	const auto packet = buildPacket(universe.id(), sequence_, universe.data(), Universe::size());

	socket_.send(packet.data(), packet.size());
}

} // namespace obsdmx
