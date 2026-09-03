#pragma once

#include "output/dmx-output.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// Sortie sACN / E1.31, en multicast par defaut.
///
/// Contrairement a Art-Net, l'univers commence a 1 et l'adresse multicast en
/// decoule : 239.255.<octet haut>.<octet bas>. Il n'y a donc rien a configurer
/// cote reseau, ce qui en fait le protocole le plus simple a mettre en oeuvre.
class SacnOutput final : public DmxOutput {
public:
	static constexpr uint16_t kPort = 5568;

	/// cid : identifiant unique de la source, stable dans le temps. Deux
	/// sources partageant un CID se perturbent mutuellement.
	/// sourceName : ce que les consoles affichent pour identifier l'emetteur.
	SacnOutput(std::array<uint8_t, 16> cid, std::string sourceName, uint8_t priority = 100);
	~SacnOutput() override;

	std::string name() const override;

	bool open(std::string &error) override;
	void close() override;
	void send(const Universe &universe) override;

	/// Construit un paquet E1.31 complet. Expose pour les tests.
	static std::vector<uint8_t> buildPacket(uint16_t universeId, uint8_t sequence, uint8_t priority,
						const std::array<uint8_t, 16> &cid, const std::string &sourceName,
						const uint8_t *slots, size_t slotCount);

	/// Adresse multicast normalisee pour un univers donne.
	static std::string multicastAddress(uint16_t universeId);

	/// Genere un CID aleatoire conforme a l'UUID version 4.
	static std::array<uint8_t, 16> generateCid();

private:
	std::array<uint8_t, 16> cid_;
	std::string sourceName_;
	uint8_t priority_;
	int socket_ = -1;
	uint8_t sequence_ = 0;
};

} // namespace obsdmx
