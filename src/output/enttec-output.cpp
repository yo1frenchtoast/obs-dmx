#include "output/enttec-output.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace obsdmx {

namespace {

constexpr uint8_t kStartOfMessage = 0x7E;
constexpr uint8_t kEndOfMessage = 0xE7;
constexpr uint8_t kLabelOutputDmx = 6;

/// Le message porte le code de depart DMX en plus des 512 emplacements.
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

	// Le micrologiciel Enttec refuse les trames trop courtes : 25 emplacements
	// est le minimum documente.
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
	std::vector<std::string> ports;

	DIR *dir = ::opendir("/dev");
	if (!dir)
		return ports;

	while (const dirent *entry = ::readdir(dir)) {
		const std::string name = entry->d_name;
		// Les interfaces DMX USB apparaissent en ttyUSB (puce FTDI) ou en
		// ttyACM (peripherique CDC).
		if (name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0)
			ports.push_back("/dev/" + name);
	}
	::closedir(dir);

	std::sort(ports.begin(), ports.end());
	return ports;
}

bool EnttecOutput::open(std::string &error)
{
	close();

	fd_ = ::open(devicePath_.c_str(), O_WRONLY | O_NOCTTY | O_NONBLOCK);
	if (fd_ < 0) {
		error = devicePath_ + ": " + std::strerror(errno);
		if (errno == EACCES)
			error += " (l'utilisateur doit appartenir au groupe dialout)";
		return false;
	}

	termios options{};
	if (::tcgetattr(fd_, &options) < 0) {
		error = std::string("tcgetattr(): ") + std::strerror(errno);
		close();
		return false;
	}

	// Mode brut : aucune interpretation des octets, l'interface attend un
	// flux binaire exact.
	::cfmakeraw(&options);

	// La vitesse est sans effet reel : l'interface genere elle-meme le
	// chronometrage DMX. On fixe une valeur usuelle pour que le pilote FTDI
	// soit content.
	::cfsetispeed(&options, B115200);
	::cfsetospeed(&options, B115200);

	options.c_cflag |= CLOCAL | CREAD;
	options.c_cflag &= ~CRTSCTS;

	if (::tcsetattr(fd_, TCSANOW, &options) < 0) {
		error = std::string("tcsetattr(): ") + std::strerror(errno);
		close();
		return false;
	}

	return true;
}

void EnttecOutput::close()
{
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = -1;
	}
}

void EnttecOutput::send(const Universe &universe)
{
	if (fd_ < 0)
		return;

	const auto message = buildMessage(universe.data(), Universe::size());

	// Descripteur non bloquant : si le tampon du noyau est plein, on laisse
	// tomber la trame plutot que de retarder le moteur.
	size_t written = 0;
	while (written < message.size()) {
		const ssize_t n = ::write(fd_, message.data() + written, message.size() - written);
		if (n <= 0)
			break;
		written += static_cast<size_t>(n);
	}
}

} // namespace obsdmx
