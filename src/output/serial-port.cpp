#include "output/serial-port.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace obsdmx {

namespace {

/// The baud rate has no real effect: an Enttec interface generates the DMX
/// timing itself. We set a usual value so the driver is happy.
constexpr int kBaudRate = 115200;

#ifdef _WIN32

constexpr std::intptr_t kInvalid = reinterpret_cast<std::intptr_t>(INVALID_HANDLE_VALUE);

std::string lastError()
{
	const DWORD code = GetLastError();
	char *text = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			       FORMAT_MESSAGE_IGNORE_INSERTS,
		       nullptr, code, 0, reinterpret_cast<char *>(&text), 0, nullptr);
	std::string message = text ? text : "";
	if (text)
		LocalFree(text);
	while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
		message.pop_back();
	return message.empty() ? ("error " + std::to_string(code)) : message;
}

/// Beyond COM9 the plain name stops working and the device has to be reached
/// through the raw namespace. Using it for every port avoids the special case.
std::string devicePath(const std::string &port)
{
	return port.rfind("\\\\.\\", 0) == 0 ? port : "\\\\.\\" + port;
}

#else

constexpr std::intptr_t kInvalid = -1;

std::string lastError()
{
	return std::strerror(errno);
}

#endif

} // namespace

SerialPort::~SerialPort()
{
	close();
}

bool SerialPort::isOpen() const
{
	return handle_ != kInvalid;
}

void SerialPort::close()
{
	if (!isOpen())
		return;
#ifdef _WIN32
	CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
	::close(static_cast<int>(handle_));
#endif
	handle_ = kInvalid;
}

#ifdef _WIN32

bool SerialPort::open(const std::string &path, std::string &error)
{
	close();

	const HANDLE opened = CreateFileA(devicePath(path).c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
					  FILE_ATTRIBUTE_NORMAL, nullptr);
	if (opened == INVALID_HANDLE_VALUE) {
		error = path + ": " + lastError();
		return false;
	}

	DCB state{};
	state.DCBlength = sizeof(state);
	if (!GetCommState(opened, &state)) {
		error = "GetCommState(): " + lastError();
		CloseHandle(opened);
		return false;
	}

	// 8 data bits, no parity, two stop bits, no flow control: the frame shape
	// the Enttec protocol expects.
	state.BaudRate = kBaudRate;
	state.ByteSize = 8;
	state.Parity = NOPARITY;
	state.StopBits = TWOSTOPBITS;
	state.fBinary = TRUE;
	state.fParity = FALSE;
	state.fOutxCtsFlow = FALSE;
	state.fOutxDsrFlow = FALSE;
	state.fDtrControl = DTR_CONTROL_DISABLE;
	state.fRtsControl = RTS_CONTROL_DISABLE;
	state.fOutX = FALSE;
	state.fInX = FALSE;

	if (!SetCommState(opened, &state)) {
		error = "SetCommState(): " + lastError();
		CloseHandle(opened);
		return false;
	}

	// Bound every write so a wedged device cannot stall the engine thread.
	COMMTIMEOUTS timeouts{};
	timeouts.WriteTotalTimeoutConstant = 50;
	SetCommTimeouts(opened, &timeouts);

	handle_ = reinterpret_cast<std::intptr_t>(opened);
	return true;
}

void SerialPort::write(const uint8_t *data, size_t size)
{
	if (!isOpen())
		return;

	DWORD written = 0;
	WriteFile(reinterpret_cast<HANDLE>(handle_), data, static_cast<DWORD>(size), &written, nullptr);
}

std::vector<std::string> SerialPort::listCandidates()
{
	std::vector<std::string> ports;

	// QueryDosDevice tells whether a COM name is actually mapped. Probing the
	// range is cruder than enumerating the registry, but it has no dependency
	// and no ordering surprises.
	char target[256];
	for (int index = 1; index <= 64; ++index) {
		const std::string name = "COM" + std::to_string(index);
		if (QueryDosDeviceA(name.c_str(), target, sizeof(target)) != 0)
			ports.push_back(name);
	}

	return ports;
}

#else

bool SerialPort::open(const std::string &path, std::string &error)
{
	close();

	const int opened = ::open(path.c_str(), O_WRONLY | O_NOCTTY | O_NONBLOCK);
	if (opened < 0) {
		error = path + ": " + lastError();
#ifdef __linux__
		if (errno == EACCES)
			error += " (the user must belong to the dialout group)";
#endif
		return false;
	}

	termios options{};
	if (::tcgetattr(opened, &options) < 0) {
		error = "tcgetattr(): " + lastError();
		::close(opened);
		return false;
	}

	// Raw mode: no interpretation of the bytes, the interface expects an exact
	// binary stream.
	::cfmakeraw(&options);
	::cfsetispeed(&options, B115200);
	::cfsetospeed(&options, B115200);

	options.c_cflag |= CLOCAL | CREAD;
	options.c_cflag &= ~CRTSCTS;

	if (::tcsetattr(opened, TCSANOW, &options) < 0) {
		error = "tcsetattr(): " + lastError();
		::close(opened);
		return false;
	}

	handle_ = opened;
	return true;
}

void SerialPort::write(const uint8_t *data, size_t size)
{
	if (!isOpen())
		return;

	// Non-blocking descriptor: if the kernel buffer is full we drop the frame
	// rather than delay the engine.
	size_t written = 0;
	while (written < size) {
		const ssize_t n = ::write(static_cast<int>(handle_), data + written, size - written);
		if (n <= 0)
			break;
		written += static_cast<size_t>(n);
	}
}

std::vector<std::string> SerialPort::listCandidates()
{
	std::vector<std::string> ports;

	DIR *dir = ::opendir("/dev");
	if (!dir)
		return ports;

	while (const dirent *entry = ::readdir(dir)) {
		const std::string name = entry->d_name;

		// macOS exposes the callout device as cu.*; the tty.* twin blocks on
		// open waiting for carrier detect, which never comes on a DMX box.
		// Linux names FTDI chips ttyUSB and CDC devices ttyACM.
		const bool candidate = name.rfind("cu.", 0) == 0 || name.rfind("ttyUSB", 0) == 0 ||
				       name.rfind("ttyACM", 0) == 0;
		if (candidate)
			ports.push_back("/dev/" + name);
	}
	::closedir(dir);

	std::sort(ports.begin(), ports.end());
	return ports;
}

#endif

} // namespace obsdmx
