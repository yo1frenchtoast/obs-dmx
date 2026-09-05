#include "output/enttec-output.h"

#include <algorithm>

namespace obsdmx {

namespace {

constexpr uint8_t kStartOfMessage = 0x7E;
constexpr uint8_t kEndOfMessage = 0xE7;
constexpr uint8_t kLabelOutputDmx = 6;

/// The message carries the DMX start code on top of the 512 slots.
constexpr size_t kStartCodeSize = 1;

} // namespace

EnttecOutput::EnttecOutput(std::string devicePath) : devicePath_(std::move(devicePath)) {}

EnttecOutput::~EnttecOutput()
{
	close();
}

std::string EnttecOutput::name() const
{
	return "Enttec " + devicePath_;
}

std::vector<uint8_t> EnttecOutput::buildMessage(const uint8_t *slots, size_t slotCount)
{
	if (slotCount > kSlotsPerUniverse)
		slotCount = kSlotsPerUniverse;

	// Enttec firmware rejects frames that are too short: 25 slots is the
	// documented minimum.
	slotCount = std::max<size_t>(slotCount, 24);

	const size_t payload = kStartCodeSize + slotCount;

	std::vector<uint8_t> message;
	message.reserve(5 + payload);

	message.push_back(kStartOfMessage);
	message.push_back(kLabelOutputDmx);
	message.push_back(static_cast<uint8_t>(payload & 0xFF)); // longueur, petit-boutiste
	message.push_back(static_cast<uint8_t>(payload >> 8));
	message.push_back(0x00); // code de depart DMX

	message.insert(message.end(), slots, slots + slotCount);
	message.push_back(kEndOfMessage);

	return message;
}

std::vector<std::string> EnttecOutput::listCandidatePorts()
{
	return SerialPort::listCandidates();
}

bool EnttecOutput::open(std::string &error)
{
	return port_.open(devicePath_, error);
}

void EnttecOutput::close()
{
	port_.close();
}

void EnttecOutput::send(const Universe &universe)
{
	if (!port_.isOpen())
		return;

	const auto message = buildMessage(universe.data(), Universe::size());
	port_.write(message.data(), message.size());
}

} // namespace obsdmx
