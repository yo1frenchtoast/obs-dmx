#pragma once

#include <cstdint>
#include <string>

namespace obsdmx {

/// A UDP socket, thin enough to be one of only two places where the platform
/// shows through.
///
/// Art-Net and sACN differ in their packets, not in how they reach the network,
/// so both go through this. Windows needs Winsock, whose types, error reporting
/// and shutdown all differ from POSIX; keeping that in one file means the
/// protocol code never has to know.
class UdpSocket {
public:
	UdpSocket() = default;
	~UdpSocket();

	UdpSocket(const UdpSocket &) = delete;
	UdpSocket &operator=(const UdpSocket &) = delete;

	bool open(std::string &error);
	void close();
	bool isOpen() const;

	/// Needed to reach a node whose address is unknown.
	bool allowBroadcast(std::string &error);

	/// Hop limit for multicast. sACN needs it; Art-Net does not.
	bool setMulticastTtl(int ttl, std::string &error);

	/// Let a receiver on this same machine see what we send. Failing to set it
	/// is not fatal: it only costs local monitoring.
	void setMulticastLoopback(bool on);

	/// Fixes the destination once. The kernel then reports ICMP errors back to
	/// us, and the address no longer has to be passed on every frame.
	bool connectTo(const std::string &host, uint16_t port, std::string &error);

	/// Sends to the connected destination. Never blocks: dropping a frame beats
	/// delaying the engine, and the next one arrives in 25 ms.
	void send(const void *data, size_t size);

	/// Sends to an explicit destination, for multicast where it changes with
	/// the universe.
	void sendTo(const std::string &host, uint16_t port, const void *data, size_t size);

private:
	/// Winsock's SOCKET is an unsigned handle, POSIX's is a signed descriptor.
	/// Storing the widest of the two keeps the header free of platform headers.
	std::intptr_t handle_ = -1;
};

} // namespace obsdmx
