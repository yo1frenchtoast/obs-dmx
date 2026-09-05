#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace obsdmx {

/// A write-only serial port, the second and last place the platform shows
/// through.
///
/// The Enttec protocol is the same everywhere; opening a port is not. POSIX
/// wants termios on a file descriptor, Windows a DCB on a handle, and the two
/// enumerate devices in entirely different ways.
class SerialPort {
public:
	SerialPort() = default;
	~SerialPort();

	SerialPort(const SerialPort &) = delete;
	SerialPort &operator=(const SerialPort &) = delete;

	/// Opens the port in raw mode. Returns false and fills error on failure,
	/// including the hint about group membership that explains most of them on
	/// Linux.
	bool open(const std::string &path, std::string &error);
	void close();
	bool isOpen() const;

	/// Writes everything or gives up. Never blocks the caller for long: a
	/// dropped frame beats a stalled engine.
	void write(const uint8_t *data, size_t size);

	/// Ports on this machine that could plausibly be a DMX interface.
	static std::vector<std::string> listCandidates();

private:
	/// A POSIX file descriptor or a Windows HANDLE, whichever is wider.
	std::intptr_t handle_ = -1;
};

} // namespace obsdmx
