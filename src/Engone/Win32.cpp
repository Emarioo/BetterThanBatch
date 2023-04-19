#include "Engone/PlatformLayer.h"

#ifdef WIN32

#ifdef PL_PRINT_ERRORS
#define PL_PRINTF printf
#else
#define PL_PRINTF
#endif

#include <unordered_map>
#include <vector>
#include <string>

// #define NOMINMAX
// #define _WIN32_WINNT 0x0601
// #define WIN32_LEAN_AND_MEAN
// #include <windows.h>

// #include <mutex>

#include "Engone/Win32Includes.h"

// Name collision
auto Win32Sleep = Sleep;

namespace engone {
    //-- Platform specific
    
#define TO_INTERNAL(X) (void*)((uint64)X+1)
#define TO_HANDLE(X) (HANDLE)((uint64)X-1)
    
	DirectoryIterator* DirectoryIteratorCreate(const std::string& path, DirectoryIteratorData* result){		
		WIN32_FIND_DATAA data;
		std::string _path;
		if(path.empty())
			_path = path+"*";
		else
			_path = path+"\\*";
		HANDLE handle = FindFirstFileA(_path.c_str(),&data);
		if(handle==INVALID_HANDLE_VALUE){
			DWORD err = GetLastError();
			if(err == ERROR_FILE_NOT_FOUND){
				PL_PRINTF("[WinError %lu] Cannot find file '%s'\n",err,path.c_str());
			}else if(err==ERROR_PATH_NOT_FOUND){
				PL_PRINTF("[WinError %lu] Cannot find path '%s'\n",err,path.c_str());
			}else {
				PL_PRINTF("[WinError %lu] Error opening '%s'\n",err,path.c_str());
			}
			return 0;
		}
		while(true){
			if(strcmp(data.cFileName,".")==0||strcmp(data.cFileName,"..")==0){
				BOOL success = FindNextFileA(handle,&data);
				if(!success){
					DWORD err = GetLastError();
					if(err == ERROR_NO_MORE_FILES){
						// PL_PRINTF("[WinError %u] No files '%lu'\n",err,(uint64)iterator);
					}else {
						PL_PRINTF("[WinError %lu] Error iterating '%llu'\n",err,(uint64)handle);
					}
					BOOL success = FindClose(handle);
					if(!success){
						DWORD err = GetLastError();
						PL_PRINTF("[WinError %lu] Error closing '%llu'\n",err,(uint64)handle);
					}
					return 0;
				}
				continue;
			}
			break;
		}
		// PL_PRINTF("F1: %s\n",data.cFileName);
		// PL_PRINTF("F2: %s\n",data.cAlternateFileName);
		result->name = data.cFileName;
		result->fileSize = (uint64)data.nFileSizeLow+(uint64)data.nFileSizeHigh*((uint64)MAXDWORD+1);
		uint64 time = (uint64)data.ftLastWriteTime.dwLowDateTime+(uint64)data.ftLastWriteTime.dwHighDateTime*((uint64)MAXDWORD+1);
		result->lastWriteSeconds = time/10000000.f; // 100-nanosecond intervals
		result->isDirectory = data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY;
		
		return (DirectoryIterator*)((uint64)handle+1); // +1 in case 0 is a valid handle. Could not find anything about this
	}
	bool DirectoryIteratorNext(DirectoryIterator* iterator, DirectoryIteratorData* result){
		WIN32_FIND_DATAA data;
		while(true){
			BOOL success = FindNextFileA((HANDLE)((uint64)iterator-1),&data);
			if(!success){
				DWORD err = GetLastError();
				if(err == ERROR_NO_MORE_FILES){
					// take one from directory and do shit.
					// PL_PRINTF("[WinError %u] No files '%lu'\n",err,(uint64)iterator);
				}else {
					PL_PRINTF("[WinError %lu] Error iterating '%llu'\n",err,(uint64)iterator);
				}
				return false;
			}
			if(strcmp(data.cFileName,".")==0||strcmp(data.cFileName,"..")==0){
				continue;
			}
			break;
		}
		
		result->name = data.cFileName;
		result->fileSize = (uint64)data.nFileSizeLow+(uint64)data.nFileSizeHigh*((uint64)MAXDWORD+1);
		uint64 time = (uint64)data.ftLastWriteTime.dwLowDateTime+(uint64)data.ftLastWriteTime.dwHighDateTime*((uint64)MAXDWORD+1);
		result->lastWriteSeconds = time/10000000.f; // 100-nanosecond intervals
		result->isDirectory = data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY;
		return true;
	}
	void DirectoryIteratorDestroy(DirectoryIterator* iterator){
		BOOL success = FindClose((HANDLE)((uint64)iterator-1));
		if(!success){
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] Error closing '%llu'\n",err,(uint64)iterator);
		}
	}
    
	// Recursive directory iterator info
	struct RDIInfo{
		std::string root;
		std::string dir;
		HANDLE handle;
		std::vector<std::string> directories;
	};
	static std::unordered_map<RecursiveDirectoryIterator*,RDIInfo> s_rdiInfos;
	static uint64 s_uniqueRDI=0;
	
	RecursiveDirectoryIterator* RecursiveDirectoryIteratorCreate(const std::string& path){
		RecursiveDirectoryIterator* iterator = (RecursiveDirectoryIterator*)(++s_uniqueRDI);
		auto& info = s_rdiInfos[iterator] = {};
		info.root = path;
		info.handle=INVALID_HANDLE_VALUE;
		info.directories.push_back(path);
		
		// bool success = RecursiveDirectoryIteratorNext(iterator,result);
		// if(!success){
		// 	RecursiveDirectoryIteratorDestroy(iterator);
		// 	return 0;
		// }
		return iterator;
	}
	bool RecursiveDirectoryIteratorNext(RecursiveDirectoryIterator* iterator, DirectoryIteratorData* result){
		auto info = s_rdiInfos.find(iterator);
		if(info==s_rdiInfos.end()){
			return false;
		}
		
		WIN32_FIND_DATAA data;
		while(true){
			if(info->second.handle==INVALID_HANDLE_VALUE){
				if(info->second.directories.empty()){
					return false;
				}
                info->second.dir.clear();
                
				if(!info->second.directories[0].empty()){
                    info->second.dir += info->second.directories[0];
				}
                
                std::string temp = info->second.dir;
                if(!temp.empty())
                    temp += "\\";
                
                temp+="*";
				// printf("FindFirstFile %s\n",temp.c_str());
				info->second.directories.erase(info->second.directories.begin());
				HANDLE handle = FindFirstFileA(temp.c_str(),&data);
				
				if(handle==INVALID_HANDLE_VALUE){
					DWORD err = GetLastError();
					PL_PRINTF("[WinError %lu] FindNextFileA '%llu'\n",err,(uint64)iterator);
					continue;
				}
				info->second.handle = handle;
			}else{
				BOOL success = FindNextFileA(info->second.handle,&data);
				if(!success){
					DWORD err = GetLastError();
					if(err == ERROR_NO_MORE_FILES){
						// PL_PRINTF("[WinError %u] No files '%lu'\n",err,(uint64)iterator);
					}else {
						PL_PRINTF("[WinError %lu] FindNextFileA '%llu'\n",err,(uint64)iterator);
					}
					bool success = FindClose(info->second.handle);
					info->second.handle = INVALID_HANDLE_VALUE;
					if(!success){
						err = GetLastError();
						PL_PRINTF("[WinError %lu] FindClose '%llu'\n",err,(uint64)iterator);
					}
					continue;
				}
			}
			if(strcmp(data.cFileName,".")==0||strcmp(data.cFileName,"..")==0){
				continue;
			}
			break;
		}
		result->name.clear();
		if(!info->second.dir.empty())
			result->name += info->second.dir+"\\";
		result->name += data.cFileName;
		
        // printf("f: %s\n",result->name.c_str());
        
		result->fileSize = (uint64)data.nFileSizeLow+(uint64)data.nFileSizeHigh*((uint64)MAXDWORD+1);
		uint64 time = (uint64)data.ftLastWriteTime.dwLowDateTime+(uint64)data.ftLastWriteTime.dwHighDateTime*((uint64)MAXDWORD+1);
		result->lastWriteSeconds = time/10000000.f; // 100-nanosecond intervals
		result->isDirectory = data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY;
		if(result->isDirectory){
            info->second.directories.push_back(result->name);
        }
		return true;
	}
	void RecursiveDirectoryIteratorSkip(RecursiveDirectoryIterator* iterator){
		auto info = s_rdiInfos.find(iterator);
		if(info==s_rdiInfos.end()){
			return;
		}
		if(!info->second.directories.empty())
			info->second.directories.pop_back();
	}
	void RecursiveDirectoryIteratorDestroy(RecursiveDirectoryIterator* iterator){
		auto info = s_rdiInfos.find(iterator);
		if(info==s_rdiInfos.end()){
			return;
		}
		
		if(info->second.handle!=INVALID_HANDLE_VALUE){
			BOOL success = FindClose(info->second.handle);
			if(!success){
				DWORD err = GetLastError();
				PL_PRINTF("[WinError %lu] Error closing '%llu'\n",err,(uint64)iterator);
			}
		}
		s_rdiInfos.erase(iterator);
	}
    
	TimePoint MeasureSeconds(){
		uint64 tp;
		BOOL success = QueryPerformanceCounter((LARGE_INTEGER*)&tp);
		// if(!success){
		// 	PL_PRINTF("time failed\n");	
		// }
		// Todo: handle err
		return tp;
	}
	static bool once = false;
	static uint64 frequency;
	double StopMeasure(TimePoint startPoint){
		if(!once){
			BOOL success = QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
			// if(!success){
			// 	PL_PRINTF("time failed\n");	
			// }
			// Todo: handle err
		}
		TimePoint endPoint;
		BOOL success = QueryPerformanceCounter((LARGE_INTEGER*)&endPoint);
		// if(!success){
		// 	PL_PRINTF("time failed\n");	
		// }
		return (double)(endPoint-startPoint)/(double)frequency;
	}
	void Sleep(double seconds){
        Win32Sleep((uint32)(seconds*1000));   
    }
    APIFile* FileOpen(const std::string& path, uint64* outFileSize, uint32 flags){
        DWORD access = GENERIC_READ|GENERIC_WRITE;
        DWORD sharing = 0;
        if(flags&FILE_ONLY_READ){
            access = GENERIC_READ;
            sharing = FILE_SHARE_READ;
		}
		DWORD creation = OPEN_EXISTING;
		if(flags&FILE_CAN_CREATE)
			creation = OPEN_ALWAYS;
		if(flags&FILE_WILL_CREATE)
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
        
		HANDLE handle = CreateFileA(path.c_str(),access,sharing,NULL,creation,FILE_ATTRIBUTE_NORMAL, NULL);
		
		if(handle==INVALID_HANDLE_VALUE){
			DWORD err = GetLastError();
			if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND){
				PL_PRINTF("[WinError %lu] Cannot find '%s'\n",err,path.c_str());
			}else if(err == ERROR_ACCESS_DENIED){
				PL_PRINTF("[WinError %lu] Denied access to '%s'\n",err,path.c_str()); // tried to open a directory?
			}else {
				PL_PRINTF("[WinError %lu] Error opening '%s'\n",err,path.c_str());
			}
			return 0;
		}else if (outFileSize){
            
			DWORD success = GetFileSizeEx(handle, (LARGE_INTEGER*)outFileSize);
			if (!success) {
				// GetFileSizeEx will probably not fail if CreateFile succeeded. But just in case it does.
				DWORD err = GetLastError();
				printf("[WinError %lu] Error aquiring file size from '%s'",err,path.c_str());
				*outFileSize = 0;
				Assert(outFileSize)
			}
		}
		return TO_INTERNAL(handle);
	}
	uint64 FileRead(APIFile* file, void* buffer, uint64 readBytes){
		Assert(readBytes!=(uint64)-1); // -1 indicates no bytes read
		
		DWORD bytesRead=0;
		DWORD success = ReadFile(TO_HANDLE(file),buffer,readBytes,&bytesRead,NULL);
		if(!success){
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] FileRead '%llu'\n",err,(uint64)file);
			return -1;
		}
		return bytesRead;
	}
	uint64 FileWrite(APIFile* file, const void* buffer, uint64 writeBytes){
		Assert(writeBytes!=(uint64)-1); // -1 indicates no bytes read
		
		DWORD bytesWritten=0;
		DWORD success = WriteFile(TO_HANDLE(file),buffer,writeBytes,&bytesWritten,NULL);
		if(!success){
			DWORD err = GetLastError();
			PL_PRINTF("[WinError %lu] FileWrite '%llu'\n",err,(uint64)file);
			return -1;
		}
		return bytesWritten;
	}
	bool FileSetHead(APIFile* file, uint64 position){
		DWORD success = 0;
		if(position==(uint64)-1){
			success = SetFilePointerEx(file,{0},NULL,FILE_END);
		}else{
			success = SetFilePointerEx(file,*(LARGE_INTEGER*)&position,NULL,FILE_BEGIN);
		}
		if(success) return true;
		
		DWORD err = GetLastError();
		PL_PRINTF("[WinError %lu] FileSetHead '%llu'\n",err,(uint64)file);
		return false;
	}
	void FileClose(APIFile* file){
		if(file)
			CloseHandle(TO_HANDLE(file));
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
				PL_PRINTF("[WinError %lu] Cannot find '%s'\n",err,path.c_str());
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
		uint64 t0 = (uint64)creation.dwLowDateTime+(uint64)creation.dwHighDateTime*((uint64)MAXDWORD+1);
		uint64 t1 = (uint64)access.dwLowDateTime+(uint64)access.dwHighDateTime*((uint64)MAXDWORD+1);
		uint64 t2 = (uint64)modified.dwLowDateTime+(uint64)modified.dwHighDateTime*((uint64)MAXDWORD+1);
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
    
    // static std::mutex s_allocStatsMutex;
	static uint64 s_totalAllocatedBytes=0;
	static uint64 s_totalNumberAllocations=0;
	static uint64 s_allocatedBytes=0;
	static uint64 s_numberAllocations=0;
	void* Allocate(uint64 bytes){
		if(bytes==0) return nullptr;
		// void* ptr = HeapAlloc(GetProcessHeap(),0,bytes);
        void* ptr = malloc(bytes);
		if(!ptr) return nullptr;
		
		// s_allocStatsMutex.lock();
		s_allocatedBytes+=bytes;
		s_numberAllocations++;
		s_totalAllocatedBytes+=bytes;
		s_totalNumberAllocations++;			
		// s_allocStatsMutex.unlock();
		
		return ptr;
	}
    void* Reallocate(void* ptr, uint64 oldBytes, uint64 newBytes){
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
                
                // s_allocStatsMutex.lock();
                s_allocatedBytes+=newBytes-oldBytes;
                
                s_totalAllocatedBytes+=newBytes;
                s_totalNumberAllocations++;			
                // s_allocStatsMutex.unlock();
                return newPtr;
            }
        }
    }
	void Free(void* ptr, uint64 bytes){
		if(!ptr) return;
		free(ptr);
		// HeapFree(GetProcessHeap(),0,ptr);
		
		// s_allocStatsMutex.lock();
		s_allocatedBytes-=bytes;
		s_numberAllocations--;
		// s_allocStatsMutex.unlock();
	}
	uint64 GetTotalAllocatedBytes(){
		return s_totalAllocatedBytes;
	}
	uint64 GetTotalNumberAllocations(){
		return s_totalNumberAllocations;
	}
	uint64 GetAllocatedBytes(){
		return s_allocatedBytes;
	}
	uint64 GetNumberAllocations(){
		return s_numberAllocations;
	}
	static HANDLE m_consoleHandle = NULL;
    void SetConsoleColor(uint16 color){
		if (m_consoleHandle == NULL) {
			m_consoleHandle = GetStdHandle(-11);
			if (m_consoleHandle == NULL)
				return;
		}
		// Todo: don't set color if already set? difficult if you have a variable of last color and a different 
		//		function sets color without changing the variable.
		SetConsoleTextAttribute((HANDLE)m_consoleHandle, color);
	}
    Semaphore::Semaphore(int initial, int max) {
		m_initial = initial;
		m_max = max;
	}
	Semaphore::~Semaphore() {
		cleanup();
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
		if (m_internalHandle == 0) {
			HANDLE handle = CreateSemaphore(NULL, m_initial, m_max, NULL);
			if (handle == INVALID_HANDLE_VALUE) {
				DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] CreateSemaphore\n",err);
			}else
                m_internalHandle = TO_INTERNAL(handle);
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
		if (m_internalHandle != 0) {
			BOOL yes = ReleaseSemaphore(TO_HANDLE(m_internalHandle), count, NULL);
			if (!yes) {
				DWORD err = GetLastError();
				PL_PRINTF("[WinError %lu] ReleaseSemaphore\n",err);
			}
		}
	}
	Mutex::~Mutex() {
		cleanup();
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
			if (m_ownerThread != 0) {
				PL_PRINTF("Mutex : Locking twice, old owner: %u, new owner: %u\n",m_ownerThread,newId);
			}
			m_ownerThread = newId;
			if (res == WAIT_FAILED) {
                // Todo: What happened do the old thread who locked the mutex. Was it okay to ownerThread = newId
				DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] WaitForSingleObject\n",err);
			}
		}
	}
	void Mutex::unlock() {
		if (m_internalHandle != 0) {
			m_ownerThread = 0;
			BOOL yes = ReleaseMutex(TO_HANDLE(m_internalHandle));
			if (!yes) {
				DWORD err = GetLastError();
                PL_PRINTF("[WinError %lu] ReleaseMutex\n",err);
			}
		}
	}
	uint32_t Mutex::getOwner() {
		return m_ownerThread;
	}
    Thread::~Thread() {
		cleanup();
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
	void Thread::init(uint32(*func)(void*), void* arg) {
		if (!m_internalHandle) {
			// const uint32 stackSize = 1024*1024;
			HANDLE handle = CreateThread(NULL, 0, (DWORD(*)(void*))func, arg, 0,(DWORD*)&m_threadId);
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
		m_internalHandle = 0;
	}
	bool Thread::isActive() {
		return m_internalHandle!=0;
	}
	bool Thread::joinable() {
		return m_threadId != GetCurrentThreadId();
	}
	ThreadId Thread::getId() {
		return m_threadId;
	}
	ThreadId Thread::GetThisThreadId() {
		return GetCurrentThreadId();
	}
	std::string GetWorkingDirectory(){
		DWORD length = GetCurrentDirectoryA(0,0);
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
			return out;
		}
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
			int index = argc * sizeof(char*);
			int totalSize = index + dataSize;
			//printf("size: %d index: %d\n", totalSize,index);
			argv = (char**)Allocate(totalSize);
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
		int totalSize = argc * sizeof(char*);
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
	std::unordered_map<APIFile*,PipeInfo> pipes;
	uint64 pipeIndex=0x500000;
	APIFile* PipeCreate(bool inheritRead,bool inheritWrite){
		PipeInfo info{};
		
		SECURITY_ATTRIBUTES saAttr; 
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		saAttr.bInheritHandle = inheritRead||inheritWrite;
		saAttr.lpSecurityDescriptor = NULL; 
		
		DWORD err = CreatePipe(&info.readH,&info.writeH,&saAttr,0);
		if(!err){
			err = GetLastError();
			PL_PRINTF("[WinError %lu] Cannot create pipe\n",err);
			return 0;
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
		
		pipes[(APIFile*)pipeIndex] = info;
		return (APIFile*)(pipeIndex++);
	}
	void PipeDestroy(APIFile* pipe){
		auto& info = pipes[pipe];
		if(info.readH)
			CloseHandle(info.readH);
		if(info.writeH)
			CloseHandle(info.writeH);
		pipes.erase(pipe);
	}
	int PipeRead(APIFile* pipe,void* buffer, int size){
		auto& info = pipes[pipe];
		DWORD read=0;
		DWORD err = ReadFile(info.readH,buffer,size,&read,0);
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
	int PipeWrite(APIFile* pipe,void* buffer, int size){
		auto& info = pipes[pipe];
		DWORD written=0;
		DWORD err = WriteFile(info.writeH,buffer,size,&written,0);
		if(!err){
			err = GetLastError();
			PL_PRINTF("[WinError %lu] Pipe write failed\n",err);
			return 0;
		}
		return written;
	}
	bool StartProgram(const std::string& path, char* commandLine, int flags, int* exitCode, APIFile* fStdin, APIFile* fStdout) {
		// if (!FileExist(path)) {
		// 	return false;
		// }
		
		if(path.empty() && !commandLine){
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
			auto& info = pipes[fStdin];
			si.hStdInput = info.readH;
		}
		if(fStdout){
			auto& info = pipes[fStdout];
			si.hStdOutput = info.writeH;
			si.hStdError = info.writeH;
		}
		bool inheritHandles=false;
		if(fStdin||fStdout){
			si.dwFlags |= STARTF_USESTDHANDLES;
			inheritHandles = true;
		}
		 
		int slashIndex = path.find_last_of("\\");
    
		std::string workingDir{};
		const char* dir = NULL;
		if(slashIndex!=-1)
			workingDir = path.substr(0, slashIndex);
		// else
		// 	workingDir = GetWorkingDirectory();
        
		if(!workingDir.empty())
			dir = workingDir.c_str();
			
		DWORD createFlags = 0;
		if(flags&PROGRAM_NEW_CONSOLE)
			createFlags |= CREATE_NEW_CONSOLE;
			
		char* exepath = 0;
		if(!path.empty())
			exepath = (char*)path.data();
		BOOL yes = CreateProcessA(exepath,   // the path
                       commandLine,        // Command line
                       NULL,           // Process handle not inheritable
                       NULL,           // Thread handle not inheritable
                       inheritHandles,          // Set handle inheritance to FALSE
                       createFlags,              // No creation flags
                       NULL,           // Use parent's environment block
                       dir,   // starting directory 
                       &si,            // Pointer to STARTUPINFO structure
                       &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
                       );
        if(!yes){
			DWORD err = GetLastError();
			if(err == ERROR_BAD_EXE_FORMAT){
				if(path.empty())
					printf("[WinError %lu] StartProgram, bad_exe_format %s\n",err,commandLine);
				else
					printf("[WinError %lu] StartProgram, bad_exe_format %s\n",err,path.c_str());
			}else{
				if(path.empty())
					printf("[WinError %lu] StartProgram, could not start %s\n",err,commandLine);
				else
					printf("[WinError %lu] StartProgram, could not start %s\n",err,path.c_str());
				// Todo: does the handles need to be closed? probably not since CreateProcess failed. double check.	
			}
			return false;
		}
		if(flags&PROGRAM_WAIT){
			WaitForSingleObject(pi.hProcess,INFINITE);
			if(exitCode){
				BOOL success = GetExitCodeProcess(pi.hProcess, (DWORD*)exitCode);
				if(!success){
					DWORD err = GetLastError();
					printf("[WinError %lu] StartProgram, failed aquring exit code (path: %s)\n",err,path.c_str());	
				}else{
					if(success==STILL_ACTIVE){
						printf("[WinWarning] StartProgram, cannot get exit code, process is active (path: %s)\n",path.c_str());	
					}else{
						
					}
				}
			}
		}else{
			// Todo: what happens with return value when we don't wait for the process?	
		}
		
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		
		if(fStdin){
			auto& info = pipes[fStdin];
			CloseHandle(info.readH);
			info.readH = 0;
		}
		if(fStdout){
			auto& info = pipes[fStdout];
			CloseHandle(info.writeH);
			info.writeH=0;
		}
		return true;
	}
	FileMonitor::~FileMonitor() { cleanup(); }
	void FileMonitor::cleanup() {
		m_mutex.lock();
		m_running = false;
		if (m_thread.joinable()) {
			HANDLE handle = m_thread.m_internalHandle;
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
					while (true) {
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
	// Todo: handle is checked against NULL, it should be checked against INVALID_HANDLE_VALUE
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
			if (m_thread.isActive()) // Todo: What if
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