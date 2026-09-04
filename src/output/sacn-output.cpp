#include "output/sacn-output.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <random>
#include <sys/socket.h>
#include <unistd.h>

namespace obsdmx {

namespace {

// Offsets of the three stacked layers of an E1.31 packet.
constexpr size_t kRootOffset = 0;
constexpr size_t kFramingOffset = 38;
constexpr size_t kDmpOffset = 115;
constexpr size_t kDataOffset = 126;

constexpr uint32_t kVectorRootData = 0x00000004;
constexpr uint32_t kVectorFramingData = 0x00000002;
constexpr uint8_t kVectorDmpSetProperty = 0x02;

/// All three PDUs carry their length on 12 bits, preceded by the 0x7 marker.
constexpr uint16_t pduFlagsAndLength(size_t length)
{
	return static_cast<uint16_t>(0x7000 | (length & 0x0FFF));
}

void put16(std::vector<uint8_t> &buffer, size_t offset, uint16_t value)
{
	buffer[offset] = static_cast<uint8_t>(value >> 8);
	buffer[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

void put32(std::vector<uint8_t> &buffer, size_t offset, uint32_t value)
{
	buffer[offset] = static_cast<uint8_t>(value >> 24);
	buffer[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
	buffer[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
	buffer[offset + 3] = static_cast<uint8_t>(value & 0xFF);
}

} // namespace

SacnOutput::SacnOutput(std::array<uint8_t, 16> cid, std::string sourceName, uint8_t priority)
	: cid_(cid), sourceName_(std::move(sourceName)), priority_(priority)
{
}

SacnOutput::~SacnOutput()
{
	close();
}

std::string SacnOutput::name() const
{
	return "sACN";
}

std::array<uint8_t, 16> SacnOutput::generateCid()
{
	std::random_device device;
	std::uniform_int_distribution<int> byte(0, 255);

	std::array<uint8_t, 16> cid{};
	for (auto &value : cid)
		value = static_cast<uint8_t>(byte(device));

	// UUID version 4 marking, RFC 4122 variant.
	cid[6] = static_cast<uint8_t>((cid[6] & 0x0F) | 0x40);
	cid[8] = static_cast<uint8_t>((cid[8] & 0x3F) | 0x80);
	return cid;
}

std::string SacnOutput::multicastAddress(uint16_t universeId)
{
	char buffer[16];
	std::snprintf(buffer, sizeof(buffer), "239.255.%u.%u", (universeId >> 8) & 0xFF, universeId & 0xFF);
	return buffer;
}

std::vector<uint8_t> SacnOutput::buildPacket(uint16_t universeId, uint8_t sequence, uint8_t priority,
					     const std::array<uint8_t, 16> &cid, const std::string &sourceName,
					     const uint8_t *slots, size_t slotCount)
{
	if (slotCount > kSlotsPerUniverse)
		slotCount = kSlotsPerUniverse;

	const size_t total = kDataOffset + slotCount;
	std::vector<uint8_t> packet(total, 0);

	// --- Root layer ---
	put16(packet, kRootOffset, 0x0010);     // taille du preambule
	put16(packet, kRootOffset + 2, 0x0000); // taille du post-ambule
	std::memcpy(packet.data() + 4, "ASC-E1.17\0\0", 12);
	put16(packet, 16, pduFlagsAndLength(total - 16));
	put32(packet, 18, kVectorRootData);
	std::memcpy(packet.data() + 22, cid.data(), cid.size());

	// --- Framing layer ---
	put16(packet, kFramingOffset, pduFlagsAndLength(total - kFramingOffset));
	put32(packet, kFramingOffset + 2, kVectorFramingData);

	// Source name: 64 bytes, truncated and null-terminated.
	const size_t nameLength = std::min<size_t>(sourceName.size(), 63);
	std::memcpy(packet.data() + kFramingOffset + 6, sourceName.data(), nameLength);

	packet[kFramingOffset + 70] = priority;
	put16(packet, kFramingOffset + 71, 0); // adresse de synchronisation : aucune
	packet[kFramingOffset + 73] = sequence;
	packet[kFramingOffset + 74] = 0; // options
	put16(packet, kFramingOffset + 75, universeId);

	// --- DMP layer ---
	put16(packet, kDmpOffset, pduFlagsAndLength(total - kDmpOffset));
	packet[kDmpOffset + 2] = kVectorDmpSetProperty;
	packet[kDmpOffset + 3] = 0xA1;             // type d'adresse et de donnee
	put16(packet, kDmpOffset + 4, 0x0000);     // premiere adresse
	put16(packet, kDmpOffset + 6, 0x0001);     // increment
	put16(packet, kDmpOffset + 8, static_cast<uint16_t>(slotCount + 1)); // + le code de depart
	packet[kDmpOffset + 10] = 0x00;            // code de depart DMX

	if (slots && slotCount > 0)
		std::memcpy(packet.data() + kDataOffset, slots, slotCount);

	return packet;
}

bool SacnOutput::open(std::string &error)
{
	close();

	socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_ < 0) {
		error = std::string("socket(): ") + std::strerror(errno);
		return false;
	}

	// 16 hops: ample for a venue network, without flooding beyond it.
	const int ttl = 16;
	if (::setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		error = std::string("setsockopt(IP_MULTICAST_TTL): ") + std::strerror(errno);
		close();
		return false;
	}

	// Loop back to the local machine: required for a visualiser running on the
	// same host to see anything.
	const int loop = 1;
	::setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

	sequence_ = 0;
	return true;
}

void SacnOutput::close()
{
	if (socket_ >= 0) {
		::close(socket_);
		socket_ = -1;
	}
}

void SacnOutput::send(const Universe &universe)
{
	if (socket_ < 0)
		return;

	// Here the sequence uses the full range: E1.31 does not reserve 0.
	++sequence_;

	const auto packet = buildPacket(universe.id(), sequence_, priority_, cid_, sourceName_, universe.data(),
					Universe::size());

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(kPort);
	::inet_pton(AF_INET, multicastAddress(universe.id()).c_str(), &addr.sin_addr);

	::sendto(socket_, packet.data(), packet.size(), MSG_DONTWAIT, reinterpret_cast<sockaddr *>(&addr),
		 sizeof(addr));
}

} // namespace obsdmx
