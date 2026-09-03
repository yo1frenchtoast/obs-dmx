#pragma once

#include "output/dmx-output.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// Sortie vers une interface Enttec DMX USB Pro, ou un clone parlant le meme
/// protocole. L'interface se presente comme un port serie et genere elle-meme
/// le chronometrage DMX : nous n'avons qu'a lui passer les 512 valeurs.
class EnttecOutput final : public DmxOutput {
public:
	explicit EnttecOutput(std::string devicePath);
	~EnttecOutput() override;

	std::string name() const override;

	bool open(std::string &error) override;
	void close() override;
	void send(const Universe &universe) override;

	/// Encapsule les valeurs dans un message Enttec. Expose pour les tests.
	static std::vector<uint8_t> buildMessage(const uint8_t *slots, size_t slotCount);

	/// Ports serie plausibles presents sur la machine.
	static std::vector<std::string> listCandidatePorts();

private:
	std::string devicePath_;
	int fd_ = -1;
};

} // namespace obsdmx
