#pragma once

#include "output/dmx-output.h"
#include "output/serial-port.h"

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// Output to an Enttec DMX USB Pro, or a clone speaking the same protocol.
/// The device appears as a serial port and generates the DMX timing itself: all
/// we have to do is hand it the 512 values.
class EnttecOutput final : public DmxOutput {
public:
	explicit EnttecOutput(std::string devicePath);
	~EnttecOutput() override;

	std::string name() const override;

	bool open(std::string &error) override;
	void close() override;
	void send(const Universe &universe) override;

	/// Wraps the values in an Enttec message. Exposed for the tests.
	static std::vector<uint8_t> buildMessage(const uint8_t *slots, size_t slotCount);

	/// Serial ports on this machine that could plausibly be one.
	static std::vector<std::string> listCandidatePorts();

private:
	std::string devicePath_;
	SerialPort port_;
};

} // namespace obsdmx
