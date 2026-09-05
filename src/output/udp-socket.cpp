#include "output/udp-socket.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace obsdmx {

namespace {

#ifdef _WIN32

constexpr std::intptr_t kInvalid = static_cast<std::intptr_t>(INVALID_SOCKET);

/// Winsock has to be started before any socket call and stopped afterwards.
/// A static object ties that to the module's lifetime, which is the only scope
/// that makes sense for a plugin.
struct WinsockGuard {
	WinsockGuard()
	{
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);
	}
	~WinsockGuard() { WSACleanup(); }
};

void ensureStarted()
{
	static WinsockGuard guard;
}

std::string lastError()
{
	const int code = WSAGetLastError();
	char *text = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			       FORMAT_MESSAGE_IGNORE_INSERTS,
		       nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<char *>(&text), 0, nullptr);
	std::string message = text ? text : "";
	if (text)
		LocalFree(text);
	// Windows appends a newline to its messages, which reads badly inline.
	while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
		message.pop_back();
	return message.empty() ? ("winsock error " + std::to_string(code)) : message;
}

void closeHandle(std::intptr_t handle)
{
	::closesocket(static_cast<SOCKET>(handle));
}

int setOption(std::intptr_t handle, int level, int name, const void *value, size_t size)
{
	return ::setsockopt(static_cast<SOCKET>(handle), level, name, static_cast<const char *>(value),
			    static_cast<int>(size));
}

#else

constexpr std::intptr_t kInvalid = -1;

void ensureStarted() {}

std::string lastError()
{
	return std::strerror(errno);
}

void closeHandle(std::intptr_t handle)
{
	::close(static_cast<int>(handle));
}

int setOption(std::intptr_t handle, int level, int name, const void *value, size_t size)
{
	return ::setsockopt(static_cast<int>(handle), level, name, value, static_cast<socklen_t>(size));
}

#endif

/// Fills a destination address, or returns false if the host is not an IPv4
/// literal. We deliberately do not resolve names: a lighting rig is addressed by
/// number, and a DNS lookup on the engine thread would be a poor idea.
bool fillAddress(const std::string &host, uint16_t port, sockaddr_in &out)
{
	std::memset(&out, 0, sizeof(out));
	out.sin_family = AF_INET;
	out.sin_port = htons(port);
	return ::inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1;
}

} // namespace

UdpSocket::~UdpSocket()
{
	close();
}

bool UdpSocket::isOpen() const
{
	return handle_ != kInvalid;
}

bool UdpSocket::open(std::string &error)
{
	close();
	ensureStarted();

	const auto created = static_cast<std::intptr_t>(::socket(AF_INET, SOCK_DGRAM, 0));
	if (created == kInvalid) {
		error = "socket(): " + lastError();
		return false;
	}

	handle_ = created;
	return true;
}

void UdpSocket::close()
{
	if (!isOpen())
		return;
	closeHandle(handle_);
	handle_ = kInvalid;
}

bool UdpSocket::allowBroadcast(std::string &error)
{
	const int on = 1;
	if (setOption(handle_, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) != 0) {
		error = "setsockopt(SO_BROADCAST): " + lastError();
		return false;
	}
	return true;
}

bool UdpSocket::setMulticastTtl(int ttl, std::string &error)
{
	// Windows wants a DWORD here, POSIX an int; both are four bytes, but the
	// signedness differs, so the value is copied into the expected type.
#ifdef _WIN32
	const DWORD value = static_cast<DWORD>(ttl);
#else
	const int value = ttl;
#endif
	if (setOption(handle_, IPPROTO_IP, IP_MULTICAST_TTL, &value, sizeof(value)) != 0) {
		error = "setsockopt(IP_MULTICAST_TTL): " + lastError();
		return false;
	}
	return true;
}

void UdpSocket::setMulticastLoopback(bool on)
{
#ifdef _WIN32
	const DWORD value = on ? 1 : 0;
#else
	const int value = on ? 1 : 0;
#endif
	setOption(handle_, IPPROTO_IP, IP_MULTICAST_LOOP, &value, sizeof(value));
}

bool UdpSocket::connectTo(const std::string &host, uint16_t port, std::string &error)
{
	sockaddr_in address{};
	if (!fillAddress(host, port, address)) {
		error = "invalid IPv4 address: " + host;
		return false;
	}

#ifdef _WIN32
	const int result = ::connect(static_cast<SOCKET>(handle_), reinterpret_cast<sockaddr *>(&address),
				     sizeof(address));
#else
	const int result = ::connect(static_cast<int>(handle_), reinterpret_cast<sockaddr *>(&address),
				     sizeof(address));
#endif
	if (result != 0) {
		error = "connect(): " + lastError();
		return false;
	}
	return true;
}

void UdpSocket::send(const void *data, size_t size)
{
	if (!isOpen())
		return;

#ifdef _WIN32
	// Winsock has no MSG_DONTWAIT; a UDP send does not block in practice, and
	// the socket stays in its default mode.
	::send(static_cast<SOCKET>(handle_), static_cast<const char *>(data), static_cast<int>(size), 0);
#else
	::send(static_cast<int>(handle_), data, size, MSG_DONTWAIT);
#endif
}

void UdpSocket::sendTo(const std::string &host, uint16_t port, const void *data, size_t size)
{
	if (!isOpen())
		return;

	sockaddr_in address{};
	if (!fillAddress(host, port, address))
		return;

#ifdef _WIN32
	::sendto(static_cast<SOCKET>(handle_), static_cast<const char *>(data), static_cast<int>(size), 0,
		 reinterpret_cast<sockaddr *>(&address), sizeof(address));
#else
	::sendto(static_cast<int>(handle_), data, size, MSG_DONTWAIT, reinterpret_cast<sockaddr *>(&address),
		 sizeof(address));
#endif
}

} // namespace obsdmx
