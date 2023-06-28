#pragma once

#include <string>

#include "Engone/Typedefs.h"

// TODO: Thread safety
// TODO: Optimize string manipulation

// TODO: Get last modified date of a file
// TODO: Check if file exists.

#define Assert(expression) if(!(expression)) {fprintf(stderr,"[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__);*((char*)0) = 0;}

#define PL_PRINT_ERRORS

#define MegaBytes(x) (x*1024llu*1024llu)
#define GigaBytes(x) (x*1024llu*1024llu*1024llu)

#ifdef OS_LINUX
#define FORMAT_64 "%l"
#else
#define FORMAT_64 "%ll"
#endif

namespace engone {
	typedef void* APIFile;
	typedef void* RecursiveDirectoryIterator;
	struct DirectoryIteratorData{
		std::string name;
		uint64 fileSize;
		double lastWriteSeconds;
		bool isDirectory;
	};
	typedef uint64 TimePoint;
	
	// For debugging memory leaks
	void TrackType(uint64 bytes, const std::string& name);
	
	void* Allocate(uint64 bytes);
    void* Reallocate(void* ptr, uint64 oldBytes, uint64 newBytes);
	void Free(void* ptr, uint64 bytes);
	// These are thread safe
	// Successful calls to Allocate
	uint64 GetTotalNumberAllocations();
	// Bytes from successful calls to Allocate
	uint64 GetTotalAllocatedBytes();
	// Currently allocated bytes
	uint64 GetAllocatedBytes();
	// Current allocations
	uint64 GetNumberAllocations();
	
	void PrintRemainingTrackTypes();
	
	TimePoint MeasureTime();
	// returns time in seconds
	double StopMeasure(TimePoint timePoint);
    
    // Note that the platform/os may not have the accuracy you need.
    void Sleep(double seconds);
	
	enum FileFlag : uint32{
		FILE_NO_FLAG=0,
		FILE_ONLY_READ=1,
		FILE_CAN_CREATE=2,
		FILE_WILL_CREATE=4,
	};

	// Returns 0 if function failed
    // canWrite = true -> WRITE and READ. False only READ.
	APIFile FileOpen(const std::string& path, uint64* outFileSize = nullptr, uint32 flags = FILE_NO_FLAG);
	// Returns number of bytes read
	// -1 means error with read
	uint64 FileRead(APIFile file, void* buffer, uint64 readBytes);
	// @return Number of bytes written. -1 indicates an error
	uint64 FileWrite(APIFile file, const void* buffer, uint64 writeBytes);
	// @return True if successful, false if not
	// position as -1 will move the head to end of file.
	bool FileSetHead(APIFile file, uint64 position);
	void FileClose(APIFile file);
    
    bool FileExist(const std::string& path);
    bool DirectoryExist(const std::string& path);
	bool DirectoryCreate(const std::string& path);
    bool FileLastWriteSeconds(const std::string& path, double* seconds);
    
	// Not thread safe if you change working directory or if used within shared libraries. (from windows docs)
	std::string GetWorkingDirectory();

	std::string EnvironmentVariable(const std::string& name);
	
	bool FileCopy(const std::string& src, const std::string& dst);
    
	// 0 is returned on failure (invalid path, empty directory)
	// result must be valid memory.
	// path should be a valid directory.
	// empty string as path assumes working directory.
	RecursiveDirectoryIterator RecursiveDirectoryIteratorCreate(const std::string& path);
	// false is returned on failure (invalid iterator, no more files/directories)
	bool RecursiveDirectoryIteratorNext(RecursiveDirectoryIterator iterator, DirectoryIteratorData* result);
	// Skip the latest directory in the internal recursive queue. The latest found directory will be last in the queue.
	void RecursiveDirectoryIteratorSkip(RecursiveDirectoryIterator iterator);
	void RecursiveDirectoryIteratorDestroy(RecursiveDirectoryIterator iterator);
    
	void SetConsoleColor(uint16 color);

	struct PlatformError{
		uint32 errorType;
		std::string message; // fixed size?
	};
	bool PollError(PlatformError* out);
	void TestPlatformErrors();

    typedef uint32 ThreadId;
	class Thread {
	public:
		Thread() = default;
		~Thread(){cleanup();}
		// The thread itself should not call this function
		void cleanup();
		
		void init(uint32(*func)(void*), void* arg);
		void join();
		
        // True: Thread is doing stuff or finished and waiting to be joined.
        // False: Thread is not linked to an os thread. Call init to start the thread.
		bool isActive();
		bool joinable();

		static ThreadId GetThisThreadId();

		ThreadId getId();

	private:
		void* m_internalHandle = 0;
		ThreadId m_threadId=0;
		
		friend class FileMonitor;
	};
    class Semaphore {
	public:
		Semaphore(int initial=1, int maxLocks=1);
		~Semaphore() {cleanup();}
		void cleanup();

		void wait();
		// count is how much to increase the semaphore's count by.
		void signal(int count=1);

	private:
		void* m_internalHandle = 0;
		uint32 m_initial = 1;
		uint32 m_max = 1;
	};
	class Mutex {
	public:
		Mutex() = default;
		~Mutex(){cleanup();}
		void cleanup();

		void lock();
		void unlock();

		ThreadId getOwner();
	private:
		ThreadId m_ownerThread = 0;
		void* m_internalHandle = 0;
	};
	
	#define PROGRAM_NEW_CONSOLE 1
	#define PROGRAM_WAIT 2
	// Starts an exe at path. Uses CreateProcess from windows.h
	// commandLine cannot be constant (CreateProcessA in windows api says so)
	bool StartProgram(const std::string& path, char* commandLine=NULL, int flags=0, int* exitCode=0, APIFile inPipe=0, APIFile outPipe=0);

	APIFile PipeCreate(bool inheritRead, bool inheritWrite);
	void PipeDestroy(APIFile pipe);
	int PipeRead(APIFile pipe,void* buffer, int size);
	int PipeWrite(APIFile pipe,void* buffer, int size);

	typedef void(*VoidFunction)();
	// @return null on error (library not found?). Pass returned value into GetFunctionAdress to get function pointer. 
	void* LoadDynamicLibrary(const std::string& path);
	void UnloadDynamicLibrary(void* library);
	// You may need to cast the function pointer to the appropriate function
	VoidFunction GetFunctionPointer(void* library, const std::string& name);

	// Converts arguments from WinMain into simpler arguments. Not unicode.
	// note that argc and argv are references and the outputs of this function.
	// do not forget to call FreeArguments because this function allocates memory.
	void ConvertArguments(int& argc, char**& argv);
	// same as previous but arguments are converted from paramteter args.
	// args is what would follow when you call the executable. Ex, mygame.exe --console --server
	void ConvertArguments(const char* args, int& argc, char**& argv);
	void FreeArguments(int argc, char** argv);
	// calls AllocConsole and sets stdin and stdout
	void CreateConsole();

	// Monitor a directory or file where any changes to files will call the callback with certain path.
	class FileMonitor {
	public:
		//-- flags
		enum Flags : uint32 {
			WATCH_SUBTREE = 1,
		};
		enum ChangeType : uint32 {
			FILE_REMOVED = 1,
			FILE_MODIFIED = 2, // includes added file
		};

		FileMonitor() = default;
		~FileMonitor();
		void cleanup();

		// path can be file or directory
		// calling this again will restart the tracking with new arguments.
		// the argument in the callback is a relative path from root to the file that was changed.
		// returns false if root path was invalid
		bool check(const std::string& root, void (*callback)(const std::string&, uint32), uint32 flags = 0);
		// bool check(const std::string& root, std::function<void(const std::string&, uint32)> callback, uint32 flags = 0);

		inline const std::string& getRoot() { return m_root; }

		inline void (*getCallback())(const std::string&, uint32) { return m_callback; }
		// inline std::function<void(const std::string&, uint32)>& getCallback() { return m_callback; }

	private:
		bool m_running = false;
		// std::function<void(const std::string&, uint32)> m_callback;
		void (*m_callback)(const std::string&, uint32);
		
		std::string m_root; // path passed to check function
		std::string m_dirPath; // if m_root isn't a directory then this will be the dir of the file
		uint32 m_flags = 0;

		void* m_changeHandle=NULL;
		Mutex m_mutex;
		Thread m_thread;
		//HANDLE m_threadHandle = NULL; // used to cancel ReadDirectoryChangesW
	
		void* m_dirHandle = NULL;
		void* m_buffer=nullptr;
		uint32 m_bufferSize=0;
	
		friend uint32 RunMonitor(void* arg);
	};
	
	// You set a threads name with it. Use -1 as threadId if you want to set the name for the current thread.
	// This only works in visual studios debugger.
	// An alternative is SetThreadDescription but it is not available in Windows 8.1 which I am using.
	void SetThreadName(ThreadId threadId, const char* threadName);
}