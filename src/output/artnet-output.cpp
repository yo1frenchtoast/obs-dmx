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
	// Art-Net impose un nombre pair d'emplacements, entre 2 et 512.
	if (slotCount > kSlotsPerUniverse)
		slotCount = kSlotsPerUniverse;
	if (slotCount < 2)
		slotCount = 2;
	if (slotCount % 2 != 0)
		++slotCount;

	std::vector<uint8_t> packet(kHeaderSize + slotCount, 0);

	std::memcpy(packet.data(), kArtnetId, sizeof(kArtnetId));

	// OpCode : petit-boutiste, contrairement au reste de l'en-tete.
	packet[8] = static_cast<uint8_t>(kOpDmx & 0xFF);
	packet[9] = static_cast<uint8_t>(kOpDmx >> 8);

	packet[10] = 0; // ProtVerHi
	packet[11] = kProtocolVersion;
	packet[12] = sequence;
	packet[13] = 0; // Physical, purement informatif

	packet[14] = static_cast<uint8_t>(universeId & 0xFF);        // SubUni
	packet[15] = static_cast<uint8_t>((universeId >> 8) & 0x7F); // Net

	// Longueur : gros-boutiste.
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

	// Autoriser la diffusion : c'est le mode le plus courant pour decouvrir
	// un noeud dont on ignore l'adresse.
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
		error = "adresse IPv4 invalide : " + host_;
		close();
		return false;
	}

	// Socket connecte : on evite de repasser l'adresse a chaque trame, et le
	// noyau nous remonte les erreurs ICMP.
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

	// La sequence sert au recepteur a detecter les trames dans le desordre.
	// La valeur 0 signifie "non utilisee", on l'evite donc en bouclant.
	if (++sequence_ == 0)
		sequence_ = 1;

	const auto packet = buildPacket(universe.id(), sequence_, universe.data(), Universe::size());

	// MSG_DONTWAIT : perdre une trame vaut mieux que bloquer le moteur. La
	// suivante arrive dans 25 ms de toute facon.
	::send(socket_, packet.data(), packet.size(), MSG_DONTWAIT);
}

} // namespace obsdmx
