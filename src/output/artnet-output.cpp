#include "output/artnet-output.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

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
	close();

	socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_ < 0) {
		error = std::string("socket(): ") + std::strerror(errno);
		return false;
	}

	// Allow broadcast: it is the usual way to reach a node whose address is
	// unknown.
	int broadcast = 1;
	if (::setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
		error = std::string("setsockopt(SO_BROADCAST): ") + std::strerror(errno);
		close();
		return false;
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_);
	if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
		error = "invalid IPv4 address: " + host_;
		close();
		return false;
	}

	// Connected socket: avoids repeating the address on every frame, and lets
	// the kernel report ICMP errors back to us.
	if (::connect(socket_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
		error = std::string("connect(): ") + std::strerror(errno);
		close();
		return false;
	}

	sequence_ = 0;
	return true;
}

void ArtnetOutput::close()
{
	if (socket_ >= 0) {
		::close(socket_);
		socket_ = -1;
	}
}

void ArtnetOutput::send(const Universe &universe)
{
	if (socket_ < 0)
		return;

	// The sequence lets the receiver spot out-of-order frames. Value 0 means
	// "not in use", so we skip it when wrapping.
	if (++sequence_ == 0)
		sequence_ = 1;

	const auto packet = buildPacket(universe.id(), sequence_, universe.data(), Universe::size());

	// MSG_DONTWAIT: dropping a frame beats blocking the engine. The next one
	// arrives in 25 ms anyway.
	::send(socket_, packet.data(), packet.size(), MSG_DONTWAIT);
}

} // namespace obsdmx
