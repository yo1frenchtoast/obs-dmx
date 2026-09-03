#pragma once

#include "output/dmx-output.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// Sortie Art-Net (ArtDMX) en UDP.
///
/// Art-Net numerote les univers sur 15 bits : les 8 bits bas forment le
/// SubUni, les 7 bits hauts le Net. On expose a l'utilisateur un simple
/// numero 0-32767 et on fait la decomposition ici.
class ArtnetOutput final : public DmxOutput {
public:
	static constexpr uint16_t kPort = 6454;

	/// host : adresse du noeud, ou une adresse de diffusion.
	ArtnetOutput(std::string host, uint16_t port = kPort);
	~ArtnetOutput() override;

	std::string name() const override;

	bool open(std::string &error) override;
	void close() override;
	void send(const Universe &universe) override;

	/// Construit une trame ArtDMX complete. Expose pour les tests.
	static std::vector<uint8_t> buildPacket(uint16_t universeId, uint8_t sequence, const uint8_t *slots,
						size_t slotCount);

private:
	std::string host_;
	uint16_t port_;
	int socket_ = -1;
	uint8_t sequence_ = 0;
};

} // namespace obsdmx
