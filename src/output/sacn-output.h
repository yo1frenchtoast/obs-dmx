#pragma once

#include "output/dmx-output.h"
#include "output/udp-socket.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// sACN / E1.31 output, multicast by default.
///
/// Unlike Art-Net, universes start at 1 and the multicast address follows from
/// them: 239.255.<high byte>.<low byte>. Nothing has to be configured on the
/// network side, which makes it the simplest of the three to set up.
class SacnOutput final : public DmxOutput {
public:
	static constexpr uint16_t kPort = 5568;

	/// cid: unique identifier of the source, stable over time. Two sources
	/// sharing a CID interfere with each other.
	/// sourceName: what desks display to identify the sender.
	SacnOutput(std::array<uint8_t, 16> cid, std::string sourceName, uint8_t priority = 100);
	~SacnOutput() override;

	std::string name() const override;

	bool open(std::string &error) override;
	void close() override;
	void send(const Universe &universe) override;

	/// Builds a complete E1.31 packet. Exposed for the tests.
	static std::vector<uint8_t> buildPacket(uint16_t universeId, uint8_t sequence, uint8_t priority,
						const std::array<uint8_t, 16> &cid, const std::string &sourceName,
						const uint8_t *slots, size_t slotCount);

	/// Standard multicast address for a given universe.
	static std::string multicastAddress(uint16_t universeId);

	/// Generates a random CID conforming to UUID version 4.
	static std::array<uint8_t, 16> generateCid();

private:
	std::array<uint8_t, 16> cid_;
	std::string sourceName_;
	uint8_t priority_;
	UdpSocket socket_;
	uint8_t sequence_ = 0;
};

} // namespace obsdmx
