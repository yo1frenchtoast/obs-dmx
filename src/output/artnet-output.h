#pragma once

#include "output/dmx-output.h"
#include "output/udp-socket.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// Art-Net (ArtDMX) output over UDP.
///
/// Art-Net numbers universes on 15 bits: the low 8 form the SubUni, the high 7
/// the Net. The user is shown a plain 0-32767 number and the split happens
/// here.
class ArtnetOutput final : public DmxOutput {
public:
	static constexpr uint16_t kPort = 6454;

	/// host: the node's address, or a broadcast address.
	ArtnetOutput(std::string host, uint16_t port = kPort);
	~ArtnetOutput() override;

	std::string name() const override;

	bool open(std::string &error) override;
	void close() override;
	void send(const Universe &universe) override;

	/// Builds a complete ArtDMX frame. Exposed for the tests.
	static std::vector<uint8_t> buildPacket(uint16_t universeId, uint8_t sequence, const uint8_t *slots,
						size_t slotCount);

private:
	std::string host_;
	uint16_t port_;
	UdpSocket socket_;
	uint8_t sequence_ = 0;
};

} // namespace obsdmx
