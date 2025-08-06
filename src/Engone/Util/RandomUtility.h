#pragma once
// requires std::hash which is included in precompiled header

#include "Engone/Logger.h"

namespace engone {
	// A random seet is set by default.
	void SetRandomSeed(uint32_t seed);
	uint32_t GetRandomSeed();
	// between 0 and 1
	double GetRandom();
	uint32_t Random32();
	uint64_t Random64();
	uint8_t HexToNum(char hex);
	uint8_t HexToNum(char hex0, char hex1);
	char NumToHex(uint8_t num, bool lowerCase=false);
	// random uuid of 16 bytes in total, use UUID::New to create one.
	// class UUID {
	// public:
	// 	// memory is not initialized.
	// 	UUID() = default;
	// 	UUID(uint64_t data0, uint64_t data1) { data[0] = data0; data[1] = data1; }
	// 	UUID(const char* str);
	// 	static UUID New();
	// 	bool operator==(const UUID& uuid) const;
	// 	bool operator!=(const UUID& uuid) const;

	// 	// only intended for 0.
	// 	UUID(const int zero);

	// 	// fullVersion as true will return the whole 16 bytes
	// 	// otherwise the first 8 will be returned
	// 	std::string toString(bool fullVersion = false) const;

	// 	uint64_t data[2];
	// private:

	// 	friend struct std::hash<engone::UUID>;
	// };
// #ifdef ENGONE_LOGGER
	// Logger& operator<<(Logger& log, UUID value);
// #endif
}
// template<>
// struct std::hash<engone::UUID> {
// 	std::size_t operator()(const engone::UUID& u) const {
// 		return std::hash<uint64_t>{}(u.data[0]) ^ (std::hash<uint64_t>{}(u.data[1]));
// 	}
// };