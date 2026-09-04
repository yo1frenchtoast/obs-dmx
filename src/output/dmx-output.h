#pragma once

#include "core/universe.h"

#include <string>

namespace obsdmx {

/// Common interface to every DMX output (Art-Net, sACN, Enttec).
/// Implementations are called from the engine thread, never from the audio
/// thread nor from the interface.
class DmxOutput {
public:
	virtual ~DmxOutput() = default;

	/// Name shown to the user.
	virtual std::string name() const = 0;

	/// Opens the output. Returns false and fills error on failure.
	virtual bool open(std::string &error) = 0;

	virtual void close() = 0;

	/// Sends one universe. Called on every engine tick (40 Hz).
	virtual void send(const Universe &universe) = 0;
};

} // namespace obsdmx
