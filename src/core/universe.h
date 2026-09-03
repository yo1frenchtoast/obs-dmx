#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace obsdmx {

inline constexpr int kSlotsPerUniverse = 512;

/// Un univers DMX : 512 emplacements, adresses 1 a 512 pour l'utilisateur.
class Universe {
public:
	explicit Universe(uint16_t id = 0) : id_(id) { clear(); }

	uint16_t id() const { return id_; }

	void clear() { slots_.fill(0); }

	/// address est l'adresse DMX vue par l'utilisateur : 1 a 512.
	void set(int address, uint8_t value)
	{
		if (address < 1 || address > kSlotsPerUniverse)
			return;
		slots_[static_cast<size_t>(address - 1)] = value;
	}

	uint8_t get(int address) const
	{
		if (address < 1 || address > kSlotsPerUniverse)
			return 0;
		return slots_[static_cast<size_t>(address - 1)];
	}

	/// Fusion HTP (highest takes precedence), comme sur une console.
	void mergeHtp(int address, uint8_t value)
	{
		if (address < 1 || address > kSlotsPerUniverse)
			return;
		auto &slot = slots_[static_cast<size_t>(address - 1)];
		if (value > slot)
			slot = value;
	}

	const uint8_t *data() const { return slots_.data(); }
	uint8_t *data() { return slots_.data(); }
	static constexpr size_t size() { return kSlotsPerUniverse; }

private:
	uint16_t id_;
	std::array<uint8_t, kSlotsPerUniverse> slots_{};
};

} // namespace obsdmx
