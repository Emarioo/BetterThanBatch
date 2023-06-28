#include "Engone/PlatformLayer.h"

// Todo: Compile for Linux and test the functions
#ifdef OS_LINUX

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#include <unordered_map>
#include <vector>
#include <string>

#ifdef PL_PRINT_ERRORS
#define PL_PRINTF printf
#else
#define PL_PRINTF
#endif

#include "BetBat/Util/Perf.h"

namespace engone {
    
#define TO_INTERNAL(X) (APIFile)((uint64)X+1)
#define TO_HANDLE(X) (int)((uint64)X-1)

#define NS 1000000000
	TimePoint MeasureTime() {
		timespec ts;
		int res = clock_gettime(CLOCK_MONOTONIC,&ts);
		if (res == -1) {
			// err
		}
		return ((uint64)ts.tv_sec*NS + (uint64)ts.tv_nsec);
	}
	double StopMeasure(TimePoint startPoint) {
		timespec ts;
		int res = clock_gettime(CLOCK_MONOTONIC, &ts);
		if (res == -1) {
			// err
		}
		return (double)(((uint64)ts.tv_sec * NS + (uint64)ts.tv_nsec) - startPoint)/(double)NS;
	}
	APIFile FileOpen(const std::string& path, uint64* outFileSize, uint32 flags) {
        int fileFlags = O_RDWR;
        int mode = 0;
        if(flags&FILE_ONLY_READ){
            fileFlags = O_RDONLY;
		}
		if(flags&FILE_CAN_CREATE) {
			fileFlags = O_CREAT | O_RDWR;
            mode = S_IRUSR | S_IWUSR;
        }
		if(flags&FILE_WILL_CREATE){
			fileFlags = O_CREAT | O_TRUNC | O_RDWR;
			mode = S_IRUSR | S_IWUSR;
		}
        // Assert(("Not implemented for linux",0 == (flags&FILE_WILL_CREATE)));
        
		// if(creation&OPEN_ALWAYS||creation&CREATE_ALWAYS){
		// 	std::string temp;
		// 	uint i=0;
		// 	int at = path.find_first_of(':');
		// 	if(at!=-1){
		// 		i = at+1;
		// 		temp+=path.substr(0,i);
		// 	}
		// 	for(;i<path.length();i++){
		// 		char chr = path[i];
		// 		if(chr=='/'||chr=='\\'){
		// 			// printf("exist %s\n",temp.c_str());
		// 			if(!DirectoryExist(temp)){
		// 				// printf(" create\n");
		// 				bool success = DirectoryCreate(temp);
		// 				if(!success)
		// 					break;
		// 			}
		// 		}
		// 		temp+=chr;
		// 	}
		// }
        // printf("OPENING\n");
        int fd = open(path.c_str(), fileFlags, mode);
		if (fd == -1) {
			printf("[LinuxError %d]\n", errno);
			return 0;
		}
        if(outFileSize) {
            off_t size = lseek(fd,0,SEEK_END);
            if (size == -1) {
                printf("[LinuxError %d]\n", errno);
                size = 0;
            }
			lseek(fd,0,SEEK_SET);
            *outFileSize = (uint64)size;
        }
		return TO_INTERNAL(fd);
	}
    uint64 FileWrite(APIFile file, const void* buffer, uint64 writeBytes){
		Assert(writeBytes!=(uint64)-1); // -1 indicates no bytes read
		
		int bytesWritten = write(TO_HANDLE(file),buffer,writeBytes);
		if(bytesWritten == -1){
			// DWORD err = GetLastError();
            int err = 0;
			PL_PRINTF("[WinError %d] FileWrite '%lu'\n",err,(uint64)file);
			return -1;
		}
		return bytesWritten;
	}
	uint64 FileRead(APIFile file, void* buffer, uint64 readBytes) {
        // printf("REDAING\n");
        ssize_t size = read(TO_HANDLE(file), buffer, readBytes);
		// printf("fr size: %lu\n",size);
        if (size == -1) {
			printf("[LinuxError %d]\n", errno);
			return -1;
		}
		return size;
	}
	void FileClose(APIFile file) {
		if (file)
			close(TO_HANDLE(file));
	}
    bool FileExist(const std::string& path){
        struct stat buffer;   
        return (stat(path.c_str(), &buffer) == 0); 
    }
    std::string GetWorkingDirectory(){
        std::string path{};
        path.resize(256);
        char* buf = nullptr;
        if(!(buf = getcwd((char*)path.data(), path.length())))
            return "";
        
        int len = strlen(buf);
        path.resize(len);

        return path;
	}
    struct AllocInfo {
		std::string name;
		int count;
	};
	static std::unordered_map<uint64, AllocInfo> allocTracking;
    // TODO: TrackType and PrintTracking is the same on Linux and Windows.
    // This will be a problem if code is changed in one file but not in the other.
	#define ENGONE_TRACK_ALLOC 0
	#define ENGONE_TRACK_FREE 1
	#define ENGONE_TRACK_REALLOC 2
    void TrackType(uint64 bytes, const std::string& name){
		auto pair = allocTracking.find(bytes);
		if(pair==allocTracking.end()){
			allocTracking[bytes] = {name,0};	
		} else {
			pair->second.name += "|";
			pair->second.name += name;
		}
	}
    void PrintTracking(uint64 bytes, int type){
		auto pair = allocTracking.find(bytes);
			
		if(pair!=allocTracking.end()){
			if(type==ENGONE_TRACK_ALLOC)
				pair->second.count++;
			else if(type==ENGONE_TRACK_FREE)
				pair->second.count--;
			// else if(type==ENGONE_TRACK_REALLOC)
			// 	pair->second.count--;
			printf("%s %s (%d left)\n",type==ENGONE_TRACK_ALLOC?"alloc":(type==ENGONE_TRACK_FREE?"free" : "realloc"), pair->second.name.c_str(),pair->second.count);
		}
	}
    void PrintRemainingTrackTypes(){
		for(auto& pair : allocTracking){
			if(pair.second.count!=0)
				printf(" %s (%lu bytes): %d left\n",pair.second.name.c_str(),pair.first,pair.second.count);	
		}
	}
	// static std::mutex s_allocStatsMutex;
	static uint64 s_totalAllocatedBytes=0;
	static uint64 s_totalNumberAllocations=0;
	static uint64 s_allocatedBytes=0;
	static uint64 s_numberAllocations=0;
	void* Allocate(uint64 bytes){
		if(bytes==0) return nullptr;
		MEASURE;
		// void* ptr = HeapAlloc(GetProcessHeap(),0,bytes);
        void* ptr = malloc(bytes);
		if(!ptr) return nullptr;
		
		PrintTracking(bytes,ENGONE_TRACK_ALLOC);
		
		// s_allocStatsMutex.lock();
		s_allocatedBytes+=bytes;
		s_numberAllocations++;
		s_totalAllocatedBytes+=bytes;
		s_totalNumberAllocations++;			
		// s_allocStatsMutex.unlock();
		#ifdef LOG_ALLOCATIONS
		printf("* Allocate %ld\n",bytes);
		#endif
		
		return ptr;
	}
    void* Reallocate(void* ptr, uint64 oldBytes, uint64 newBytes){
		MEASURE;
        if(newBytes==0){
            Free(ptr,oldBytes);
            return nullptr;   
        }else{
            if(ptr==0){
                return Allocate(newBytes);   
            }else{
                if(oldBytes==0){
                    PL_PRINTF("Reallocate : oldBytes is zero while the ptr isn't!?\n");   
                }
                // void* newPtr = HeapReAlloc(GetProcessHeap(),0,ptr,newBytes);
                void* newPtr = realloc(ptr,newBytes);
                if(!newPtr)
                    return nullptr;
                
				if(allocTracking.size()!=0)
					printf("%s %lu -> %lu\n","realloc", oldBytes, newBytes);
				// PrintTracking(newBytes,ENGONE_TRACK_REALLOC);
                // s_allocStatsMutex.lock();
                s_allocatedBytes+=newBytes-oldBytes;
                
                s_totalAllocatedBytes+=newBytes;
                s_totalNumberAllocations++;			
                // s_allocStatsMutex.unlock();
				#ifdef LOG_ALLOCATIONS
				printf("* Reallocate %ld -> %ld\n",oldBytes, newBytes);
				#endif
                return newPtr;
            }
        }
    }
	void Free(void* ptr, uint64 bytes){
		if(!ptr) return;
		MEASURE;
		free(ptr);
		// HeapFree(GetProcessHeap(),0,ptr);
		PrintTracking(bytes,ENGONE_TRACK_FREE);
		// s_allocStatsMutex.lock();
		s_allocatedBytes-=bytes;
		s_numberAllocations--;
		Assert(s_allocatedBytes>=0);
		Assert(s_numberAllocations>=0);
		// s_allocStatsMutex.unlock();
		#ifdef LOG_ALLOCATIONS
		printf("* Free %ld\n",bytes);
		#endif
	}
	uint64 GetTotalAllocatedBytes() {
		return s_totalAllocatedBytes;
	}
	uint64 GetTotalNumberAllocations() {
		return s_totalNumberAllocations;
	}
	uint64 GetAllocatedBytes() {
		return s_allocatedBytes;
	}
	uint64 GetNumberAllocations() {
		return s_numberAllocations;
	}
    void SetConsoleColor(uint16 color){
        u32 fore = color&0x0F;
		u32 foreExtra = 0;
        u32 back = color&0xF0;
        switch (fore){
            #undef CASE
            #define CASE(A,B) case 0x##A: fore = B; break;
            #define CASE2(A,B) case 0x##A: fore = B; foreExtra = 1; break;
            CASE(00,30) /* BLACK    = 0x00, */ 
                CASE2(01,34) /* NAVY     = 0x01, */
            CASE(02,32) /* GREEN    = 0x02, */
            CASE(03,36) /* CYAN     = 0x03, */
                CASE2(04,31) /* BLOOD    = 0x04, */
                CASE(05,35) /* PURPLE   = 0x05, */
                CASE2(06,33) /* GOLD     = 0x06, */
                CASE(07,37) /* SILVER   = 0x07, */
                CASE2(08,30) /* GRAY     = 0x08, */
            CASE(09,34) /* BLUE     = 0x09, */
                CASE2(0A,32) /* LIME     = 0x0A, */
                CASE2(0B,36) /* AQUA     = 0x0B, */
            CASE(0C,31) /* RED      = 0x0C, */
            CASE2(0D,35) /* MAGENTA  = 0x0D, */
            CASE(0E,33) /* YELLOW   = 0x0E, */
            CASE2(0F,37) /* WHITE    = 0x0F, */
        /*
            black        30         40
            red          31         41
            green        32         42
            yellow       33         43
            blue         34         44
            magenta      35         45
            cyan         36         46
            white        37         47
        */
	    /*
			reset             0  (everything back to normal)
			bold/bright       1  (often a brighter shade of the same colour)
			underline         4
			inverse           7  (swap foreground and background colours)
			bold/bright off  21
			underline off    24
			inverse off      27
	    */
            #undef CASE
        }
		/*	
		#define PCOLOR(X) log::out << log::X << #X "\n";
		PCOLOR(WHITE)
		PCOLOR(SILVER)
		PCOLOR(GRAY)
		PCOLOR(BLACK)
		PCOLOR(GOLD)
		PCOLOR(YELLOW)
		PCOLOR(BLOOD)
		PCOLOR(RED)
		PCOLOR(BLUE)
		PCOLOR(NAVY)
		PCOLOR(AQUA)
		PCOLOR(CYAN)
		PCOLOR(LIME)
		PCOLOR(GREEN)
		PCOLOR(MAGENTA)
		PCOLOR(PURPLE)
		*/

        printf("\033[%u;%um",foreExtra, fore);
	}
    void Mutex::cleanup() {
        // if (m_internalHandle != 0){
        //     BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
        //     if(!yes){
        //         DWORD err = GetLastError();
        //         PL_PRINTF("[WinError %lu] CloseHandle\n",err);
        //     }
		// 	m_internalHandle=0;
        // }
	}
	void Mutex::lock() {
		// if (m_internalHandle == 0) {
		// 	HANDLE handle = CreateMutex(NULL, false, NULL);
		// 	if (handle == INVALID_HANDLE_VALUE) {
		// 		DWORD err = GetLastError();
		// 		PL_PRINTF("[WinError %lu] CreateMutex\n",err);
		// 	}else
        //         m_internalHandle = TO_INTERNAL(handle);
		// }
		// if (m_internalHandle != 0) {
		// 	DWORD res = WaitForSingleObject(TO_HANDLE(m_internalHandle), INFINITE);
		// 	uint32 newId = Thread::GetThisThreadId();
		// 	if (m_ownerThread != 0) {
		// 		PL_PRINTF("Mutex : Locking twice, old owner: %u, new owner: %u\n",m_ownerThread,newId);
		// 	}
		// 	m_ownerThread = newId;
		// 	if (res == WAIT_FAILED) {
        //         // TODO: What happened do the old thread who locked the mutex. Was it okay to ownerThread = newId
		// 		DWORD err = GetLastError();
        //         PL_PRINTF("[WinError %lu] WaitForSingleObject\n",err);
		// 	}
		// }
	}
	void Mutex::unlock() {
		// if (m_internalHandle != 0) {
		// 	m_ownerThread = 0;
		// 	BOOL yes = ReleaseMutex(TO_HANDLE(m_internalHandle));
		// 	if (!yes) {
		// 		DWORD err = GetLastError();
        //         PL_PRINTF("[WinError %lu] ReleaseMutex\n",err);
		// 	}
		// }
	}
	uint32_t Mutex::getOwner() {
		return m_ownerThread;
	}
    void Thread::cleanup() {
		// if (m_internalHandle) {
		// 	if (m_threadId == GetThisThreadId()) {
        //         PL_PRINTF("Thread : Thread cannot clean itself\n");
		// 		return;
		// 	}
        //     BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
        //     if(!yes){
        //         DWORD err = GetLastError();
        //         PL_PRINTF("[WinError %lu] CloseHandle\n",err);
        //     }
		// 	m_internalHandle=0;
		// }
	}
	void Thread::init(uint32(*func)(void*), void* arg) {
		// if (!m_internalHandle) {
		// 	// const uint32 stackSize = 1024*1024;
		// 	HANDLE handle = CreateThread(NULL, 0, (DWORD(*)(void*))func, arg, 0,(DWORD*)&m_threadId);
		// 	if (handle==INVALID_HANDLE_VALUE) {
		// 		DWORD err = GetLastError();
        //         PL_PRINTF("[WinError %lu] CreateThread\n",err);
		// 	}else
        //         m_internalHandle = TO_INTERNAL(handle);
		// }
	}
	void Thread::join() {
		// if (!m_internalHandle)
		// 	return;
		// DWORD res = WaitForSingleObject(TO_HANDLE(m_internalHandle), INFINITE);
		// if (res==WAIT_FAILED) {
		// 	DWORD err = GetLastError();
        //     PL_PRINTF("[WinError %lu] WaitForSingleObject\n",err);
		// }
		// BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
        // if(!yes){
        //     DWORD err = GetLastError();
        //     PL_PRINTF("[WinError %lu] CloseHandle\n",err);
        // }
		m_threadId = 0;
		m_internalHandle = 0;
	}
	bool Thread::isActive() {
		return m_internalHandle!=0;
	}
	bool Thread::joinable() {
        return false;
		// return m_threadId!=0 && m_threadId != GetCurrentThreadId();
	}
	ThreadId Thread::getId() {
		return m_threadId;
	}
	ThreadId Thread::GetThisThreadId() {
        return 0;
		// return GetCurrentThreadId();
	}
	
}
#endif