
#include "Engone/PlatformLayer.h"

#ifdef OS_WINDOWS

#ifdef PL_PRINT_ERRORS
#define PL_PRINTF printf
#else
#define PL_PRINTF
#endif

// RDTSC togerher with cpu frequency from the windows register
// is more accurate than QueryPerformance-Counter/Frequency
#define USE_RDTSC

// #define LOG_ALLOCATIONS
// #define LOG_ALLOC(...) if(global_loggingSection&LOG_ALLOCATIONS) { __VA_ARGS__; }

#include <unordered_map>
#include <vector>
#include <string>

// #define NOMINMAX
// #define _WIN32_WINNT 0x0601
// #define WIN32_LEAN_AND_MEAN
// #include <windows.h>

// #include <mutex>
// Cheeky include
#include "BetBat/Util/Perf.h"
#include "Engone/Win32Includes.h"

#include "Engone/Util/Array.h"

#ifdef USE_RDTSC
// to get cpu clock speed
#include <winreg.h>
// #include <intrin.h> // included by Win32Includes
// #pragma comment(lib,"Advapi32.lib")
#endif

// Name collision
auto WIN_Sleep = Sleep;

auto WIN_SHARE_READ = FILE_SHARE_READ;
auto WIN_SHARE_WRITE = FILE_SHARE_WRITE;

namespace engone {
    //-- Platform specific
    
#define TO_INTERNAL(X) ((u64)X+1)
#define TO_HANDLE(X) (HANDLE)((u64)X-1)
    
	// Recursive directory iterator info
	struct RDIInfo{
		std::string root;
		std::string dir;
		HANDLE handle;
		DynamicArray<std::string> directories;
		char* tempPtr = nullptr; // used in result
		u32 max = 0; // not including \0
	};
	static std::unordered_map<DirectoryIterator,RDIInfo> s_rdiInfos;
	static u64 s_uniqueRDI=0;
	
	DirectoryIterator DirectoryIteratorCreate(const char* name, int pathlen){
		DirectoryIterator iterator = (DirectoryIterator)(++s_uniqueRDI);
		auto& info = s_rdiInfos[iterator] = {};
		info.root.resize(pathlen);
		memcpy((char*)info.root.data(), name, pathlen);
		info.handle=INVALID_HANDLE_VALUE;
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

		WIN32_FIND_DATAA data;
		while(true) {
			if(info->second.handle==INVALID_HANDLE_VALUE){
				if(info->second.directories.size()==0){
					return false;
				}
                info->second.dir.clear();
                
				if(info->second.directories[0].size()!=0){
                    info->second.dir += info->second.directories[0];
				}
                
                std::string temp = info->second.dir;
                if(!temp.empty())
                    temp += "\\";
                
                temp+="*";
				// printf("FindFirstFile %s\n",temp.c_str());
				// TODO: This does a memcpy on the whole array (almost).
				//  That's gonna be slow!
				info->second.directories.removeAt(0);
				// fprintf(stderr, "%s %p\n", temp.c_str(), &data);
				
				// DWORD err = GetLastError();
				// PL_PRINTF("[WinError %lu] GetLastError '%llu'\n",err,(u64)iterator);
				HANDLE handle=INVALID_HANDLE_VALUE;
				handle = FindFirstFileA(temp.c_str(),&data);
				// handle = FindFirstFileA("src\\*",&data);
				// fprintf(stderr, "WHY%s %p\n", temp.c_str(), &data);
				
				if(handle==INVALID_HANDLE_VALUE){
					DWORD err = GetLastError();
					PL_PRINTF("[WinError %lu] FindNextFileA '%llu'\n",err,(u64)iterator);
					continue;
				}
				info->second.handle = handle;
			}else{
				BOOL success = FindNextFileA(info->second.handle,&data);
				if(!success){
					DWORD err = GetLastError();
					if(err == ERROR_NO_MORE_FILES){
						// PL_PRINTF("[WinError %u] No files '%lu'\n",err,(u64)iterator);
					}else {
						PL_PRINTF("[WinError %lu] FindNextFileA '%llu'\n",err,(u64)iterator);
					}
					bool success = FindClose(info->second.handle);
					info->second.handle = INVALID_HANDLE_VALUE;
					if(!success){
						err = GetLastError();
						PL_PRINTF("[WinError %lu] FindClose '%llu'\n",err,(u64)iterator);
					}
					continue;
				}
			}
			// fprintf(stderr, "%p %p\n", iterator, result);
			if(strcmp(data.cFileName,".")==0||strcmp(data.cFileName,"..")==0){
				continue;
			}
			break;
		}

		int cFileNameLen = strlen(data.cFileName);
		int newLength = cFileNameLen;
		if(!info->second.dir.empty())
			newLength += info->second.dir.length()+1;

		if(!info->second.tempPtr) {
			info->second.max = 256;
			info->second.tempPtr = (char*)Allocate(info->second.max + 1);
            Assert(info->second.tempPtr);
		} else if(info->second.max < newLength) {
			u32 old = info->second.max;
			info->second.max = info->second.max + newLength;
			info->second.tempPtr = (char*)Reallocate(info->second.tempPtr, old + 1, info->second.max + 1);
            Assert(info->second.tempPtr);
		}
		result->name = info->second.tempPtr;
		result->namelen = newLength;
        // printf("np: %p\n",result->name);

		if(!info->second.dir.empty()) {
			memcpy(result->name, info->second.dir.c_str(), info->second.dir.length());
			result->name[info->second.dir.length()] = '/';
			memcpy(result->name + info->second.dir.length() + 1, data.cFileName, cFileNameLen);
		} else {
			memcpy(result->name, data.cFileName, cFileNameLen);
		}
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

		result->fileSize = (u64)data.nFileSizeLow+(u64)data.nFileSizeHigh*((u64)MAXDWORD+1);
		u64 time = (u64)data.ftLastWriteTime.dwLowDateTime+(u64)data.ftLastWriteTime.dwHighDateTime*((u64)MAXDWORD+1);
		result->lastModified = time;
        result->lastWriteSeconds = time/10000000.f; // 100-nanosecond intervals
		result->isDirectory = data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY;
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
		
		if(info->second.handle!=INVALID_HANDLE_VALUE){
			BOOL success = FindClose(info->second.handle);
			if(!success){
				DWORD err = GetLastError();
				PL_PRINTF("[WinError %lu] Error closing '%llu'\n",err,(u64)iterator);
			}
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
	TimePoint StartMeasure(){
		#ifdef USE_RDTSC
		return (u64)__rdtsc();
		#else
		u64 tp;
		BOOL success = QueryPerformanceCounter((LARGE_INTEGER*)&tp);
		// if(!success){
		// 	PL_PRINTF("time failed\n");	
		// }
		// TODO: handle err
		return tp;
		#endif
	}
	static bool once = false;
	static u64 frequency;
	double StopMeasure(TimePoint startPoint){
		if(!once){
			once=true;
			#ifdef USE_RDTSC
			// TODO: Does this work on all Windows versions?
			//  Could it fail? In any case, something needs to be handled.
			DWORD mhz = 0;
			DWORD size = sizeof(DWORD);
			LONG err = RegGetValueA(
				HKEY_LOCAL_MACHINE,
				"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
				"~MHz",
				RRF_RT_DWORD,
				NULL,
				&mhz,
				&size
			);
			frequency = (u64)mhz * (u64)1000000;
			#else
			BOOL success = QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
			// if(!success){
			// 	PL_PRINTF("time failed\n");	
			// }
			// TODO: handle err
			#endif
		}
		TimePoint endPoint;
		#ifdef USE_RDTSC
		endPoint = (u64)__rdtsc();
		#else
		BOOL success = QueryPerformanceCounter((LARGE_INTEGER*)&endPoint);
		// if(!success){
		// 	PL_PRINTF("time failed\n");	
		// }
		#endif
		return (double)(endPoint-startPoint)/(double)frequency;
	}
	double DiffMeasure(TimePoint endSubStart) {
		if(!once){
			once=true;
			#ifdef USE_RDTSC
			// TODO: Does this work on all Windows versions?
			//  Could it fail? In any case, something needs to be handled.
			DWORD mhz = 0;
			DWORD size = sizeof(DWORD);
			LONG err = RegGetValueA(
				HKEY_LOCAL_MACHINE,
				"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
				"~MHz",
				RRF_RT_DWORD,
				NULL,
				&mhz,
				&size
			);
			frequency = (u64)mhz * (u64)1000000;
			#else
			BOOL success = QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
			// if(!success){
			// 	PL_PRINTF("time failed\n");	
			// }
			// TODO: handle err
			#endif
		}
		return (double)(endSubStart)/(double)frequency;
	}
	u64 GetClockSpeed(){
		static u64 clockSpeed = 0; // frequency
		if(clockSpeed==0){
			// TODO: Does this work on all Windows versions?
			//  Could it fail? In any case, something needs to be handled.
			DWORD mhz = 0;
			DWORD size = sizeof(DWORD);
			LONG err = RegGetValueA(
				HKEY_LOCAL_MACHINE,
				"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
				"~MHz",
				RRF_RT_DWORD,
				NULL,
				&mhz,
				&size
			);
			clockSpeed = (u64)mhz * (u64)1000000;
		}
		return clockSpeed;
	}
	void Sleep(double seconds){
        WIN_Sleep((uint32)(seconds*1000));   
    }
    APIFile FileOpen(const char* path, u32 len, FileOpenFlags flags, u64* outFileSize){
		return FileOpen(std::string(path,len), flags, outFileSize);
	}
    APIFile FileOpen(const std::string& path, FileOpenFlags flags, u64* outFileSize){
        DWORD access = GENERIC_READ|GENERIC_WRITE;
        DWORD sharing = WIN_SHARE_READ|WIN_SHARE_WRITE;
		Assert(flags != 0);
		#undef FILE_READ_ONLY // bye bye Windows defined flag
        if(flags & FILE_READ_ONLY){
            access = GENERIC_READ;
		}
		
		DWORD creation = OPEN_EXISTING;
		// if(flags&FILE_CAN_CREATE)
		// 	creation = OPEN_ALWAYS;
		if(flags&FILE_CLEAR_AND_WRITE)
			creation = CREATE_ALWAYS;
		
		if(creation&OPEN_ALWAYS||creation&CREATE_ALWAYS){
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
		SECURITY_ATTRIBUTES sa;
		ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = true;
        
		HANDLE handle = CreateFileA(path.c_str(),access,sharing,&sa,creation,FILE_ATTRIBUTE_NORMAL, NULL);
		
		if(handle==INVALID_HANDLE_VALUE){
			DWORD err = GetLastError();
			if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND){
				// this can happen, it's not really an error, no need to print.
				// denied access to some more fundamental error is important however.
				// PL_PRINTF("[WinError %lu] Cannot find '%s'\n",err,path.c_str());
			}else if(err == ERROR_ACCESS_DENIED){
				PL_PRINTF("[WinError %lu] Denied access to '%s'\n",err,path.c_str()); // tried to open a directory?
			}else {
				PL_PRINTF("[WinError %lu] Error opening '%s'\n",err,path.c_str());
			}
			return {};
		}else if (outFileSize){
            
			DWORD success = GetFileSizeEx(handle, (LARGE_INTEGER*)outFileSize);
			if (!success) {
				// GetFileSizeEx will probably not fail if CreateFile succeeded. But just in case it does.
				DWORD err = GetLastError();
				printf("[WinError %lu] Error aquiring file size from '%s'",err,path.c_str());
				*outFileSize = 0;
				// Assert(outFileSize);
			}
		}
		return {TO_INTERNAL(handle)};
	}
	u64 FileRead(APIFile file, void* buffer, u64 readBytes){
		// Assert(readBytes!=(u64)-1); // -1 indicates no bytes read
		// Assert(buffer);
		DWORD bytesRead=0;
		DWORD success = ReadFile(TO_HANDLE(file.internal),buffer,readBytes,&bytesRead,NULL);
		if(!success){
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] FileRead '%llu'\n",err,file.internal);
			return -1;
		}
		return bytesRead;
	}
	u64 FileWrite(APIFile file, const void* buffer, u64 writeBytes){
		// Assert(writeBytes!=(u64)-1); // -1 indicates no bytes read
		// Assert(buffer);
		DWORD bytesWritten=0;
		DWORD success = WriteFile(TO_HANDLE(file.internal),buffer,writeBytes,&bytesWritten,NULL);
		if(!success){
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] FileWrite '%llu'\n",err,file.internal);
			return -1;
		}
		return bytesWritten;
	}
	bool FileSetHead(APIFile file, u64 position){
		DWORD success = 0;
		if(position==(u64)-1){
			success = SetFilePointerEx(TO_HANDLE(file.internal),{0},NULL,FILE_END);
		}else{
			success = SetFilePointerEx(TO_HANDLE(file.internal),*(LARGE_INTEGER*)&position,NULL,FILE_BEGIN);
		}
		if(success) return true;
		
		DWORD err = GetLastError();
		PL_PRINTF("[WinError %lu] FileSetHead '%llu'\n",err,file.internal);
		return false;
	}
	u64 FileGetSize(APIFile file){
		u64 size = 0;
		bool yes = GetFileSizeEx(TO_HANDLE(file.internal),(LARGE_INTEGER*)&size);
		if(!yes) {
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] GetFileSizeEx '%llu'\n",err,file.internal);
		}
		return size;
	}
	bool FileFlushBuffers(APIFile file){
		bool yes = FlushFileBuffers(TO_HANDLE(file.internal));
		if(!yes) {
			DWORD err = GetLastError();
			if (err == ERROR_INVALID_HANDLE) {
				// you might have use stdout/stdin/stderr
			} else {
				PL_PRINTF("[WinError %lu] FlushFileBuffers '%llu'\n",err,file.internal);
			}
		}
		return yes;
	}
	u64 FileGetHead(APIFile file) {
		u64 head = 0;
		DWORD success = SetFilePointerEx(TO_HANDLE(file.internal),{0},(LARGE_INTEGER*)&head,FILE_CURRENT);
		if(success)
			return head;
		
		DWORD err = GetLastError();
		PL_PRINTF("[WinError %lu] SetFilePointerEx '%llu'\n",err,file.internal);
		return false;
	}
	void FileClose(APIFile file){
		if(file)
			CloseHandle(TO_HANDLE(file.internal));
	}
	bool FileExist(const std::string& path){
        DWORD attributes = GetFileAttributesA(path.c_str());   
        if(attributes == INVALID_FILE_ATTRIBUTES){
            // DWORD err = GetLastError();
            // PL_PRINTF("[WinError %lu] GetFileAttributesA '%s'\n",err,path.c_str());
            return false;
        }
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
	bool DirectoryCreate(const std::string& path){
		DWORD success = CreateDirectoryA(path.c_str(),0);
		if(!success){
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] CreateDirectoryA '%s'\n",err,path.c_str());
            return false;
		}
		return true;
	}
    bool DirectoryExist(const std::string& path){
        DWORD attributes = GetFileAttributesA(path.c_str());   
        if(attributes == INVALID_FILE_ATTRIBUTES){
            DWORD err = GetLastError();
			if(err==ERROR_FILE_NOT_FOUND)
				return false;
            PL_PRINTF("[WinError %lu] GetFileAttributesA '%s'\n",err,path.c_str());
            return false;
        }
        return (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }
    bool FileLastWriteSeconds(const std::string& path, double* seconds){
        // HANDLE handle = CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL, NULL);
        HANDLE handle = CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0, NULL);
		
		if(handle==INVALID_HANDLE_VALUE){
			DWORD err = GetLastError();
			if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND){
				// this can happen, it's not really an error, no need to print.
				// denied access to some more fundamental error is important however.
				// PL_PRINTF("[WinError %lu] Cannot find '%s'\n",err,path.c_str());
			}else if(err == ERROR_ACCESS_DENIED){
				PL_PRINTF("[WinError %lu] Denied access to '%s'\n",err,path.c_str()); // tried to open a directory?
			}else {
				PL_PRINTF("[WinError %lu] Error opening '%s'\n",err,path.c_str());
			}
			return false;
		}
        FILETIME modified;
        FILETIME creation;
        FILETIME access;
        BOOL success = GetFileTime(handle,&creation,&access,&modified);
        if(!success){
            DWORD err = GetLastError();
            PL_PRINTF("[WinError %lu] GetFileTime '%s'\n",err,path.c_str());
            return false;
        }
        success = CloseHandle(handle);
        if(!success){
            DWORD err = GetLastError();
            PL_PRINTF("[WinError %lu] CloseHandle '%s'\n",err,path.c_str());
        }
		u64 t0 = (u64)creation.dwLowDateTime+(u64)creation.dwHighDateTime*((u64)MAXDWORD+1);
		u64 t1 = (u64)access.dwLowDateTime+(u64)access.dwHighDateTime*((u64)MAXDWORD+1);
		u64 t2 = (u64)modified.dwLowDateTime+(u64)modified.dwHighDateTime*((u64)MAXDWORD+1);
		//printf("T: %llu, %llu, %llu\n",t0,t1,t2);
		*seconds = t2/10000000.; // 100-nanosecond intervals
        return true;
    }
	bool FileCopy(const std::string& src, const std::string& dst){
		bool yes = CopyFileA(src.c_str(),dst.c_str(),0);
		if(!yes){
			DWORD err = GetLastError();
            PL_PRINTF("[WinError %lu] CopyFile '%s' '%s'\n",err,src.c_str(),dst.c_str());
            return false;
		}
		return true;
	}
	bool FileMove(const std::string& src, const std::string& dst){
		bool yes = MoveFileA(src.c_str(),dst.c_str());
		if(!yes){
			DWORD err = GetLastError();
            PL_PRINTF("[WinError %lu] MoveFile '%s' '%s'\n",err,src.c_str(),dst.c_str());
            return false;
		}
		return true;
	}
	bool FileDelete(const std::string& path){
		bool yes = DeleteFileA(path.c_str());
		if(!yes){
			DWORD err = GetLastError();
			if(err == ERROR_FILE_NOT_FOUND) {
				return true;
			}
            PL_PRINTF("[WinError %lu] DeleteFile '%s'\n",err,path.c_str());
            return false;
		}
		return true;
	}
#define DEBUG_PLATFORM_ERROR(x) x
	// #define DEBUG_PLATFORM_ERROR(x)
    
	static const int PLATFORM_ERROR_BUFFER = 3;
	static PlatformError s_platformErrors[PLATFORM_ERROR_BUFFER];
	static int s_errorIn = 0;
	static int s_errorOut = 0;
	static bool s_platformErrorEmpty = true;
	bool PollError(PlatformError* out){
		if(s_errorIn==s_errorOut){
            // if(s_errorIn==(s_errorOut+1)%PLATFORM_ERROR_BUFFER){
			DEBUG_PLATFORM_ERROR(printf("PlatformError: empty, in:%d out:%d\n",s_errorIn, s_errorOut);)
                // empty
                return false;
		}
		*out = s_platformErrors[s_errorOut];
		s_errorOut = (s_errorOut+1)%PLATFORM_ERROR_BUFFER;
		if(s_errorIn==s_errorOut)
			s_platformErrorEmpty=true;
		DEBUG_PLATFORM_ERROR(printf("PlatformError: poll %d, new out: %d\n",out->errorType,s_errorOut);)
            return true;
	}
	bool PushError(PlatformError* error){
		if(s_errorIn==s_errorOut){
            // if(s_errorOut==(s_errorIn+1)%PLATFORM_ERROR_BUFFER){
			DEBUG_PLATFORM_ERROR(printf("PlatformError: full, in:%d, out:%d\n",s_errorIn, s_errorOut);)
                return false; // full
		}
		s_platformErrorEmpty=false;
		s_platformErrors[s_errorIn] = *error;
		s_errorIn = (s_errorIn+1)%PLATFORM_ERROR_BUFFER;
		DEBUG_PLATFORM_ERROR(printf("PlatformError: push %d, new out: %d\n",error->errorType,s_errorIn);)
            return false;
	}
	void ClearErrors(){
		s_errorIn = 0;
		s_errorOut = 0;
	}
    
	void TestPlatformErrors(){
		PlatformError e1 = {1,""};
		PlatformError e2 = {2,""};
		PlatformError e3 = {3,""};
		PlatformError e4 = {4,""};
		PlatformError tmp;
        
		// Note: Set MAX_PLATFORM_BUFFER to 4 when testing
        
		//-- Empty case
		printf("--- Empty case ---\n");
		PollError(&tmp);
		PushError(&e1);
		PollError(&tmp);
		PollError(&tmp);
        
		ClearErrors();
        
		//-- Full case
		printf("--- Full case ---\n");
		PushError(&e1);
		PushError(&e2);
		PushError(&e3);
		PushError(&e4);
		PollError(&tmp);
		PushError(&e3);
        
		ClearErrors();
		
		//-- Normal case
		printf("--- Normal case ---\n");
		PushError(&e1);
		PollError(&tmp);
		PushError(&e2);
		PushError(&e3);
		PollError(&tmp);
        
		ClearErrors();
	}
    struct AllocInfo {
		std::string name;
		int count;
	};
	static std::unordered_map<u64, AllocInfo> allocTracking;
	
	void TrackType(u64 bytes, const std::string& name){
		auto pair = allocTracking.find(bytes);
		if(pair==allocTracking.end()){
			allocTracking[bytes] = {name,0};	
		} else {
			pair->second.name += "|";
			pair->second.name += name;
		}
	}
	#define ENGONE_TRACK_ALLOC 0
	#define ENGONE_TRACK_FREE 1
	#define ENGONE_TRACK_REALLOC 2
	static bool s_trackerEnabled=true;
	void PrintTracking(u64 bytes, int type){
		if(!s_trackerEnabled)
			return;
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
	
	
    // static std::mutex s_allocStatsMutex;
	static u64 s_totalAllocatedBytes=0;
	static u64 s_totalNumberAllocations=0;
	static u64 s_allocatedBytes=0;
	static u64 s_numberAllocations=0;

	struct Allocation {
		void* ptr=nullptr;
		u64 size=0;
	};
	static Allocation allocationsSlots[5000]; // nullptr indicates empty slot
	
	void SetTracker(bool on){
		s_trackerEnabled = on;
	}
	void* Allocate(u64 bytes){
		if(bytes==0) return nullptr;
		#ifndef NO_PERF
		// MEASURE
		#endif
		// void* ptr = HeapAlloc(GetProcessHeap(),0,bytes);
        void* ptr = malloc(bytes);
		if(!ptr) return nullptr;
		
		// Pri+ntTracking(bytes,ENGONE_TRACK_ALLOC);
		
		// s_allocStatsMutex.lock();
		s_allocatedBytes+=bytes;
		s_numberAllocations++;
		s_totalAllocatedBytes+=bytes;
		s_totalNumberAllocations++;			
		// s_allocStatsMutex.unlock();

		// for(auto& slot : allocationsSlots){
		// 	if(slot.ptr)
		// 		continue;
		// 	slot.ptr = ptr;
		// 	slot.size = bytes;
		// 	break;
		// }

		#ifdef LOG_ALLOCATIONS
		printf("* Allocate %lld, %p\n",bytes, ptr);
		#endif
		
		return ptr;
	}
    void* Reallocate(void* ptr, u64 oldBytes, u64 newBytes){
		#ifndef NO_PERF
		// MEASURE
		#endif
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
				if(!newPtr) {
					printf("Err %d\n",errno);
                    return nullptr;
				}
                
				if(allocTracking.size()!=0)
					printf("%s %llu -> %llu\n","realloc", oldBytes, newBytes);
				// PrintTracking(newBytes,ENGONE_TRACK_REALLOC);
                // s_allocStatsMutex.lock();
                s_allocatedBytes+=newBytes-oldBytes;
                
                s_totalAllocatedBytes+=newBytes;
                s_totalNumberAllocations++;			

				// for(auto& slot : allocationsSlots){
				// 	if(slot.ptr != ptr)
				// 		continue;
				// 	slot.ptr = newPtr;
				// 	slot.size = newBytes;
				// }

                // s_allocStatsMutex.unlock();
				#ifdef LOG_ALLOCATIONS
				printf("* Reallocate %lld -> %lld\n",oldBytes, newBytes);
				#endif
                return newPtr;
            }
        }
    }
	void Free(void* ptr, u64 bytes){
		if(!ptr) return;
		#ifndef NO_PERF
		// MEASURE
		#endif
		free(ptr);
		// HeapFree(GetProcessHeap(),0,ptr);
		// PrintTracking(bytes,ENGONE_TRACK_FREE);
		// s_allocStatsMutex.lock();
		s_allocatedBytes-=bytes;
		s_numberAllocations--;
		// Assert(s_allocatedBytes>=0);
		// Assert(s_numberAllocations>=0);

		// for(auto& slot : allocationsSlots){
		// 	if(slot.ptr != ptr)
		// 		continue;
		// 	slot.ptr = nullptr;
		// 	slot.size = 0;
		// 	break;
		// }

		// s_allocStatsMutex.unlock();
		#ifdef LOG_ALLOCATIONS
		printf("* Free %lld\n",bytes);
		#endif
	}
	u64 GetTotalAllocatedBytes(){
		return s_totalAllocatedBytes;
	}
	u64 GetTotalNumberAllocations(){
		return s_totalNumberAllocations;
	}
	u64 GetAllocatedBytes(){
		return s_allocatedBytes;
	}
	u64 GetNumberAllocations(){
		return s_numberAllocations;
	}
	void PrintRemainingTrackTypes(){
		for(auto& pair : allocTracking){
			if(pair.second.count!=0)
				printf(" %s (%llu bytes): %d left\n",pair.second.name.c_str(),pair.first,pair.second.count);	
		}
		for(auto& slot : allocationsSlots){
			if(slot.ptr){
				printf(" %p: %llu bytes\n",slot.ptr, slot.size);
			}
		}
	}
	static HANDLE m_consoleHandle = NULL; // possibly a bad idea
    void SetConsoleColor(uint16 color){
		if (m_consoleHandle == NULL) {
			m_consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			if (m_consoleHandle == NULL)
				return;
		}
		// TODO: don't set color if already set? difficult if you have a variable of last color and a different 
		//		function sets color without changing the variable.
		SetConsoleTextAttribute((HANDLE)m_consoleHandle, color);
	}
	int GetConsoleWidth() {
		HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO info;
		bool yes = GetConsoleScreenBufferInfo(handle, &info);
		if(!yes) {
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] GetConsoleScreenBufferInfo\n",err);
			return 0;	
		}
		return info.dwSize.X;
	}
    Semaphore::Semaphore(u32 initial, u32 max) {
		Assert(!m_internalHandle);
		m_initial = initial;
		m_max = max;
	}
	void Semaphore::init(u32 initial, u32 max) {
		Assert(!m_internalHandle);
		m_initial = initial;
		m_max = max;
		HANDLE handle = CreateSemaphore(NULL, m_initial, m_max, NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] CreateSemaphore\n",err);
		} else {
			m_internalHandle = TO_INTERNAL(handle);
		}
	}
	void Semaphore::cleanup() {
		if (m_internalHandle != 0){
            BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
            if(!yes){
                DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] CloseHandle\n",err);
            }
			m_internalHandle=0;
        }
	}
	void Semaphore::wait() {
		// MEASURE
		if (m_internalHandle == 0) {
			HANDLE handle = CreateSemaphore(NULL, m_initial, m_max, NULL);
			if (handle == INVALID_HANDLE_VALUE) {
				DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] CreateSemaphore\n",err);
			} else {
                m_internalHandle = TO_INTERNAL(handle);
			}
		}
		if (m_internalHandle != 0) {
			DWORD res = WaitForSingleObject(TO_HANDLE(m_internalHandle), INFINITE);
			if (res == WAIT_FAILED) {
				DWORD err = GetLastError();
				PL_PRINTF("[WinError %lu] WaitForSingleObject\n",err);
			}
		}
	}
	void Semaphore::signal(int count) {
		// MEASURE
		if (m_internalHandle != 0) {
			BOOL yes = ReleaseSemaphore(TO_HANDLE(m_internalHandle), count, NULL);
			if (!yes) {
				DWORD err = GetLastError();
				if(err == ERROR_TOO_MANY_POSTS)
					PL_PRINTF("[WinError %lu] ReleaseSemaphore, to many posts\n",err);
				else
					PL_PRINTF("[WinError %lu] ReleaseSemaphore\n",err);
			}
		}
	}
	void Mutex::cleanup() {
        if (m_internalHandle != 0){
            BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
            if(!yes){
                DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] CloseHandle\n",err);
            }
			m_internalHandle=0;
        }
	}
	void Mutex::lock() {
		// MEASURE
		if (m_internalHandle == 0) {
			HANDLE handle = CreateMutex(NULL, false, NULL);
			if (handle == INVALID_HANDLE_VALUE) {
				DWORD err = GetLastError();
				PL_PRINTF("[WinError %lu] CreateMutex\n",err);
			}else
                m_internalHandle = TO_INTERNAL(handle);
		}
		if (m_internalHandle != 0) {
			DWORD res = WaitForSingleObject(TO_HANDLE(m_internalHandle), INFINITE);
			uint32 newId = Thread::GetThisThreadId();
			auto owner = m_ownerThread;
			// printf("Lock %d %d\n",newId, (int)m_internalHandle);
			if (owner != 0) {
				PL_PRINTF("Mutex : Locking twice, old owner: %llu, new owner: %u\n",owner,newId);
			}
			m_ownerThread = newId;
			if (res == WAIT_FAILED) {
                // TODO: What happened do the old thread who locked the mutex. Was it okay to ownerThread = newId
				DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] WaitForSingleObject\n",err);
			}
		}
	}
	void Mutex::unlock() {
		// MEASURE
		if (m_internalHandle != 0) {
			// printf("Unlock %d %d\n",m_ownerThread, (int)m_internalHandle);
			m_ownerThread = 0;
			BOOL yes = ReleaseMutex(TO_HANDLE(m_internalHandle));
			if (!yes) {
				DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] ReleaseMutex\n",err);
			}
		}
	}
	ThreadId Mutex::getOwner() {
		return m_ownerThread;
	}
	void Thread::cleanup() {
		if (m_internalHandle) {
			if (m_threadId == GetThisThreadId()) {
                PL_PRINTF("Thread : Thread cannot clean itself\n");
				return;
			}
            BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
            if(!yes){
                DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] CloseHandle\n",err);
            }
			m_internalHandle=0;
		}
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
	DWORD WINAPI SomeThreadProc(void* ptr){
		DataForThread* data = (DataForThread*)ptr;
		uint32 ret = data->func(data->userData);
		DataForThread::Destroy(data);
		return ret;
	}
	u32 Thread::CreateTLSIndex() {
		DWORD ind = TlsAlloc();
		if(ind == TLS_OUT_OF_INDEXES) {
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] TlsAlloc\n",err);
		}
		return ind+1;
	}
	bool Thread::DestroyTLSIndex(u32 index) {
		bool yes = TlsFree((DWORD)(index-1));
		if(!yes) {
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] TlsFree\n",err);
		}
		return yes;
	}
	void* Thread::GetTLSValue(u32 index){
		void* ptr = TlsGetValue((DWORD)(index-1));
		DWORD err = GetLastError();
		if(err!=ERROR_SUCCESS){
			PL_PRINTF("[WinError %lu] TlsGetValue, invalid index\n",err);
		}
		return ptr;
	}
	bool Thread::SetTLSValue(u32 index, void* ptr){
		bool yes = TlsSetValue((DWORD)(index-1), ptr);
		if(!yes) {
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] TlsSetValue, invalid index\n",err);
		}
		return yes;
	}
	void Thread::init(uint32(*func)(void*), void* arg) {
		if (!m_internalHandle) {
			// const uint32 stackSize = 1024*1024;
			// Casting func to LPTHREAD_START_ROUTINE like this is questionable.
			// func doesn't use a specific call convention while LPTHREAD uses __stdcall.
			// HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0,(DWORD*)&m_threadId);

			// SomeThreadProc is correct, we create a little struct to keep the func ptr and arg in.
			auto ptr = DataForThread::Create();
			ptr->func = func;
			ptr->userData = arg;
			
			HANDLE handle = CreateThread(NULL, 0, SomeThreadProc, ptr, 0,(DWORD*)&m_threadId);
			if (handle==INVALID_HANDLE_VALUE) {
				DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] CreateThread\n",err);
			}else
                m_internalHandle = TO_INTERNAL(handle);
		}
	}
	void Thread::join() {
		if (!m_internalHandle)
			return;
		DWORD res = WaitForSingleObject(TO_HANDLE(m_internalHandle), INFINITE);
		if (res==WAIT_FAILED) {
			DWORD err = GetLastError();
            PL_PRINTF("[WinError %lu] WaitForSingleObject\n",err);
		}
		BOOL yes = CloseHandle(TO_HANDLE(m_internalHandle));
        if(!yes){
            DWORD err = GetLastError();
            PL_PRINTF("[WinError %lu] CloseHandle\n",err);
        }
		m_threadId = 0;
		m_internalHandle = 0;
	}
	bool Thread::isActive() {
		return m_internalHandle!=0;
	}
	bool Thread::joinable() {
		return m_threadId!=0 && m_threadId != GetCurrentThreadId();
	}
	ThreadId Thread::getId() {
		return m_threadId;
	}
	ThreadId Thread::GetThisThreadId() {
		return GetCurrentThreadId();
	}
	std::string GetWorkingDirectory(){
		DWORD length = GetCurrentDirectoryA(0,nullptr);
		if(length==0){
			DWORD err = GetLastError();
			printf("[WinError %lu] GetWorkingDirectory\n",err);
			return "";
		} else {
			std::string out{};
			out.resize(length-1); // -1 to exclude \0
			length = GetCurrentDirectoryA(length,(char*)out.data());
			if(length==0){
				DWORD err = GetLastError();
				printf("[WinError %lu] GetWorkingDirectory\n",err);
				return "";
			}
			for(int i=0;i<out.length();i++){
				if(out[i] == '\\')
					((char*)out.data())[i] = '/';
			}
			return out;
		}
	}
	bool SetWorkingDirectory(const std::string& path){
		bool yes = SetCurrentDirectoryA(path.c_str());
		if(!yes) {
			DWORD err = GetLastError();
			printf("[WinError %lu] SetWorkingDirectory '%s'\n",err, path.c_str());
			return false;
		}
		return true;
	}
	void* LoadDynamicLibrary(const std::string& path){
		HMODULE module = LoadLibraryA(path.c_str());
		if (module == NULL) {
			DWORD err = GetLastError();
			printf("[WinError %lu] LoadDynamicLibrary: %s\n",err,path.c_str());
		}
		return module;
	}
	void UnloadDynamicLibrary(void* library){
		int yes = FreeLibrary((HMODULE)library);
		if (!yes) {
			DWORD err = GetLastError();
			printf("[WinError %lu] UnloadDynamicLibrary\n",err);
		}
	}
	VoidFunction GetFunctionPointer(void* library, const std::string& name){
		FARPROC proc = GetProcAddress((HMODULE)library, name.c_str());
		if (proc == NULL) {
			int err = GetLastError();
			if(err==ERROR_PROC_NOT_FOUND){
				printf("[WinError %u] GetFunctionPointer, could not find '%s'\n", err, name.c_str());
			}else
				printf("[WinError %u] GetFunctionPointer %s\n", err, name.c_str());
		}
		return (VoidFunction)proc;
	}
    
	void ConvertArguments(int& argc, char**& argv) {
		LPWSTR wstr = GetCommandLineW();
        
		wchar_t** wargv = CommandLineToArgvW(wstr, &argc);
        
		if (wargv == NULL) {
			int err = GetLastError();
			printf("ConvertArguments : WinError %d!\n",err);
		} else {
			// argv will become a pointer to contigous memory which contain arguments.
			int totalSize = argc * sizeof(char*);
			int index = totalSize;
			for (int i = 0; i < argc; i++) {
				int length = wcslen(wargv[i]);
				//printf("len: %d\n", length);
				totalSize += length + 1;
			}
			//printf("size: %d index: %d\n", totalSize,index);
			argv = (char**)Allocate(totalSize);
			char* argData = (char*)argv + index;
			if (!argv) {
				printf("ConvertArguments : Allocation failed!\n");
			} else {
				index = 0;
				for (int i = 0; i < argc; i++) {
					int length = wcslen(wargv[i]);
                    
					argv[i] = argData + index;
                    
					for (int j = 0; j < length; j++) {
						argData[index] = wargv[i][j];
						index++;
					}
					argData[index++] = 0;
				}
			}
		}
		LocalFree(wargv);
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
			argv[argc] = nullptr; // terminate pointer array with a nullptr, this is an optional way apart from "argc" to find the end of the array.
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
		int totalSize = (argc +1) * sizeof(char*);  // argc +1 because the array is terminated with a nullptr (Unix execv needs it). Not needed in Windows implementation but it's good to be consistent.
		int index = totalSize;
		for (int i = 0; i < argc; i++) {
			int length = strlen(argv[i]);
			//printf("len: %d\n", length);
			totalSize += length + 1;
		}
		Free(argv, totalSize);
	}
	void CreateConsole() {
		bool b = AllocConsole();
		if (b) {
			freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
			freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
		}
	}
	std::string EnvironmentVariable(const std::string& name){
		std::string buffer{};
		DWORD length = GetEnvironmentVariableA(name.c_str(),(char*)buffer.data(),0);
		if(length==0){
			DWORD err = GetLastError();
			if(err==ERROR_ENVVAR_NOT_FOUND){
				printf("[WinError %lu] EnvironmentVariable 1, %s not found\n",err,name.c_str());
			}else{
				printf("[WinError %lu] EnvironmentVariable 1, %s\n",err,name.c_str());
			}
			return "";
		}
		buffer.resize(length-1);
		length = GetEnvironmentVariableA(name.c_str(),(char*)buffer.data(),length);
		
		// printf("%lu != %lu\n",length,buffer.length());
		if(length != buffer.length()){
			DWORD err = GetLastError();
			printf("[WinError %lu] EnvironmentVariable 2, %s\n",err,name.c_str());
			return ""; // failed
		}
		return buffer;
	}
	struct PipeInfo{
		HANDLE readH=0;	
		HANDLE writeH=0;	
	};
	std::unordered_map<u64,PipeInfo> pipes;
	u64 pipeIndex=0x500000;
	APIPipe PipeCreate(u64 pipeBuffer, bool inheritRead,bool inheritWrite){
		PipeInfo info{};
		
		SECURITY_ATTRIBUTES saAttr; 
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		saAttr.bInheritHandle = inheritRead||inheritWrite;
		saAttr.lpSecurityDescriptor = NULL;
		
		DWORD err = CreatePipe(&info.readH,&info.writeH,&saAttr,pipeBuffer);
		if(!err){
			err = GetLastError();
			PL_PRINTF("[WinError %lu] Cannot create pipe\n",err);
			return {};
		}
		
		if(!inheritRead){
			err = SetHandleInformation(info.readH, HANDLE_FLAG_INHERIT, 0);
			if(!err){
				err = GetLastError();
				PL_PRINTF("[WinError %lu] Pipe setting information failed\n",err);	
			}
		}
		if(!inheritWrite){
			err = SetHandleInformation(info.writeH, HANDLE_FLAG_INHERIT, 0);
			if(!err){
				err = GetLastError();
				PL_PRINTF("[WinError %lu] Pipe setting information failed\n",err);	
			}
		}
		
		pipes[pipeIndex] = info;
		return {pipeIndex++};
	}
	void PipeDestroy(APIPipe pipe){
		auto& info = pipes[pipe.internal];
		if(info.readH)
			CloseHandle(info.readH);
		if(info.writeH)
			CloseHandle(info.writeH);
		pipes.erase(pipe.internal);
	}
	u64 PipeRead(APIPipe pipe,void* buffer, u64 size){
		auto& info = pipes[pipe.internal];
		DWORD read=0;
		DWORD err = ReadFile(info.readH,buffer,size,&read,NULL);
		if(!err){
			err = GetLastError();
			if(err==ERROR_BROKEN_PIPE){
				return 0; // pipe is closed, nothing to read
			}
			PL_PRINTF("[WinError %lu] Pipe read failed\n",err);
			return 0;
		}
		
		return read;
	}
	u64 PipeWrite(APIPipe pipe,void* buffer, u64 size){
		auto& info = pipes[pipe.internal];
		DWORD written=0;
		DWORD err = WriteFile(info.writeH,buffer,size,&written,NULL);
		if(!err){
			err = GetLastError();
			PL_PRINTF("[WinError %lu] Pipe write failed\n",err);
			return 0;
		}
		return written;
	}
	APIFile PipeGetRead(APIPipe pipe){
		auto& info = pipes[pipe.internal];
		return {TO_INTERNAL(info.readH)};
	}
	APIFile PipeGetWrite(APIPipe pipe){
		auto& info = pipes[pipe.internal];
		return {TO_INTERNAL(info.writeH)};
	}
	bool SetStandardOut(APIFile file){
		DWORD res = SetStdHandle(STD_OUTPUT_HANDLE, TO_HANDLE(file.internal));
		return res;
	}
	APIFile GetStandardOut(){
		return {TO_INTERNAL(GetStdHandle(STD_OUTPUT_HANDLE))};
	}
	bool SetStandardErr(APIFile file){
		DWORD res = SetStdHandle(STD_ERROR_HANDLE, TO_HANDLE(file.internal));
		return res;
	}
	APIFile GetStandardErr(){
		return {TO_INTERNAL(GetStdHandle(STD_ERROR_HANDLE))};
	}
	bool SetStandardIn(APIFile file){
		DWORD res = SetStdHandle(STD_INPUT_HANDLE, TO_HANDLE(file.internal));
		return res;
	}
	APIFile GetStandardIn(){
		return {TO_INTERNAL(GetStdHandle(STD_INPUT_HANDLE))};
	}
	bool ExecuteCommand(const std::string& cmd, bool asynchonous, int* exitCode){
		if(cmd.length() == 0) return false;

		auto pipe = PipeCreate(100 + cmd.length(), true, true);

		// additional information
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		// set the size of the structures
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
        
		bool inheritHandles=true;
		 
		DWORD createFlags = 0;

		HANDLE prev = GetStdHandle(STD_INPUT_HANDLE);
		SetStdHandle(STD_INPUT_HANDLE, TO_HANDLE(PipeGetRead(pipe).internal));
			
		PipeWrite(pipe, (void*)cmd.data(), cmd.length());

		BOOL yes = CreateProcessA(NULL,   // the path
			(char*)cmd.data(),        // Command line
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			inheritHandles,          // Set handle inheritance
			createFlags,              // creation flags
			NULL,           // Use parent's environment block
			NULL,   // starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
			);
		
		SetStdHandle(STD_INPUT_HANDLE, prev);

		if(!asynchonous){
			WaitForSingleObject(pi.hProcess,INFINITE);
			if(exitCode){
				BOOL success = GetExitCodeProcess(pi.hProcess, (DWORD*)exitCode);
				if(!success){
					DWORD err = GetLastError();
					printf("[WinError %lu] ExecuteCommand, failed aquring exit code (path: %s)\n",err,cmd.c_str());	
				}else{
					if(success==STILL_ACTIVE){
						printf("[WinWarning] ExecuteCommand, cannot get exit code, process is active (path: %s)\n",cmd.c_str());	
					}else{
						
					}
				}
			}
		}

		PipeDestroy(pipe);
		
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		return true;
	}
	bool StartProgram(char* commandLine, u32 flags, int* exitCode, APIFile fStdin, APIFile fStdout, APIFile fStderr) {
		// if (!FileExist(path)) {
		// 	return false;
		// }
		
		if(!commandLine || strlen(commandLine) == 0){
			return false;
		}
        
		// additional information
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		// set the size of the structures
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
        
		if(fStdin){
			// auto& info = pipes[fStdin];
			// si.hStdInput = info.readH;
			si.hStdInput = TO_HANDLE(fStdin.internal);
		}
		if(fStdout){
			// auto& info = pipes[fStdout];
			// si.hStdOutput = info.writeH;
			// si.hStdError = info.writeH;
			si.hStdOutput = TO_HANDLE(fStdout.internal);
		}
		if(fStderr){
			// auto& info = pipes[fStderr];
			// si.hStdError = info.writeH;
			si.hStdError = TO_HANDLE(fStderr.internal);
		}
		bool inheritHandles=true;
		if(fStdin||fStdout||fStderr){
			si.dwFlags |= STARTF_USESTDHANDLES;
			inheritHandles = true;
		}
		// inheritHandles = false;
		 
		// int slashIndex = path.find_last_of("\\");
    
		// std::string workingDir{};
		const char* dir = NULL;
		// if(slashIndex!=-1)
		// 	workingDir = path.substr(0, slashIndex);
		// else
		// 	workingDir = GetWorkingDirectory();
        
		// if(!workingDir.empty())
		// 	dir = workingDir.c_str();
			
		DWORD createFlags = 0;
		if(flags&PROGRAM_NEW_CONSOLE)
			createFlags |= CREATE_NEW_CONSOLE;
			
		BOOL yes = CreateProcessA(NULL,   // the path
                       commandLine,        // Command line
                       NULL,           // Process handle not inheritable
                       NULL,           // Thread handle not inheritable
                       inheritHandles,          // Set handle inheritance
                       createFlags,              // creation flags
                       NULL,           // Use parent's environment block
                       dir,   // starting directory 
                       &si,            // Pointer to STARTUPINFO structure
                       &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
                       );
        if(!yes){
			DWORD err = GetLastError();
			if(err == ERROR_BAD_EXE_FORMAT){
				printf("[WinError %lu] StartProgram, bad_exe_format %s\n",err,commandLine);
			}else{
				printf("[WinError %lu] StartProgram, could not start %s\n",err,commandLine);
				// TODO: does the handles need to be closed? probably not since CreateProcess failed. double check.	
			}
			return false;
		}
		if(flags&PROGRAM_WAIT){
			WaitForSingleObject(pi.hProcess,INFINITE);
			if(exitCode){
				BOOL success = GetExitCodeProcess(pi.hProcess, (DWORD*)exitCode);
				if(!success){
					DWORD err = GetLastError();
					printf("[WinError %lu] StartProgram, failed acquring exit code (path: %s)\n",err,commandLine);	
				}else{
					if(success==STILL_ACTIVE){
						printf("[WinWarning] StartProgram, cannot get exit code, process is active (path: %s)\n",commandLine);	
					}else{
						
					}
				}
			}
		}else{
			// TODO: what happens with return value when we don't wait for the process?	
		}
		
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		
		return true;
	}
	FileMonitor::~FileMonitor() { cleanup(); }
	void FileMonitor::cleanup() {
		m_mutex.lock();
		m_running = false;
		if (m_thread.joinable()) {
			HANDLE handle = TO_HANDLE(m_thread.m_internalHandle);
			int err = CancelSynchronousIo(handle);
			if (err == 0) {
				err = GetLastError();
				// ERROR_NOT_FOUND
				if (err != ERROR_NOT_FOUND)
				printf("FileMontitor : FIX LOGGING!\n");
					// log::out << log::RED << "FileMonitor::cleanup - err " << err << "\n";
			}
		}
		if (m_changeHandle != NULL) {
			FindCloseChangeNotification(m_changeHandle);
			m_changeHandle = NULL;
		}
		if (m_dirHandle != NULL) {
			CloseHandle(m_dirHandle);
			m_dirHandle = NULL;
		}
		m_mutex.unlock();

		if (m_thread.joinable())
			m_thread.join();
		//m_threadHandle = NULL;
		if (m_buffer) {
			Free(m_buffer, m_bufferSize);
			m_buffer = nullptr;
		}
		// m_running is set to false in thread
	}
	uint32 RunMonitor(void* arg){
		static const uint32 INITIAL_SIZE = 5 * (sizeof(FILE_NOTIFY_INFORMATION) + 500);
	
		FileMonitor* self = (FileMonitor*)arg;
		SetThreadName(GetCurrentThreadId(), "FileMonitor");
		//m_threadHandle = GetCurrentThread();
		std::string temp;
		DWORD waitStatus;
		while (true) {
			waitStatus = WaitForSingleObject(self->m_changeHandle, INFINITE);

			if (!self->m_running)
				break;
			if (waitStatus == WAIT_OBJECT_0) {
				//log::out << "FileMonitor::check - catched " << m_root << "\n";
			
				if (!self->m_buffer) {
					self->m_buffer = (FILE_NOTIFY_INFORMATION*)Allocate(INITIAL_SIZE);
					if (!self->m_buffer) {
						self->m_bufferSize = 0;
						printf("FileMontitor : FIX LOGGING!\n");
						// log::out << log::RED << "FileMonitor::check - buffer allocation failed\n";
						break;
					}
					self->m_bufferSize = INITIAL_SIZE;
				}

				DWORD bytes = 0;
				BOOL success = ReadDirectoryChangesW(self->m_dirHandle, self->m_buffer, self->m_bufferSize, self->m_flags & FileMonitor::WATCH_SUBTREE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME, &bytes, NULL, NULL);
				//log::out << "ReadDir change\n";
				if (bytes == 0) {
					// try to read changes again but with bigger buffer? while loop?
					//log::out << log::RED << "FileMonitor::check - buffer to small or big\n";
					// this could also mean that we cancelled the read.
				}
				if (success == 0) {
					int err = GetLastError();

					// ERROR_INVALID_HANDLE
					// ERROR_INVALID_PARAMETER
					// ERROR_INVALID_FUNCTION
					// ERROR_OPERATION_ABORTED

					if (err != ERROR_OPERATION_ABORTED)
						// this could also mean that we cancelled the read.
						printf("FileMontitor : FIX LOGGING!\n");
						// log::out << log::RED << "FileMonitor::check - ReadDirectoryChanges win error " << err << " (path: " << self->m_root << ")\n";
					break;
				} else {
					int offset = 0;
					int limit = 1000;
					while (limit--) {
						FILE_NOTIFY_INFORMATION& info = *(FILE_NOTIFY_INFORMATION*)((char*)self->m_buffer + offset);
						int length = info.FileNameLength / sizeof(WCHAR);
						if (length < MAX_PATH + 1) {
							temp.resize(length);
							for (int i = 0; i < length; i++) {
								((char*)temp.data())[i] = (char)*(info.FileName + i);
							}
						}

						if (self->m_dirPath == self->m_root || temp == self->m_root) {
							//log::out << "FileMonitor::check - call callback " << temp << "\n";
							FileMonitor::ChangeType type = (FileMonitor::ChangeType)0;
							if (info.Action == FILE_ACTION_MODIFIED || info.Action == FILE_ACTION_ADDED) type = FileMonitor::FILE_MODIFIED;
							if (info.Action == FILE_ACTION_REMOVED) type = FileMonitor::FILE_REMOVED;
							if (type == 0) {
								printf("FileMontitor : FIX LOGGING!\n");
								// log::out << log::YELLOW << "FileMonitor - type was 0 (info.Action=" << (int)info.Action << ")\n";
								//DebugBreak();
							}
							self->m_callback(temp, type);
						}

						if (info.NextEntryOffset == 0)
							break;
						offset += info.NextEntryOffset;
					}
				}

				self->m_mutex.lock();
				bool yes = FindNextChangeNotification(self->m_changeHandle);
				self->m_mutex.unlock();

				if (!yes) {
					// handle could have been closed
					break;
				}
			} else {
				// not sure why we are here but it isn't good so stop.
				break;
			}
		}
		self->m_mutex.lock();
		self->m_running = false;

		if (self->m_changeHandle) {
			FindCloseChangeNotification(self->m_changeHandle);
			self->m_changeHandle = NULL;
		}

		if (self->m_dirHandle) {
			CloseHandle(self->m_dirHandle);
			self->m_dirHandle = NULL;
		}
		self->m_root.clear();
		self->m_mutex.unlock();
		//log::out << "FileMonitor - finished thread\n";
		return 0;
	}
	// TODO: handle is checked against NULL, it should be checked against INVALID_HANDLE_VALUE
	bool FileMonitor::check(const std::string& path, void(*callback)(const std::string&, uint32), uint32 flags) {
	// bool FileMonitor::check(const std::string& path, std::function<void(const std::string&, uint32)> callback, uint32 flags) {
		
		if(!FileExist(path))
			return false;
		//log::out << log::RED << "FileMonitor::check - invalid path : " << m_root << "\n";

		m_mutex.lock();

		// Just a heads up for you. This functions is not beautiful.

		if (m_root == path) { // no reason to check the same right?
			m_mutex.unlock();
			return true;
		}
		//if (m_threadHandle) {
		//	CancelSynchronousIo(m_threadHandle);
		//}
		if (m_changeHandle != NULL) {
			FindCloseChangeNotification(m_changeHandle);
			m_changeHandle = NULL;
		}
		if (m_dirHandle) {
			CloseHandle(m_dirHandle);
			m_dirHandle = NULL;
		}

		if (!m_running) {
			if (m_thread.isActive()) // TODO: What if
				m_thread.join();
			//m_threadHandle = NULL;

			m_root = path;
			m_callback = callback;
			m_flags = flags;

			DWORD attributes = GetFileAttributesA(m_root.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES) {

			} else if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
				m_dirPath = m_root;
			} else {
				int index = m_root.find_last_of('\\');
				if (index == -1) {
					m_dirPath = ".\\";
				} else {
					m_dirPath = m_root.substr(0, index);
				}
			}
			m_changeHandle = FindFirstChangeNotificationA(m_dirPath.c_str(), flags & WATCH_SUBTREE,
				FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
			if (m_changeHandle == INVALID_HANDLE_VALUE || m_changeHandle == NULL) {
				m_changeHandle = NULL;
				DWORD err = GetLastError();
				printf("FileMontitor : FIX LOGGING!\n");
				// log::out << log::RED << "FileMonitor::check - invalid handle (err: " << (int)err << "): " << m_dirPath << "\n";
				m_mutex.unlock();
				return false;
			}

			//FILE_FLAG_OVERLAPPED
			m_dirHandle = CreateFileA(m_dirPath.c_str(), FILE_LIST_DIRECTORY,
				FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
			if (m_dirHandle == NULL || m_dirHandle == INVALID_HANDLE_VALUE) {
				m_dirHandle = NULL;
				DWORD err = GetLastError();
				printf("FileMontitor : FIX LOGGING!\n");
				// log::out << log::RED << "FileMonitor::check - dirHandle failed(" << (int)err << "): " << m_dirPath << "\n";
				if (m_changeHandle != NULL) {
					FindCloseChangeNotification(m_changeHandle);
					m_changeHandle = NULL;
				}
				m_mutex.unlock();
				return false;
			}
			m_running = true;

			//log::out << "FileMonitor : Checking " << m_root << "\n";
			m_thread.init(RunMonitor,this);
		}
		m_mutex.unlock();
		return true;
	}
#ifdef VISUAL_STUDIO
	// Code below comes from https://learn.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2022
	const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)
	void SetThreadName(ThreadId threadId, const char* threadName) {
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = threadName;
		info.dwThreadID = threadId;
		info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
		__try {
			RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
		}
#pragma warning(pop)
	}
#else
	void SetThreadName(ThreadId threadId, const char* threadName) {
		// Empty
	}
#endif
}
#endif