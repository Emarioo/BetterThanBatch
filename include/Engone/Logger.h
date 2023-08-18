#pragma once

#include "Engone/PlatformLayer.h"

#ifdef ENGONE_PHYSICS
#include "Engone/Utilities/rp3d.h"
#endif

#include "Engone/Alloc.h"
#include "Engone/Typedefs.h"

#include <unordered_map>
#include <vector>

namespace engone {
	#pragma once

	namespace log {
		enum Color : uint8 {
			BLACK = 0x00,
			NAVY = 0x01,
			GREEN = 0x02,
			CYAN = 0x03,
			BLOOD = 0x04,
			PURPLE = 0x05,
			GOLD = 0x06,
			SILVER = 0x07,
			GRAY = 0x08,
			BLUE = 0x09,
			LIME = 0x0A,
			AQUA = 0x0B,
			RED = 0x0C,
			MAGENTA = 0x0D,
			YELLOW = 0x0E,
			WHITE = 0x0F,
			_BLACK = 0x00,
			_BLUE = 0x10,
			_GREEN = 0x20,
			_TEAL = 0x30,
			_BLOOD = 0x40,
			_PURPLE = 0x50,
			_GOLD = 0x60,
			_SILVER = 0x70,
			_GRAY = 0x80,
			_NAVY = 0x90,
			_LIME = 0xA0,
			_AQUA = 0xB0,
			_RED = 0xC0,
			_MAGENTA = 0xD0,
			_YELLOW = 0xE0,
			_WHITE = 0xF0,
			NO_COLOR = 0x00,
		};
		enum Filter : uint8 {
			INFO = 1,
			WARN = 2,
			ERR = 4,
			FilterAll = INFO | WARN | ERR,
		};
    }
	class Logger {
	public:
		Logger() = default;
		~Logger() { cleanup(); }
		void cleanup();

		Logger(const Logger&) = delete;
		Logger& operator =(const Logger&) = delete;

		// master report is always written to
		void enableReport(bool yes = true);
		void enableConsole(bool yes = true);
		// individual for each thread
		void setMasterColor(log::Color color);
		// all threads write to this report
		void setMasterReport(const std::string path);
		// extra report, each thread, set empty path for no report which is default.
		void setReport(const std::string path);
		void setIndent(int indent);

		// extra report which is individual for each thread
		void useThreadReports(bool yes);

		void flushInternal();
		
		void flush();

		uint64 getMemoryUsage();

		// Returns master color if it is set otherwise normal color.
		log::Color getColor();
		Logger& operator<<(log::Color value);
		Logger& operator<<(log::Filter value);

		Logger& operator<<(const char* value);
		Logger& operator<<(char* value); // treated as a string
		Logger& operator<<(char value);
		Logger& operator<<(const std::string& value);

		Logger& operator<<(void* value);
		Logger& operator<<(int64 value);
		Logger& operator<<(int32 value);
		Logger& operator<<(int16 value);
		//Logger& operator<<(int8 value); // interpreted as a character not a number.
		
		Logger& operator<<(uint64 value);
		Logger& operator<<(uint32 value);
		Logger& operator<<(uint16 value);
		Logger& operator<<(uint8 value);

		Logger& operator<<(double value);
		Logger& operator<<(float value);

#ifdef ENGONE_GLM
		Logger& operator<<(const glm::vec3& value);
		Logger& operator<<(const glm::vec4& value);
		Logger& operator<<(const glm::quat& value);
		Logger& operator<<(const glm::mat4& value);
#endif
#ifdef ENGONE_PHYSICS
		Logger& operator<<(const rp3d::Vector3& value);
		Logger& operator<<(const rp3d::Quaternion& value);
#endif

		void print(char* str, int length);
		
		Logger& TIME();

	private:
		Mutex m_printMutex;
		
		struct ThreadInfo {
			log::Color color = log::SILVER;
			log::Filter filter = log::FilterAll;

			std::string logReport;

			Memory<char> lineBuffer{};

			// ensure free space
			char* ensure(uint32 bytes);
			// use some of ensured space
			void use(uint32 bytes);
		};
		ThreadInfo& getThreadInfo();

		log::Color m_masterColor = log::NO_COLOR;
		std::string m_masterReportPath="master.txt";
		bool m_useThreadReports = false;
		bool m_enabledConsole = true;
		bool m_enabledReports = true;
		int m_indent = 0;
		int m_nextIndent = 0;
		bool skipIndent=false;
		char lastPrintedChar=0;
		bool onEmptyLine=false;

		std::string m_rootDirectory = "logs";

		std::unordered_map<std::string, APIFile> m_logFiles;
		
		std::unordered_map<ThreadId, ThreadInfo> m_threadInfos;
	};
	namespace log {
		extern Logger out;
	}
}