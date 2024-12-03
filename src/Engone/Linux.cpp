#include "Engone/PlatformLayer.h"

#include "Engone/Asserts.h"

// Todo: Compile for Linux and test the functions
#ifdef OS_LINUX

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <sys/random.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include <sys/types.h>
#include <sys/ptrace.h>

#include <unordered_map>
#include <vector>
#include <string>


#ifdef PL_PRINT_ERRORS
#define PL_PRINTF(...) printf("[LinuxError %d] %s, ", errno, strerror(errno)); printf(__VA_ARGS__);
#else
#define PL_PRINTF(...)
#endif
#ifndef NATIVE_BUILD
#include "BetBat/Config.h"
#else
#define _LOG(...)
#endif
#include "Engone/Util/Array.h"
#include "BetBat/Util/Perf.h"

namespace engone {
    
#define TO_INTERNAL(X) ((u64)X+1)
#define TO_HANDLE(X) (int)((u64)X-1)

	// Recursive directory iterator info
	struct RDIInfo{
		std::string root;
		std::string dir;
		DIR* dirIter = nullptr;
		// HANDLE handle;
		DynamicArray<std::string> directories{};
		char* tempPtr = nullptr; // used in result
		u32 max = 0; // not including \0
	};
	static std::unordered_map<DirectoryIterator,RDIInfo> s_rdiInfos;
	static u64 s_uniqueRDI=0;
	
	DirectoryIterator DirectoryIteratorCreate(const char* name, int pathlen){
		DirectoryIterator iterator = (DirectoryIterator)(++s_uniqueRDI);
		auto& info = s_rdiInfos[iterator] = {};
		// if(pathlen == 0) {
		// 	info.root = "."; // CWD
		// } else {
			info.root.resize(pathlen);
			memcpy((char*)info.root.data(), name, pathlen);
		// }
		info.directories.add(info.root);

		
		// DWORD err = GetLastError();
		// PL_PRINTF("[WinError %lu] GetLastError '%llu'\n",err,(u64)iterator);
		
		// bool success = DirectoryIteratorNext(iterator,result);
		// if(!success){
		// 	DirectoryIteratorDestroy(iterator);
		// 	return 0;
		// }
        
		return iterator;
	}
	bool DirectoryIteratorNext(DirectoryIterator iterator, DirectoryIteratorData* result){
		auto info = s_rdiInfos.find(iterator);
		if(info==s_rdiInfos.end()){
			return false;
		}
        // printf("NEXT\n");

		struct stat statBuffer{};
		char filepath[256];
		int filepath_len = 0;

		while(true) {
			if(!info->second.dirIter){
				if(info->second.directories.size()==0){
					return false;
				}

                info->second.dir.clear();
				if(info->second.directories[0].size()!=0){
                    info->second.dir += info->second.directories[0];
				}
				info->second.directories.removeAt(0);
	

                // std::string temp = info->second.dir;
                // if(!temp.empty())
                //     temp += "\\";
                // temp+="*";
				// printf("FindFirstFile %s\n",temp.c_str());
				// TODO: This does a memcpy on the whole array (almost).
				//  That's gonna be slow!
				// fprintf(stderr, "%s %p\n", temp.c_str(), &data);
				
				// DWORD err = GetLastError();
				// PL_PRINTF("[WinError %lu] GetLastError '%llu'\n",err,(u64)iterator);
				if(!info->second.dir.empty())
					info->second.dirIter = opendir(info->second.dir.c_str());
				else {
					info->second.dirIter = opendir("./");
				}


				// HANDLE handle=INVALID_HANDLE_VALUE;
				// handle = FindFirstFileA(temp.c_str(),&data);
				// handle = FindFirstFileA("src\\*",&data);
				// fprintf(stderr, "WHY%s %p\n", temp.c_str(), &data);
				
				if(!info->second.dirIter){
					// DWORD err = GetLastError();
					// PL_PRINTF("[WinError %lu] FindNextFileA '%llu'\n",err,(u64)iterator);
					continue;
				}
			}
			
			dirent* entry = readdir(info->second.dirIter);
			if(!entry) {
				closedir(info->second.dirIter);
				info->second.dirIter = nullptr;
				continue;
			}
			if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
				continue;
			
			if(!info->second.dir.empty()) {
            	filepath_len = snprintf(filepath, sizeof(filepath), "%s/%s", info->second.dir.c_str(), entry->d_name);
			} else {
            	filepath_len = snprintf(filepath, sizeof(filepath), "%s", entry->d_name);
			}
			// printf("F %s\n",filepath);

			int err = stat(filepath, &statBuffer);
			if(err == -1) {
				// printf("err: %d\n",errno);
				continue;
			} else {
				// if(buf.st_mode & S_IFDIR) {
				// 	dirs.push_back(filepath);
				// } else {
				// 	printf("%s, %d\n",entry->d_name, buf.st_mode);
				// }
			}
			break;
		}

		int newLength = filepath_len;
		// if(!info->second.dir.empty())
		// 	newLength += info->second.dir.length()+1;

		if(!info->second.tempPtr) {
			info->second.max = 256;
			info->second.tempPtr = (char*)Allocate(info->second.max + 1);
		} else if(info->second.max < newLength) {
			u32 old = info->second.max;
			info->second.max = info->second.max + newLength;
			info->second.tempPtr = (char*)Reallocate(info->second.tempPtr, old + 1, info->second.max + 1);
		}
		result->name = info->second.tempPtr;
		result->namelen = newLength;
        // printf("np: %p\n",result->name);

		// if(!info->second.dir.empty()) {
		// 	memcpy(result->name, info->second.dir.c_str(), info->second.dir.length());
		// 	result->name[info->second.dir.length()] = '/';
		// 	memcpy(result->name + info->second.dir.length() + 1, data.cFileName, cFileNameLen);
		// } else {
			memcpy(result->name, filepath, newLength);
		// }
		result->name[result->namelen] = '\0';
		// result->name.clear();
		// if(!info->second.dir.empty())
		// 	result->name += info->second.dir+"/";
		// result->name += data.cFileName;
		
        // printf("f: %s\n",result->name);
		// for(int i=0;i<result->namelen.size();i++){
		// 	if(result->name[i]=='\\')
		// 		((char*)result->name.data())[i] = '/';
		// }

		result->fileSize = statBuffer.st_size;
		u64 time = statBuffer.st_mtime;
		result->lastWriteSeconds = time/10000000.f; // 100-nanosecond intervals
		result->isDirectory = statBuffer.st_mode & S_IFDIR;
		if(result->isDirectory){
            info->second.directories.add(result->name);
        }

		for(int i=0;i<result->namelen;i++){
			if(result->name[i]=='\\')
				result->name[i] = '/';
		}
		return true;
	}
	void DirectoryIteratorSkip(DirectoryIterator iterator){
		auto info = s_rdiInfos.find(iterator);
		if(info==s_rdiInfos.end()){
			return;
		}
		if(info->second.directories.size()!=0)
			info->second.directories.pop();
	}
	void DirectoryIteratorDestroy(DirectoryIterator iterator, DirectoryIteratorData* dataToDestroy){
		auto info = s_rdiInfos.find(iterator);
		if(info==s_rdiInfos.end()){
			return;
		}
		
		if(info->second.dirIter){
			closedir(info->second.dirIter);
			info->second.dirIter = nullptr;
		}
		if(info->second.tempPtr) {
			Free(info->second.tempPtr, info->second.max + 1);
			info->second.tempPtr = nullptr;
			info->second.max = 0;
		}
		if(dataToDestroy && dataToDestroy->name) {
			// Free(dataToDestroy->name, dataToDestroy->namelen + 1);
			dataToDestroy->name = nullptr;
			dataToDestroy->namelen = 0;
		}
		s_rdiInfos.erase(iterator);
	}

#define NS (u64)1000000000
	TimePoint StartMeasure() {
		struct timespec ts;
		memset(&ts,0,sizeof(ts));
		int res = clock_gettime(CLOCK_MONOTONIC,&ts);
		if (res == -1) {
			PL_PRINTF("StartMeasure");
		}
		return ((u64)ts.tv_sec*NS + (u64)ts.tv_nsec);
		// return ((u64)ts.tv_sec);
	}
	double StopMeasure(TimePoint startPoint) {
		struct timespec ts;
		memset(&ts,0,sizeof(ts));
		int res = clock_gettime(CLOCK_MONOTONIC, &ts);
		if (res == -1) {
			PL_PRINTF("StopMeasure");
		}
		// printf("%d\n",ts.tv_sec);
		// return (double)(((u64)ts.tv_sec) - startPoint);
		return (double)(((u64)ts.tv_sec * NS + (u64)ts.tv_nsec) - startPoint)/(double)NS;
	}
	double DiffMeasure(TimePoint endSubStart) {
		return (double)(endSubStart)/(double)NS;
	}
	void Sleep(double seconds){
		timespec tim = {};
		u64 ns = seconds * 1.0e9;
		tim.tv_sec = (u64)seconds;
		tim.tv_nsec = ns - (u64)(((u64)seconds) * 1.0e9);
		nanosleep(&tim, nullptr);
    }
	u64 GetClockSpeed(){
		// TODO: Don't hardcode the processors' frequency
		return 2800 * 1e6;
	}
	APIFile FileOpen(const char* path, u32 len, FileOpenFlags flags, u64* outFileSize){
		return FileOpen(std::string(path,len), flags, outFileSize);
	}
	APIFile FileOpen(const std::string& path, FileOpenFlags flags, u64* outFileSize) {
        int fileFlags = O_RDWR;
        int mode = 0;
        if(flags&FILE_READ_ONLY){
            fileFlags = O_RDONLY;
		}
		// if(flags&FILE_CAN_CREATE) {
		// 	fileFlags = O_CREAT | O_RDWR;
        //     mode = S_IRUSR | S_IWUSR;
        // }
		if(flags&FILE_CLEAR_AND_WRITE){
			fileFlags = O_CREAT | O_TRUNC | O_RDWR;
			mode = S_IRUSR | S_IWUSR;
		}
        // Assert(("Not implemented for linux",0 == (flags&FILE_CLEAR_AND_WRITE)));
        
		if(flags&FILE_CLEAR_AND_WRITE){
			std::string temp;
			uint i=0;
			int at = path.find_first_of(':');
			if(at!=-1){
				i = at+1;
				temp+=path.substr(0,i);
			}
			for(;i<path.length();i++){
				char chr = path[i];
				if(chr=='/'||chr=='\\'){
					// printf("exist %s\n",temp.c_str());
					if(!DirectoryExist(temp)){
						// printf(" create\n");
						bool success = DirectoryCreate(temp);
						if(!success)
							break;
					}
				}
				temp+=chr;
			}
		}
        // printf("OPENING\n");
        int fd = open(path.c_str(), fileFlags, mode);
		if (fd == -1) {
            PL_PRINTF("open %s\n", path.c_str())
			return {0};
		}
        if(outFileSize) {
            off_t size = lseek(fd,0,SEEK_END);
            if (size == -1) {
                PL_PRINTF("lseek\n")
                size = 0;
            }
			lseek(fd,0,SEEK_SET);
            *outFileSize = (u64)size;
        }
		return {TO_INTERNAL(fd)};
	}
    u64 FileWrite(APIFile file, const void* buffer, u64 writeBytes){
		Assert(writeBytes!=(u64)-1); // -1 indicates no bytes read
		
		int bytesWritten = write(TO_HANDLE(file.internal),buffer,writeBytes);
		if(bytesWritten == -1){
			// DWORD err = GetLastError();
            int err = 0;
			PL_PRINTF("write '%lu'\n",file.internal);
			return -1;
		}
		return bytesWritten;
	}
	u64 FileRead(APIFile file, void* buffer, u64 readBytes) {
        // printf("REDAING\n");
        ssize_t size = read(TO_HANDLE(file.internal), buffer, readBytes);
		// printf("fr size: %lu\n",size);
        if (size == -1) {
			PL_PRINTF("read\n");
			return -1;
		}
		return size;
	}
	void FileClose(APIFile file) {
		if (file)
			close(TO_HANDLE(file.internal));
	}
	bool FileSetHead(APIFile file, u64 position){
		int err;
		if(position==(u64)-1){
			err = lseek(TO_HANDLE(file.internal), 0, SEEK_END);
		}else{
			err = lseek(TO_HANDLE(file.internal), position, SEEK_SET);
		}
		if(err != -1) return true;
		
		// PL_PRINTF("[WinError %lu] lseek '%llu'\n",err,file.internal);
		return false;
	}
	u64 FileGetSize(APIFile file){
		struct stat buffer;   
        if(fstat(TO_HANDLE(file.internal), &buffer) == 0) {
			return buffer.st_size;
		}
		PL_PRINTF("fstat '%lu'\n",file.internal);
		return buffer.st_size;
	}
	bool FileFlushBuffers(APIFile file){
		PL_PRINTF("FileFlushBuffers not implemented\n");
		return false;
	}
    bool FileExist(const std::string& path){
        struct stat buffer;  
        int err = stat(path.c_str(), &buffer);
        auto ok = errno;
        if (err != 0)
            return false;
        return (buffer.st_mode & S_IFDIR) == 0;
    }
	bool DirectoryExist(const std::string& path){
        struct stat buffer;   
        return (stat(path.c_str(), &buffer) == 0) && (buffer.st_mode & S_IFDIR);
    }
	bool DirectoryCreate(const std::string& path) {
		return 0 == mkdir(path.c_str(), 0700); // 0700 = read, write, execute permissions
	}
	u64 FileGetHead(APIFile file){
		off_t position = lseek(TO_HANDLE(file.internal), 0, SEEK_CUR);
		if(position == -1) {
			// PL_PRINTF("[WinError %lu] lseek '%llu'\n",err,file.internal);
			return 0;
		}
		return position;
	}
	int GetCPUCoreCount() {
		return sysconf(_SC_NPROCESSORS_ONLN);
	}	
    bool FileLastWriteSeconds(const std::string& path, double* seconds, bool log_error){
		struct stat buffer;
		int err = stat(path.c_str(), &buffer);
		if(err != 0)
			return false;

        // return ( == 0) && (buffer.st_mode & S_IFDIR);

		auto& ts = buffer.st_mtim;
		*seconds = (double)(((u64)ts.tv_sec * NS + (u64)ts.tv_nsec))/(double)NS;
        return true;
    }
     bool FileLastWriteTimestamp_us(const std::string& path, u64* timestamp, bool log_error){
		struct stat buffer;
		int err = stat(path.c_str(), &buffer);
		if(err != 0)
			return false;

        // return ( == 0) && (buffer.st_mode & S_IFDIR);

		auto& ts = buffer.st_mtim;
		*timestamp = (u64)ts.tv_sec * 1000'000 + (u64)ts.tv_nsec;
        return true;
    }
	bool FileCopy(const std::string& src, const std::string& dst, bool log_error){
		// Assert(("FileCopy function not implemented",false));
		 
		int from = open(src.c_str(), O_RDONLY);
		if(from < 0)
			return false;
		int to = open(src.c_str(), O_WRONLY | O_CREAT, 00644);
		if(to < 0) {
			close(to);
			return false;
		}
		#define BUFFER_SIZE 0x100'000
		void* ptr = malloc(BUFFER_SIZE);
		if(!ptr)
			return false;

        bool success = true;
		int n;
		while ((n = read(from, ptr, BUFFER_SIZE)) > 0) {
			int res = write(to, ptr, n);
            if(res < 0) {
                success = false;
                break;
            }
		}

		close(from);
		close(to);

		free(ptr);

		return success;
	}
	bool FileMove(const std::string& src, const std::string& dst){
		int err = rename(src.c_str(),dst.c_str());
		if(err == -1){
            PL_PRINTF("rename '%s' '%s'\n",src.c_str(),dst.c_str());
            return false;
		}
		return true;
	}
	bool FileDelete(const std::string& path){
		int err = remove(path.c_str());
		if(err == -1) {
            PL_PRINTF("remove '%s'\n",path.c_str());
            return false;
		}
		return true;
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
    std::string EnvironmentVariable(const std::string& name){
		char* value = getenv(name.c_str());
        return value;
	}
    // struct AllocInfo {
	// 	std::string name;
	// 	int count;
	// };
	// static std::unordered_map<u64, AllocInfo> allocTracking;
    // TODO: TrackType and PrintTracking is the same on Linux and Windows.
    // This will be a problem if code is changed in one file but not in the other.
	// #define ENGONE_TRACK_ALLOC 0
	// #define ENGONE_TRACK_FREE 1
	// #define ENGONE_TRACK_REALLOC 2
	// static bool s_trackerEnabled=true;
    // void TrackType(u64 bytes, const std::string& name){
	// 	auto pair = allocTracking.find(bytes);
	// 	if(pair==allocTracking.end()){
	// 		allocTracking[bytes] = {name,0};	
	// 	} else {
	// 		pair->second.name += "|";
	// 		pair->second.name += name;
	// 	}
	// }
    // void PrintTracking(u64 bytes, int type){
	// 	if(!s_trackerEnabled)
	// 		return;

	// 	auto pair = allocTracking.find(bytes);
	// 	if(pair!=allocTracking.end()){
	// 		if(type==ENGONE_TRACK_ALLOC)
	// 			pair->second.count++;
	// 		else if(type==ENGONE_TRACK_FREE)
	// 			pair->second.count--;
	// 		// else if(type==ENGONE_TRACK_REALLOC)
	// 		// 	pair->second.count--;
	// 		printf("%s %s (%d left)\n",type==ENGONE_TRACK_ALLOC?"alloc":(type==ENGONE_TRACK_FREE?"free" : "realloc"), pair->second.name.c_str(),pair->second.count);
	// 	}
	// }
	// void SetTracker(bool on){
	// 	s_trackerEnabled = on;
	// }
    // void PrintRemainingTrackTypes(){
	// 	for(auto& pair : allocTracking){
	// 		if(pair.second.count!=0)
	// 			printf(" %s (%lu bytes): %d left\n",pair.second.name.c_str(),pair.first,pair.second.count);	
	// 	}
	// }

	static std::unordered_map<void*, int> s_userAllocations;

	// static std::mutex s_allocStatsMutex;
	static u64 s_totalAllocatedBytes=0;
	static u64 s_totalNumberAllocations=0;
	static u64 s_allocatedBytes=0;
	static u64 s_numberAllocations=0;
	void* Allocate(u64 bytes){
		if(bytes==0) return nullptr;
		// MEASURE;
		// void* ptr = HeapAlloc(GetProcessHeap(),0,bytes);
		// _LOG(LOG_ALLOCATIONS,printf("* Allocate %lu\n",bytes);)
        void* ptr = malloc(bytes);
		if(!ptr) return nullptr;
		// _LOG(LOG_ALLOCATIONS,printf("* Allocate %lu %p\n",bytes,ptr);)
		
		// _LOG(LOG_ALLOCATIONS,
		// 	auto pair = s_userAllocations.find(ptr);
		// 	Assert(("retrieved the same pointer?",pair == s_userAllocations.end()));
		// 	s_userAllocations[ptr] = bytes;
		// )
		
		// PrintTracking(bytes,ENGONE_TRACK_ALLOC);
		
		// s_allocStatsMutex.lock();
		s_allocatedBytes+=bytes;
		s_numberAllocations++;
		s_totalAllocatedBytes+=bytes;
		s_totalNumberAllocations++;			
		// s_allocStatsMutex.unlock();
		
		return ptr;
	}
    void* Reallocate(void* ptr, u64 oldBytes, u64 newBytes){
		// MEASURE;
        if(newBytes==0){
			// Assert(newBytes != 0);
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

				// _LOG(LOG_ALLOCATIONS,printf("* Reallocate %lu -> %lu %p->%p\n",oldBytes, newBytes, ptr, newPtr);)

				// _LOG(LOG_ALLOCATIONS,
				// 	auto pair = s_userAllocations.find(ptr);
				// 	Assert(("pointer doesn't exist?",pair != s_userAllocations.end()));
				// 	auto pair2 = s_userAllocations.find(newPtr);
				// 	Assert(("new pointer exists?",ptr == newPtr || pair2 == s_userAllocations.end()));
				// 	s_userAllocations.erase(ptr);
				// 	s_userAllocations[newPtr] = newBytes;
				// )
                
				// if(allocTracking.size()!=0)
				// 	printf("%s %lu -> %lu\n","realloc", oldBytes, newBytes);
				// PrintTracking(newBytes,ENGONE_TRACK_REALLOC);
                // s_allocStatsMutex.lock();
                s_allocatedBytes+=newBytes-oldBytes;
                
                s_totalAllocatedBytes+=newBytes;
                s_totalNumberAllocations++;			
                // s_allocStatsMutex.unlock();
                return newPtr;
            }
        }
    }
	void Free(void* ptr, u64 bytes){
		if(!ptr) return;
		// MEASURE;
		// _LOG(LOG_ALLOCATIONS, printf("* Free %lu %p\n",bytes, ptr);)

		// _LOG(LOG_ALLOCATIONS,
		// 	auto pair = s_userAllocations.find(ptr);
		// 	Assert(("pointer doesn't exist?",pair != s_userAllocations.end()));
		// 	s_userAllocations.erase(ptr);
		// )	
		free(ptr);
		// HeapFree(GetProcessHeap(),0,ptr);
		// PrintTracking(bytes,ENGONE_TRACK_FREE);
		// s_allocStatsMutex.lock();
		s_allocatedBytes-=bytes;
		s_numberAllocations--;
		Assert(s_allocatedBytes>=0);
		Assert(s_numberAllocations>=0);
		// s_allocStatsMutex.unlock();
	}
	u64 GetTotalAllocatedBytes() {
		return s_totalAllocatedBytes;
	}
	u64 GetTotalNumberAllocations() {
		return s_totalNumberAllocations;
	}
	u64 GetAllocatedBytes() {
		return s_allocatedBytes;
	}
	u64 GetNumberAllocations() {
		return s_numberAllocations;
	}
    void SetConsoleColor(uint16 color){
		// ANSI/bash colors: https://gist.github.com/JBlond/2fea43a3049b38287e5e9cefc87b2124
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
			default: {
				// printf("{MIS}");
			}
		/*
					   color  background   bright (not very bright, depends on your terminal I suppose)
			black        30		40			90
			red          31		41			91
			green        32		42			92
			yellow       33		43			93
			blue         34		44			94
			magenta      35		45			95
			cyan         36		46			96
			white        37		47			97
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
		if(color == 0) {
        	fprintf(stdout,"\033[0m");
			fflush(stdout);
        	// printf("\033[0m");
		} else {
        	fprintf(stdout,"\033[%u;%um",foreExtra, fore);
			fflush(stdout);
			// write(STDOUT_FILENO, );
        	// printf("\033[%u;%um",foreExtra, fore);
		}
		// fprintf(stdout,"{%d}",(int)color);
		// fflush(stdout);
	}
	std::string GetPathToExecutable() {
		std::string out{};
		out.resize(0x180);

		char self[PATH_MAX] = { 0 };
		int len = readlink("/proc/self/exe", (char*)out.data(), out.size());

		if(len < 0 || len > out.size())
			return "";

		out.resize(len);
		return out;
	}

	int GetConsoleWidth() {
		struct winsize w;
		int err = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(err==-1) {
			PL_PRINTF("ioctl, TIOCGWINSZ")
			return 0;
		}
		return w.ws_col;
	}
    i32 atomic_add(volatile i32* ptr, i32 value) {
        i32 res = value + __atomic_fetch_add((volatile long*)ptr, value, __ATOMIC_SEQ_CST);
        return res;
    }
	Semaphore::Semaphore(u32 initial, u32 max) {
		Assert(!m_initialized);
		m_initial = initial;
		m_max = max;
	}
	void Semaphore::init(u32 initial, u32 max) {
		Assert(!m_initialized);
		m_initial = initial;
		m_max = max;

		Assert(sizeof(sem_t) == SEM_SIZE);

		if(!m_initialized) {
			memset(m_data, 0, 32);
			int err = sem_init((sem_t*)m_data,0,initial);
			if(err == -1) {
				PL_PRINTF("sem_init\n");
			} else {
				m_initialized = true;
			}
		}
	}
	void Semaphore::cleanup() {
		if(m_initialized) {
			int err = sem_destroy((sem_t*)m_data);
			if(err == -1) {
				PL_PRINTF("sem_destroy\n");
			}
			m_initialized = false;
		}
	}
	void Semaphore::wait() {
		// MEASURE

		Assert(sizeof(sem_t) == SEM_SIZE);
		if(m_initialized) {
			memset(m_data, 0, 32);
			int err = sem_init((sem_t*)m_data,0,m_initial);
			if(err == -1) {
				PL_PRINTF("sem_init\n");
			}
		}

		int err = sem_wait((sem_t*)m_data);
		if(err != 0) {
			PL_PRINTF("sem_wait\n");
		}
	}
	void Semaphore::signal(int count) {
		// MEASURE

		if(m_initialized) {
			while(count>0){
				--count;
				int err = sem_post((sem_t*)m_data);
				if(err == -1) {
					PL_PRINTF("sem_init\n");
					break;
				}
			}
		}
	}
    void Mutex::cleanup() {
        if (m_internalHandle != 0){
			auto ptr = (pthread_mutex_t*)m_internalHandle;
			
			// printf("destroy %p\n", ptr);
			int res = pthread_mutex_destroy(ptr);
			free(ptr);
			if(res != 0) {
				Assert(false);
				return;
			}
			m_internalHandle=0;
        }
	}
	void Mutex::lock() {
		if(m_internalHandle == 0) {
			auto ptr = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
			m_internalHandle = (u64)ptr;
			int res = pthread_mutex_init(ptr, nullptr);
			if(res != 0) {
				Assert(false);	
				return;
			}
			// printf("init %p\n", ptr);
		}
		if(m_internalHandle != 0) {
			auto ptr = (pthread_mutex_t*)m_internalHandle;
			u32 newId = Thread::GetThisThreadId();
			// printf("lock %p\n", ptr);
			if (m_ownerThread != 0) {
				PL_PRINTF("Mutex : Locking twice, old owner: %lu, new owner: %u\n",m_ownerThread, newId);
				Assert(false);
			}
			int res = pthread_mutex_lock(ptr);
			if(res != 0) {
				Assert(false);
				return;
			}
			m_ownerThread = newId;
		}
	}
	void Mutex::unlock() {
		// O_CREAT
		if (m_internalHandle != 0) {
			if (m_ownerThread == 0) {
				u32 newId = Thread::GetThisThreadId();
				PL_PRINTF("Mutex : Unlocking twice, culprit: %u\n",newId);
				Assert(false);
			}
			m_ownerThread = 0;
			auto ptr = (pthread_mutex_t*)m_internalHandle;
			// printf("unlock %p\n", ptr);
			int res = pthread_mutex_unlock(ptr);
			if(res != 0) {
				Assert(false);
				return;
			}
		}
	}
	ThreadId Mutex::getOwner() {
		return m_ownerThread;
	}
	struct DataForThread {
		uint32(*func)(void*) = nullptr;
		void* userData = nullptr;
		static DataForThread* Create(){
			// Heap allocated at the moment but you could create a bucket array
			// instead. Or store 40 of these as global data and then use heap
			// allocation if it fills up.
			auto ptr = (DataForThread*)Allocate(sizeof(DataForThread));
			new(ptr)DataForThread();
			return ptr;
		}
		static void Destroy(DataForThread* ptr){
			ptr->~DataForThread();
			Free(ptr,sizeof(DataForThread));
		}
	};
	void* SomeThreadProc(void* ptr){
		DataForThread* data = (DataForThread*)ptr;
		uint32 ret = data->func(data->userData);
		DataForThread::Destroy(data);
		return (void*)(u64)ret;
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
		Assert(sizeof(pthread_t) == THREAD_SIZE);

		if(m_threadId == 0) {
			auto ptr = DataForThread::Create();
			ptr->func = func;
			ptr->userData = arg;

			int err = pthread_create((pthread_t*)&m_data, nullptr, SomeThreadProc, ptr);
			if(err == -1) {
				PL_PRINTF("pthread_create\n");
			} else {
				// Assert((m_data & 0xFFFFFFFF00000000) == 0);
				Assert(sizeof(m_threadId) == sizeof(pthread_t));
				m_threadId = m_data;
			}
		}
	}
	void Thread::join() {
		if(m_threadId != 0) {
			int err = pthread_join(*(pthread_t*)&m_data, 0);
			if(err == -1) {
	            PL_PRINTF("pthread_join\n");
			} else {
				m_threadId = 0;
			}
		}
		// if (!m_internalHandle)
		// 	return;
		// DWORD res = WaitForSingleObject(TO_HANDLE(m_internalHandle), INFINITE);
		// if (res==WAIT_FAILED) {
		// 	DWORD err = GetLastError();
		// }
		// BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
        // if(!yes){
        //     DWORD err = GetLastError();
        //     PL_PRINTF("[WinError %lu] CloseHandle\n",err);
        // }
	}
	bool Thread::isActive() {
		return m_threadId!=0;
	}
	bool Thread::joinable() {
        return m_threadId != 0 && m_threadId != GetThisThreadId();
	}
	ThreadId Thread::getId() {
		return m_threadId;
	}
	ThreadId Thread::GetThisThreadId() {
		pthread_t id = pthread_self();
		// fprintf(stderr,"id %lu",(u64)id);
		// Assert(((u64)id & 0xFFFFFFFF00000000) == 0);
        return id;
	}
	
	u32 Thread::CreateTLSIndex() {
		pthread_key_t key = 0;
		int err = pthread_key_create(&key, nullptr);
		if(err == -1) {
			PL_PRINTF("pthread_key_create\n");
		}
		Assert(sizeof(pthread_key_t) <= 4);
		return (u32)key+1;
	}
	bool Thread::DestroyTLSIndex(u32 index) {
		pthread_key_t key = (pthread_key_t)(index-1);
		int err = pthread_key_delete(key);
		if(err == -1) {
			PL_PRINTF("pthread_key_delete\n");
		}
		return err != -1;
	}
	void* Thread::GetTLSValue(u32 index){
		pthread_key_t key = (pthread_key_t)(index-1);
		void* ptr = pthread_getspecific(key);
		return ptr;
	}
	bool Thread::SetTLSValue(u32 index, void* ptr){
		pthread_key_t key = (pthread_key_t)(index-1);
		int err = pthread_setspecific(key, ptr);
		if(err == -1) {
			PL_PRINTF("pthread_setspecific, invalid index\n");
		}
		return err != -1;
	}
	struct PipeInfo{
		int readFD=0;	
		int writeFD=0;	
	};
	std::unordered_map<u64,PipeInfo> pipes;
	u64 pipeIndex=0x500000;
	APIPipe PipeCreate(u64 pipeBuffer, bool inheritRead,bool inheritWrite){
		PipeInfo info{};

		// TODO: pipeBuffer and inherit arguments are ignored

		int fds[2];

		int err = pipe(fds);
		if(err == -1) {
			return {0};
		}

		info.readFD = fds[0];
		info.writeFD = fds[1];
		Assert(info.readFD != 0 || info.writeFD != 0); // we assume 0 means invalid/no descriptor, is this the case though?

		pipes[pipeIndex] = info;
		return {pipeIndex++};
	}
	void PipeDestroy(APIPipe pipe, bool close_read, bool close_write){
		auto& info = pipes[pipe.internal];
		if(info.readFD && close_read) {
			close(info.readFD);
			info.readFD = 0;
		}
		if(info.writeFD && close_write) {
			close(info.writeFD);
			info.writeFD = 0;
		}
		if(!info.readFD && !info.writeFD)
			pipes.erase(pipe.internal);
	}
	u64 PipeRead(APIPipe pipe, void* buffer, u64 size){
		auto& info = pipes[pipe.internal];

		u64 readBytes = read(info.readFD, buffer, size);
		if(readBytes == (u64)-1) {
			PL_PRINTF("Pipe read failed\n");
			return -1;
		}
		return readBytes;
	}
	u64 PipeWrite(APIPipe pipe,void* buffer, u64 size){
		auto& info = pipes[pipe.internal];
		u64 written = write(info.writeFD, buffer, size);
		if(written == (u64)-1) {
			PL_PRINTF("Pipe write failed\n");
			return -1;
		}
		return written;
	}
	APIFile PipeGetRead(APIPipe pipe){
		auto& info = pipes[pipe.internal];
		return {TO_INTERNAL(info.readFD)};
	}
	APIFile PipeGetWrite(APIPipe pipe){
		auto& info = pipes[pipe.internal];
		return {TO_INTERNAL(info.writeFD)};
	}
	bool SetStandardOut(APIFile file){
		int fd = TO_HANDLE(file.internal);
		int err = dup2(fd, STDOUT_FILENO);
		return err != -1;
	}
	APIFile GetStandardOut(){
		return {TO_INTERNAL(STDOUT_FILENO)};
	}
	bool SetStandardErr(APIFile file){
		int fd = TO_HANDLE(file.internal);
		int err = dup2(fd, STDERR_FILENO);
		return err != -1;
	}
	APIFile GetStandardErr(){
		return {TO_INTERNAL(STDERR_FILENO)};
	}
	bool SetStandardIn(APIFile file){
		int fd = TO_HANDLE(file.internal);
		int err = dup2(fd, STDIN_FILENO);
		return err != -1;
	}
	APIFile GetStandardIn(){
		return {TO_INTERNAL(STDIN_FILENO)};
	}
	bool ExecuteCommand(const std::string& cmd, bool asynchonous, int* exitCode){
		return false;
		
		// TODO: This whole function
		// if(cmd.length() == 0) return false;

		// auto pipe = PipeCreate(100 + cmd.length(), true, true);

		// // additional information
		// STARTUPINFOA si;
		// PROCESS_INFORMATION pi;
		// // set the size of the structures
		// ZeroMemory(&si, sizeof(si));
		// si.cb = sizeof(si);
		// ZeroMemory(&pi, sizeof(pi));
        
		// bool inheritHandles=true;
		 
		// DWORD createFlags = 0;

		// HANDLE prev = GetStdHandle(STD_INPUT_HANDLE);
		// SetStdHandle(STD_INPUT_HANDLE, TO_HANDLE(PipeGetRead(pipe).internal));
			
		// PipeWrite(pipe, (void*)cmd.data(), cmd.length());

		// BOOL yes = CreateProcessA(NULL,   // the path
		// 	(char*)cmd.data(),        // Command line
		// 	NULL,           // Process handle not inheritable
		// 	NULL,           // Thread handle not inheritable
		// 	inheritHandles,          // Set handle inheritance
		// 	createFlags,              // creation flags
		// 	NULL,           // Use parent's environment block
		// 	NULL,   // starting directory 
		// 	&si,            // Pointer to STARTUPINFO structure
		// 	&pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
		// 	);
		
		// SetStdHandle(STD_INPUT_HANDLE, prev);

		// if(!asynchonous){
		// 	WaitForSingleObject(pi.hProcess,INFINITE);
		// 	if(exitCode){
		// 		BOOL success = GetExitCodeProcess(pi.hProcess, (DWORD*)exitCode);
		// 		if(!success){
		// 			DWORD err = GetLastError();
		// 			printf("[WinError %lu] ExecuteCommand, failed aquring exit code (path: %s)\n",err,cmd.c_str());	
		// 		}else{
		// 			if(success==STILL_ACTIVE){
		// 				printf("[WinWarning] ExecuteCommand, cannot get exit code, process is active (path: %s)\n",cmd.c_str());	
		// 			}else{
						
		// 			}
		// 		}
		// 	}
		// }

		// PipeDestroy(pipe);
		
		// CloseHandle(pi.hProcess);
		// CloseHandle(pi.hThread);

		// return true;
	}
	bool StartProgram(const char* commandLine, u32 flags, int* exitCode, bool* non_normal_exit, APIFile fStdin, APIFile fStdout, APIFile fStderr) {
		// if (!FileExist(path)) {
		// 	return false;
		// }
		int cmdlen = strlen(commandLine);
		if(!commandLine || cmdlen == 0){
			return false;
		}

		// TODO: Do something with standard handles?

        char** argv = nullptr;
        int argc = 0;
        ConvertArguments(commandLine, argc, argv);
        
        // FileExist does not work for programs in PATH
        // bool yes = FileExist(argv[0]);
        // if(!yes) {
        //     printf("[LinuxError] StartProgram, '%s' does not exist\n", argv[0]);
        // }
        
		int pid = fork();
		if(pid == -1) {
			FreeArguments(argc, argv);
			PL_PRINTF("fork failed\n");
			return false;
		}
		if(pid == 0) {
            auto old_in = engone::GetStandardIn();
            auto old_out = engone::GetStandardOut();
            auto old_err = engone::GetStandardErr();

			bool any_error = false;
			if(fStdin)
				any_error |= !engone::SetStandardIn(fStdin);
			if(fStdout)
				any_error |= !engone::SetStandardOut(fStdout);
			if(fStderr)
				any_error |= !engone::SetStandardErr(fStderr);

			if(any_error) {
				printf("[LinuxError] One ore more pipes to StartProgram were invalid.\n"); // TODO: Print the file descriptors.
				return false;
			}

			int err = execvp(argv[0], argv);

			// we only end up here if execvp failed
            
            if(old_in)
				engone::SetStandardIn(old_in);
			if(old_out)
				engone::SetStandardOut(old_out);
			if(old_err)
				engone::SetStandardErr(old_err);
			// printf("%d\n",err);
            
			if(err < 0) {
				printf("[LinuxError] %s, execv(\"%s\")\n", strerror(errno), argv[0]);
				FreeArguments(argc, argv);
				exit(1); // error
			}
            // PL_PRINTF("execv\n");
			FreeArguments(argc, argv);
			exit(0);
			return false; // shouldn't happen
		}
		
		bool failed = false;
		if(flags&PROGRAM_WAIT){
			int status = 0;
			int err = waitpid(pid, &status, 0);
			if(err < 0) {
				PL_PRINTF("waitpid\n");
				failed = true;
			} else {
				if(WIFEXITED(status)) {
					int new_status = WEXITSTATUS(status);
					if(exitCode)
						*exitCode = new_status;
					failed = new_status != 0;
				}
				if(WIFSIGNALED(status)) {
					if(non_normal_exit) {
						*non_normal_exit = true;
						failed = true;
						// *non_normal_exit = WTERMSIG(status);
					}
				}

			}
		}
        FreeArguments(argc, argv);
		return !failed;
	}
	void ConvertArguments(const char* args, int& argc, char**& argv) {
		if (args == nullptr) {
			printf("ConvertArguments : Args was null\n");
		} else {
			argc = 0;
			// argv will become a pointer to contigous memory which contain arguments.
			int dataSize = 0;
			int argsLength = strlen(args);
			int argLength = 0;
			for (int i = 0; i < argsLength + 1; i++) {
				char chr = args[i];
				if (chr == 0 || chr == ' ') {
					if (argLength != 0) {
						dataSize++; // null terminated character
						argc++;
					}
					argLength = 0;
					if (chr == 0)
						break;
				} else {
					argLength++;
					dataSize++;
				}
			}
			int index = (argc + 1) * sizeof(char*);
			int totalSize = index + dataSize;
			//printf("size: %d index: %d\n", totalSize,index);
			argv = (char**)Allocate(totalSize);
			argv[argc] = nullptr;
			char* argData = (char*)argv + index;
			if (!argv) {
				printf("ConverArguments : Allocation failed!\n");
			} else {
				int strIndex = 0; // index of char*
				for (int i = 0; i < argsLength + 1; i++) {
					char chr = args[i];
                    
					if (chr == 0 || chr == ' ') {
						if (argLength != 0) {
							argData[i] = 0;
							dataSize++; // null terminated character
						}
						argLength = 0;
						if (chr == 0)
							break;
					} else {
						if (argLength == 0) {
							argv[strIndex] = argData + i;
							strIndex++;
						}
						argData[i] = chr;
						argLength++;
					}
				}
			}
		}
	}
	void FreeArguments(int argc, char** argv) {
		int totalSize = (argc + 1) * sizeof(char*); // argc +1 because the array is terminated with a nullptr (Linux execv needs it)
		int index = totalSize;
		for (int i = 0; i < argc; i++) {
			int length = strlen(argv[i]);
			//printf("len: %d\n", length);
			totalSize += length + 1;
		}
		Free(argv, totalSize);
	}
	DynamicLibrary LoadDynamicLibrary(const std::string& path, bool log_error) {
		auto ptr = dlopen(path.c_str(), 0);
		if (log_error) {
			// TODO: Print error
		}
		return ptr;
	}
	void UnloadDynamicLibrary(DynamicLibrary library) {
		dlclose(library);
	}
	// You may need to cast the function pointer to the appropriate function
	VoidFunction GetFunctionPointer(DynamicLibrary library, const std::string& name) {
		auto ptr = dlsym(library, name.c_str());
		return (VoidFunction)ptr;
	}
    
    // This works on both linux and MacOSX (and any BSD kernel).
    bool IsProcessDebugged() {
        // https://forum.juce.com/t/detecting-if-a-process-is-being-run-under-a-debugger/2098
        static bool isCheckedAlready = false;
        static int underDebugger = 0;
        if (!isCheckedAlready) {
            if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0)
                underDebugger = 1;
            else
                ptrace(PTRACE_DETACH, 0, 1, 0);
            isCheckedAlready = true;
        }
        return underDebugger == 1;
    }
}
#endif