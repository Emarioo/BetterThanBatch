#pragma once

#include <string>

#include "Engone/Typedefs.h"

// TODO: Thread safety
// TODO: Optimize string manipulation

// TODO: Get last modified date of a file
// TODO: Check if file exists.

// #define Assert(expression) if(!(expression)) {fprintf(stderr,"[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__);*((char*)0) = 0;}
// #define Assert(expression) ((expression) ? true : (fprintf(stderr,"[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))

// You can only disable this if you don't use any important code in the expression.
// Like a resize or something.
// #define Assert(expression)

#define PL_PRINT_ERRORS

#define MegaBytes(x) (x*1024llu*1024llu)
#define GigaBytes(x) (x*1024llu*1024llu*1024llu)

#ifdef OS_UNIX
#define FORMAT_64 "%l"
#else
#define FORMAT_64 "%ll"
#endif

namespace engone {
	struct APIFile {
		u64 internal=0;
		operator bool() {
			return internal;
		}
		operator i32() = delete;
	};
	struct APIPipe {
		u64 internal=0;
		operator bool() {
			return internal;
		}
		operator i32() = delete;
	};
	typedef void* DirectoryIterator;
	// struct String {
	// 	char* ptr;
	// 	u32 len;
	// 	u32 max;
	// 	bool _reserve();
	// };
	struct DirectoryIteratorData{
		struct {
			char* name = nullptr;
			u64 namelen = 0;
		};
		u64 fileSize = 0;
		u64 lastModified = 0;
		float lastWriteSeconds = 0.0;
		bool isDirectory = false;
	};
	typedef u64 TimePoint;
	
	// For debugging memory leaks
	void TrackType(u64 bytes, const std::string& name);
	void SetTracker(bool on);

	void* Allocate(u64 bytes);
    void* Reallocate(void* ptr, u64 oldBytes, u64 newBytes);
	void Free(void* ptr, u64 bytes);
	// These are thread safe
	// Successful calls to Allocate
	u64 GetTotalNumberAllocations();
	// Bytes from successful calls to Allocate
	u64 GetTotalAllocatedBytes();
	// Currently allocated bytes
	u64 GetAllocatedBytes();
	// Current allocations
	u64 GetNumberAllocations();
	
	void PrintRemainingTrackTypes();
	
	TimePoint StartMeasure();
	// returns time in seconds
	double StopMeasure(TimePoint startPoint);
	// example: DiffMeasure(StartMeasure() - StartMeasure())
	double DiffMeasure(TimePoint endSubStart);
	// Returns value in Hz
	u64 GetClockSpeed();
    
    // Note that the platform/os may not have the accuracy you need.
    void Sleep(double seconds);
	
	enum FileOpenFlags : u32 {
		FILE_READ_ONLY	 		= 0x1,
		FILE_CLEAR_AND_WRITE 	= 0x2,
		FILE_READ_AND_WRITE 	= 0x4,
		// FILE_SHARE_READ = 8, // always true
		// FILE_SHARE_WRITE = 16,
	};

	// Returns 0 if function failed
    // canWrite = true -> WRITE and READ. False only READ.
	APIFile FileOpen(const std::string& path, FileOpenFlags flags, u64* outFileSize = nullptr);
	APIFile FileOpen(const char* path, u32 pathlen, FileOpenFlags flags, u64* outFileSize = nullptr);
	// Returns number of bytes read
	// -1 means error with read
	u64 FileRead(APIFile file, void* buffer, u64 readBytes);
	// @return Number of bytes written. -1 indicates an error
	u64 FileWrite(APIFile file, const void* buffer, u64 writeBytes);
	// @return True if successful, false if not.
	// @param position as -1 will move the head to end of file.
	bool FileSetHead(APIFile file, u64 position);
	u64 FileGetHead(APIFile file);
	u64 FileGetSize(APIFile file);
	void FileClose(APIFile file);
	bool FileFlushBuffers(APIFile file);
    
    bool FileExist(const std::string& path);
    bool DirectoryExist(const std::string& path);
	bool DirectoryCreate(const std::string& path);
    bool FileLastWriteSeconds(const std::string& path, double* seconds);
    
	// Not thread safe if you change working directory or if used within shared libraries. (from windows docs)
	// Backslashes are converted to forward slashes. Every function in platform layer uses forward slashes but
	// converts to backslashes if the operating system requires it.
	std::string GetWorkingDirectory();
	bool SetWorkingDirectory(const std::string& path);

	std::string EnvironmentVariable(const std::string& name);
	
	bool FileCopy(const std::string& src, const std::string& dst);
	bool FileMove(const std::string& src, const std::string& dst);
	bool FileDelete(const std::string& path);
    
	// 0 is returned on failure (invalid path, empty directory)
	// result must be valid memory.
	// path should be a valid directory.
	// empty string as path assumes working directory.
	DirectoryIterator DirectoryIteratorCreate(const char* path, int pathlen);
	// false is returned on failure (invalid iterator, no more files/directories)
	// result points to some data where the result of the iteration is stored. Don't forget to destroy potential
	// allocations in the result wit ...IteratorDestroy.
	bool DirectoryIteratorNext(DirectoryIterator iterator, DirectoryIteratorData* result);
	// Skip the latest directory in the internal recursive queue. The latest found directory will be last in the queue.
	void DirectoryIteratorSkip(DirectoryIterator iterator);
	void DirectoryIteratorDestroy(DirectoryIterator iterator, DirectoryIteratorData* dataToDestroy);
    
	void SetConsoleColor(uint16 color);
	// returns 0 if something went wrong
	int GetConsoleWidth();

	struct PlatformError{
		uint32 errorType;
		std::string message; // fixed size?
	};
	bool PollError(PlatformError* out);
	void TestPlatformErrors();

    typedef u64 ThreadId;
    typedef u32 TLSIndex;
	class Thread {
	public:
		Thread() = default;
		~Thread(){ cleanup(); }
		// The thread itself should not call this function
		void cleanup();
		
		void init(uint32(*func)(void*), void* arg);
		// joins and destroys the thread
		void join();
		
        // True: Thread is doing stuff or finished and waiting to be joined.
        // False: Thread is not linked to an os thread. Call init to start the thread.
		bool isActive();
		bool joinable();
		ThreadId getId();

		static ThreadId GetThisThreadId();

		// Thread local storage (or thread specific data)
		// 0 indicates failure
		static TLSIndex CreateTLSIndex();
		static bool DestroyTLSIndex(TLSIndex index);

		static void* GetTLSValue(TLSIndex index);
		static bool SetTLSValue(TLSIndex index, void* ptr);


	private:
		static const int THREAD_SIZE = 8;
		union {
			struct { // Windows
				u64 m_internalHandle;
			};
			struct { // Unix
				u64 m_data;
			};
            u64 _zeros = 0;
		};
		ThreadId m_threadId=0;
		
		friend class FileMonitor;
	};
    class Semaphore {
	public:
		Semaphore() = default;
		Semaphore(u32 initial, u32 maxLocks);
		~Semaphore() { cleanup(); }
		void cleanup();

		void init(u32 initial, u32 maxLocks);

		void wait();
		// count is how much to increase the semaphore's count by.
		void signal(int count=1);

	private:
		u32 m_initial = 1;
		u32 m_max = 1;
		static const int SEM_SIZE = 32;
		union {
			struct { // Windows
				u64 m_internalHandle;
			};
			struct { // Unix
				u8 m_data[SEM_SIZE]; // sem_t
				bool m_initialized;
			};
            u8 _zeros[SEM_SIZE+1]{0}; // intelisense doesn't like multiple initialized fields even though they are in anonmymous struct, sigh. So we use one field here.
		};
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
		u64 m_internalHandle = 0;
	};
	
	#define PROGRAM_NEW_CONSOLE 1
	#define PROGRAM_WAIT 2
	// PROGRAM_ASYNC or something instead? you usually want to wait? or not?

	bool ExecuteCommand(const std::string& cmd, bool asynchronous = false, int* exitCode = nullptr);

	// Starts an exe at path. Uses CreateProcess from windows.h
	// commandLine cannot be constant (CreateProcessA in windows api says so)
	// exitCode is ignored unless you wait for the process to finish.
	// TODO: Implement APIProcess so that you can get do some stuff in the mean time and then wait
	//   for the process to finishd and get the exitCode.
	bool StartProgram(char* commandLine, u32 flags=0, int* exitCode=0, APIFile inFile={}, APIFile outFile={}, APIFile errFile={});

	APIPipe PipeCreate(u64 pipeBuffer, bool inheritRead, bool inheritWrite);
	void PipeDestroy(APIPipe pipe);
	u64 PipeRead(APIPipe pipe,void* buffer, u64 size);
	u64 PipeWrite(APIPipe pipe,void* buffer, u64 size);
	APIFile PipeGetRead(APIPipe pipe);
	APIFile PipeGetWrite(APIPipe pipe);

	bool SetStandardOut(APIFile file);
	APIFile GetStandardOut();
	
	bool SetStandardErr(APIFile file);
	APIFile GetStandardErr();
	
	bool SetStandardIn(APIFile file);
	APIFile GetStandardIn();

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

	// these should be intrinsics
	// bool compare_swap(i32* ptr, i32 oldValue, i32 newValue);
	// void atomic_add(i32* ptr, i32 value);
	// #define compare_swap(ptr, oldValue, newValue)
	// #define atomic_add
	
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