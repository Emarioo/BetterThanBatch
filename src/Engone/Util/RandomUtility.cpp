#include "Engone/Util/RandomUtility.h"

#include "Engone/Logger.h"
#include "Engone/PlatformLayer.h"

#include <random>

namespace engone {
	static std::mt19937 randomGenerator;
	static u32 randomSeed = 0;
	static bool seedonce = false;
	static void initGenerator() {
		if (!seedonce) {
			SetRandomSeed((uint32_t)StartMeasure());
			// SetRandomSeed((uint32_t)GetSystemTime());
		}
	}
	static std::uniform_real_distribution<double> random_standard(0.0, 1.0);
	static std::uniform_int_distribution<uint32_t> random_32(0, -1);
	void SetRandomSeed(uint32_t seed) {
		seedonce = true;
		randomSeed = seed;
		randomGenerator.seed(seed);
	}
	uint32_t GetRandomSeed() {
		return randomSeed;
	}
	double GetRandom() {
		initGenerator();
		return random_standard(randomGenerator);
	}
	uint32_t Random32() {
		initGenerator();
		return random_32(randomGenerator);
	}
	uint64_t Random64() {
		initGenerator();
		return ((uint64_t)random_32(randomGenerator) << 32)
			| (uint64_t)random_32(randomGenerator);
	}
	uint8_t HexToNum(char hex) {
		if (hex >= '0' && hex <= '9') return hex - '0';
		if (hex >= 'a' && hex <= 'f') return 10+hex - 'a';
		return 10+hex - 'A';
	}
	uint8_t HexToNum(char hex0,char hex1) {
		return HexToNum(hex0)<<4 | HexToNum(hex1);
	}
	char NumToHex(uint8_t num, bool lowerCase) {
		if (num < 10) return '0' + num;
		if(lowerCase)
			return 'a' + num - 10;
		return 'A' + num - 10;
	}
	// UUID UUID::New() {
	// 	uint64_t out[2];
	// 	for (int i = 0; i < 2; i++) {
	// 		out[i] = Random64();
	// 	}
	// 	return *(UUID*)&out;
	// }
	// UUID::UUID(const int num) {
	// 	// Uncomment debugbreak, run program, check stackframe.
	// 	//if (num != 0) DebugBreak();
	// 	assert(("UUID was created from non 0 int which is not allowed. Put a breakpoint here and check stackframe.", num == 0));
	// 	memset(this, 0, sizeof(UUID));
	// }
// 	UUID::UUID(const char* str){
// 		int len = strlen(str);
// 		if (len != 36) {
// 			log::out <<log::RED<< "UUID::UUID - string cannot be converted\n";
// 			return;
// 		}
// 		int index = 0;
// 		uint8_t* ut = (uint8_t*)data;
// 		for (int i = 0; i < len; i++) {
// 			if (str[i] == '-') continue;
// 			uint8_t num = HexToNum(str[i]);

// 			if(index%2==0)
// 				ut[index / 2]=num;
// 			else
// 				ut[index / 2]|=num<<4;

// 			index++;
// 		}
// 	}
// 	bool UUID::operator==(const UUID& uuid) const {
// 		for (int i = 0; i < 2; i++) {
// 			if (data[i] != uuid.data[i])
// 				return false;
// 		}
// 		return true;
// 	}
// 	bool UUID::operator!=(const UUID& uuid) const {
// 		return !(*this == uuid);
// 	}
// 	std::string UUID::toString(bool fullVersion) const {
// 		char out[2 * sizeof(UUID) + 4 + 1]; // maximum hash string, four dashes, 1 null

// 		uint8_t* bytes = (uint8_t*)this;

// 		int write = 0;
// 		for (int i = 0; i < sizeof(UUID); i++) {
// 			if (i == sizeof(UUID) / 2 && !fullVersion)
// 				break;
// 			if (i == 4 || i == 6 || i == 8 || i == 10)
// 				out[write++] = '-';
// 			out[write++] = NumToHex(bytes[i] & 15,true);
// 			out[write++] = NumToHex(bytes[i] >> 4,true);
// 		}
// 		out[write] = 0; // finish with null terminated char
// 		return out;
// 	}
// #ifdef ENGONE_LOGGER
// 	Logger& operator<<(Logger& log, UUID value) {
// 		return log << value.toString();
// 	}
// #endif
}