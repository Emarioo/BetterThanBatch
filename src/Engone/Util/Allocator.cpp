
// #define WIN32

#include "Engone/Util/Allocator.h"
#ifdef OS_WINDOWS
#include "Engone/Win32Includes.h"
#else
#include "sys/mman.h"
#endif

#include "Engone/Logger.h"

// #include "Engone/Util/RandomUtility.h"
#include "Engone/PlatformLayer.h"
#include "Engone/Asserts.h"

//#define FORCE_HEAP

//#define DEBUG_GAME_MEMORY(x) x
#define DEBUG_GAME_MEMORY(x)

namespace engone {
    
    
    void Tracker::add(void* ptr, int bytes, const DebugLocation& debug) {
        auto id = debug.type.hash_code();
        int alloc_id = engone::atomic_add(&allocation_id, 1) - 1;

        // @DEBUG
        int alloc_ids[]{
            0,
            // 60657,
            // 47941,
            // 13265,
            // 42476,
            // 42
            // 1, 103, 104, 106, 107
        };
        for(int i=1;i<sizeof(alloc_ids)/sizeof(*alloc_ids);i++) {
            BREAK(alloc_id == alloc_ids[i]);
        }

        mutex.lock();

        bool prev = m_enableTracking;
        m_enableTracking = false;
        
        int count = debug.type_size == 0 ? bytes : bytes / debug.type_size;
        TrackerType* tracked_type = nullptr;
        {
            auto pair = m_trackedTypes.find(id);
            if(pair == m_trackedTypes.end()) {
                tracked_type = &(m_trackedTypes[id] = {});
                auto name = debug.type.name();
                tracked_type->name = name;
                tracked_type->size = debug.type_size;
            } else {
                tracked_type = &pair->second;
            }
            tracked_type->count += count;
        }
        #ifdef LOG_TRACKER
        engone::log::out  << "Track "<<count<<": "<<engone::log::GREEN<<debug.type.name()<<engone::log::NO_COLOR <<" (left: "<<tracked_type->count<<") "<<ptr<<"\n";
        engone::log::out.flush();
        #endif
        {
            auto pair = tracked_type->locations.find(ptr);
            if(pair != tracked_type->locations.end()) {
                Assert(false); // pointer already exists? why?
            } else {
                auto loc = &(tracked_type->locations[ptr] = {});
                loc->alloc_id = alloc_id;
                loc->file = debug.file;
                loc->line = debug.line;
            }
        }
        m_enableTracking = prev;
        mutex.unlock();
    }
    void Tracker::del(void* ptr, int bytes, const DebugLocation& debug) {
        auto id = debug.type.hash_code();

        mutex.lock();

        bool prev = m_enableTracking;
        m_enableTracking = false;
        
        int count = debug.type_size == 0 ? bytes : bytes / debug.type_size;
        TrackerType* tracked_type = nullptr;
        {
            auto pair = m_trackedTypes.find(id);
            if(pair == m_trackedTypes.end()) {
                tracked_type = &(m_trackedTypes[id] = {});
                auto name = debug.type.name();
                tracked_type->name = name;
                tracked_type->size = debug.type_size;
            } else {
                tracked_type = &pair->second;
            }
            if(debug.type_size == 0)
                tracked_type->count -= bytes;
            else
                tracked_type->count -= bytes / debug.type_size;
        }
        #ifdef LOG_TRACKER
        engone::log::out  << "Untrack "<<count<<": "<<engone::log::GREEN<<debug.type.name()<<engone::log::NO_COLOR <<" (left: "<<tracked_type->count<<") "<<ptr<<"\n";
        engone::log::out.flush();
        #endif
        {
            auto pair = tracked_type->locations.find(ptr);
            if(pair != tracked_type->locations.end()) {
                tracked_type->locations.erase(ptr);
            } else {
                Assert(false); // double free?
            }
        }
        m_enableTracking = prev;
        mutex.unlock();
    }
    
    void Tracker::printAllocations(){
        using namespace engone;

        mutex.lock();
        bool prev = m_enableTracking;
        m_enableTracking = false;

        u32 totalMemory = 0;
        log::out << log::BLUE<<"Tracked types:\n";
        // DynamicArray<DebugLocation> m_uniqueTrackLocations{};
        
        std::string cwd = engone::GetWorkingDirectory() + "/";
        
        for(auto& pair : m_trackedTypes) {
            TrackerType* trackType = &pair.second;
            if(trackType->count == 0)
                continue;
            
            if(trackType->name.find("struct ") == 0) // Naming on Windows?
                log::out <<" "<<log::LIME<< (trackType->name.c_str()+strlen("struct "))<<log::NO_COLOR <<"["<<trackType->size<<"]: "<<trackType->count<<"\n";
            else
                log::out <<" "<<log::LIME<< trackType->name <<log::NO_COLOR<<"["<<trackType->size<<"]: "<<trackType->count<<"\n";
            int limit = 10;
            int cur_locs = 0;
            for(auto pair : trackType->locations){
                if (cur_locs > limit)
                    break;
                cur_locs++;

                auto& loc = pair.second;
                int len = strlen(loc.file);
                int cutoff = len-1;
                int slashCount = 2;
                std::string path = loc.file;
                
                for(int i=0;i<path.size();i++) {
                    if(path[i] == '\\')
                        path[i] = '/';
                }
                
                int where = path.find(cwd);
                if(where == 0) {
                    path = path.substr(cwd.length());
                }
                log::out << "  "<<(path) << ":"<<loc.line<< ", id: "<<loc.alloc_id<<"\n";
            }
            totalMemory += trackType->count * trackType->size;
        }
        if(totalMemory != 0)
            log::out << log::RED;
        log::out << "Tracked memory: "<<totalMemory<<"\n";
        
        m_enableTracking = prev;
        mutex.unlock();
    }
    int Tracker::getMemoryUsage(){
        u32 memory = 0;
        // we use standard library and don't track memory usage.
        // mutex.lock();
        // for(auto& pair : m_trackedTypes) {
        //     TrackerType* trackType = &pair.second;
        //     memory += trackType->locations.max * sizeof(DebugLocation);
        // }
        // mutex.unlock();
        return memory;
    }
    
    static HeapAllocator global_heapAllocator{};
    HeapAllocator* GlobalHeapAllocator() {
        return &global_heapAllocator;
    }
    
    void StateAllocator::cleanup(){
        // delete common;
        // delete allocator;
    }
    bool StateAllocator::init(void* baseAddress, u64 bytes, bool noPhysics){
        if (baseAllocation)
            return true; // returns true since the memory is initialized
        mutex.lock();
        if(!GetStateAllocator()){
            SetStateAllocator(this);
        }
        // baseAllocation = (void*)GigaBytes(1);
        // baseAllocation = (void*)MegaBytes(900);
        // maxSize = 500;
        maxSize = bytes;
        // GigaBytes(1);
        #ifdef OS_WINDOWS
        baseAllocation = VirtualAlloc(baseAddress,maxSize,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if(!baseAllocation){
            int err = GetLastError();
            printf("StateAllocator : Virtual alloc failed %d!\n",err);
            mutex.unlock();
            return false;
        }
        #else
        baseAllocation = mmap(baseAddress, bytes, PROT_READ | PROT_WRITE, MAP_FIXED, 0, 0);
        if(!baseAllocation){
            printf("StateAllocator : Virtual alloc failed!\n");
            mutex.unlock();
            return false;
        }
        #endif
        freeBlocks.push_back({0,maxSize});
        
        mutex.unlock();
        //log::out << "size: " << sizeof(rp3d::PhysicsCommon) << "\n";
        //common = new rp3d::PhysicsCommon(allocator);
        return true;
    }
    void* StateAllocator::getBaseAdress() {
        return baseAllocation;
    }
    bool StateAllocator::save(const std::string& path){
        auto file = FileOpen(path, FILE_CLEAR_AND_WRITE);
        if (!file) {
            log::out << log::RED << "Engone : Failed saving game memory\n";
            return false;
        }
        // Use the start of the last free block as bytes to write instead of max memory.
        // It will be faster since it requires less bytes to write.
        
        //Block* emptyBlock = 0;
        u64 memory = getMaxMemory();
        if (getUsedBlocks().size() != 0) {
            Block& block = getUsedBlocks().back();
            memory = block.start + block.size;
        }
        
        log::out << "Saving game memory ("<<memory<<" bytes)... ";
        log::out.flush();
        FileWrite(file, getBaseAdress(), memory);
        log::out << "done\n";
        FileClose(file);
        return true;
    }
    bool StateAllocator::load(const std::string& path){
        u64 fileSize;
        #undef FILE_READ_ONLY // bye Windows flag
        auto file = FileOpen(path, FILE_READ_ONLY, &fileSize);
        if (!file) {
            log::out << log::RED << "Engone : Failed loading game memory\n";
            return false;
        }
        // Check errors?
        log::out << "Loading game memory ("<<fileSize<<" bytes)... ";
        log::out.flush();
        FileRead(file, getBaseAdress(), fileSize);
        log::out << "done\n";
        FileClose(file);
        return true;
    }
    void StateAllocator::insertFreeBlock(Block block){
        //-- Find spot for block
        int index = freeBlocks.size();
        for(int i=0;i<freeBlocks.size();i++){
            Block& b = freeBlocks[i];
            if(b.start>block.start){
                index = i;
                break;
            }
        }
        
        //-- Get adjacent blocks
        Block* prev=nullptr;
        Block* next=nullptr;
        if(index-1>=0){
            prev = &freeBlocks[index-1];
        }
            if(index<freeBlocks.size()){
            next = &freeBlocks[index];
        }
        
        //-- Combine blocks
        bool replacePrev = false;
        bool replaceNext = false;
        if(prev){
            if(prev->start+prev->size==block.start){
                block.start = prev->start;
                block.size = block.size + prev->size;
                replacePrev = true;
            }
        }
        if(next){
            if(block.start+block.size==next->start){
                block.size = block.size + next->size;
                replaceNext = true;
            }
        }
        
        //-- Replace/remove or create new blocks
        if(replacePrev){
            *prev = block;
            if(replaceNext)
                freeBlocks.erase(freeBlocks.begin()+index);
        }else if(replaceNext) {
            *next = block;
        }else{
            freeBlocks.insert(freeBlocks.begin()+index,block);
        }
    }
    void StateAllocator::insertUsedBlock(Block block){
        for(int i=0;i<usedBlocks.size();i++){
            Block& b = usedBlocks[i];
            if(b.start>block.start){
                usedBlocks.insert(usedBlocks.begin()+i,block);  
                return; 
            }
        }
        usedBlocks.push_back(block); 
    }
    void* StateAllocator::allocate(u64 bytes){
        if(bytes==0) return nullptr;
        mutex.lock();
        int index = -1;
        //Todo: start searching from random position to allocate more uniformly.
        //      Will also use less loops.
        for(int i=0;i<freeBlocks.size();i++){
            Block& block = freeBlocks[i];
            if(block.size>=bytes){
                index = i;
                break;
            }
        }
        
        if (index == -1) {
            mutex.unlock();
            DEBUG_GAME_MEMORY(log::out << "Failed allocate\n";)
            return nullptr;
        }
        Block& freeBlock = freeBlocks[index];
        Block newBlock = {freeBlocks[index].start,bytes};
        if(freeBlock.size == bytes){
              freeBlocks.erase(freeBlocks.begin()+index); 
        }else{
            freeBlock.start += bytes;
            freeBlock.size -= bytes;
        }
        usedMemory+=newBlock.size;
        insertUsedBlock(newBlock);
        mutex.unlock();
        DEBUG_GAME_MEMORY(log::out<<"Alloc {"<<newBlock.start<<","<<newBlock.size<<"}\n";)
        return (void*)((u64)baseAllocation + (u64)newBlock.start);
    }
    void* StateAllocator::reallocate(u64 bytes, void* oldPtr, u64 oldBytes){
        // Todo: bytes==0, bytes<used.size, bytes==used.size
        
        // Todo: ptr is 0?
        
        if(bytes==0){
            StateAllocator::free(oldPtr);
            return nullptr;
        }
        if(oldPtr == nullptr) {
            return StateAllocator::allocate(bytes);
        }
        mutex.lock();
        u64 start = (u64)oldPtr-(u64)baseAllocation;
        
        //-- Find the pointer/block
        int index=-1;
        for(int i=0;i<usedBlocks.size();i++){
            Block& block = usedBlocks[i];
            if(block.start==start){
                index = i;
                break;
            }
        }
        if (index == -1) {
            mutex.unlock();
            Assert(("GameMemory: pointer doesn't exist", false));
            return nullptr; // ptr doesn't exist
        }
        Block& used = usedBlocks[index];
        //log::out << "Realloc old->{" << used.start << "," << used.size << "}\n";
        if (used.size == bytes) {
            mutex.unlock();
            DEBUG_GAME_MEMORY(log::out << "Cannot reallocate same size\n";)
            return oldPtr;
        }
        
        //-- Find adjacent free block
        index=-1;
        for(int i=0;i<freeBlocks.size();i++){
            Block& block = freeBlocks[i];
            if(used.start+used.size == block.start){
                if(used.size+block.size>=bytes)
                    index = i;
                break;
            }
            if(block.start>used.start)
                break;
        }
        if(index==-1){
            if(bytes<used.size){
                usedMemory+=bytes-used.size;
                Block freeBlock = {used.start+bytes,used.size-bytes};
                insertFreeBlock(freeBlock);
                used.size = bytes;
                mutex.unlock();
                return oldPtr;
            }else{
                // new allocation
                u64 size = used.size; // copy value because used becomes invalid
                void* newPtr = StateAllocator::allocate(bytes);
                // Todo: check failure
                if(newPtr){
                    memcpy(newPtr,oldPtr,size);
                    // Todo: free will find the used block but we already know it so if we could
                    //      Give it to the function or write the function again but based on what we already know
                    //      We could save time performance.
                    mutex.unlock();
                    StateAllocator::free(oldPtr);
                    return newPtr;
                }
                mutex.unlock();
                return nullptr;
            }
        }else{
            usedMemory+=bytes-used.size;
            Block& freeBlock = freeBlocks[index];
            if(bytes<used.size){
                freeBlock.start = used.start + bytes;
                freeBlock.size += used.size - bytes;
                used.size = bytes;
            } else if(freeBlock.size+used.size==bytes){
                freeBlocks.erase(freeBlocks.begin()+index);
                used.size=bytes;
            }else{
                freeBlock.start = used.start+bytes;
                freeBlock.size -= bytes-used.size;
                used.size = bytes;
            }
            mutex.unlock();
            return oldPtr;
        }
    }
    void StateAllocator::free(void* ptr){
        mutex.lock();
        u64 start = (u64)ptr-(u64)baseAllocation;
        int index=-1;
        for(int i=0;i<usedBlocks.size();i++){
            Block& block = usedBlocks[i];
            if(block.start==start){
                index = i;
                break;
            }
            if(block.start>start)
                break;
        }
        if (index == -1) {
            mutex.unlock();
            DEBUG_GAME_MEMORY(log::out << "Cannot free empty pointer\n";)
            return;
        }
        
        Block block = usedBlocks[index];
        usedMemory-=block.size;
        usedBlocks.erase(usedBlocks.begin()+index);
        insertFreeBlock(block);
        mutex.unlock();
        DEBUG_GAME_MEMORY(log::out << "Free {" << block.start << "," << block.size << "}\n";)
    }
    u64 StateAllocator::getMaxMemory(){
        return maxSize;
    }
    u64 StateAllocator::getUsedMemory(){
        return usedMemory;
    }
    std::vector<StateAllocator::Block>& StateAllocator::getFreeBlocks(){
        return freeBlocks;
    }
    std::vector<StateAllocator::Block>& StateAllocator::getUsedBlocks(){
        return usedBlocks;
    }
    void StateAllocator::getPointers(std::vector<void*>& ptrs){
        ptrs.clear();
        for(Block& block : usedBlocks){
            ptrs.push_back((void*)(block.start + (u64)baseAllocation));
        }
    }
    void StateAllocator::print(){
        int address=0;
        int freeIndex = 0;
        int usedIndex = 0;
        
        while(true){
            Block* f = nullptr;
            Block* u = nullptr;
            if(freeIndex<freeBlocks.size())
                f = &freeBlocks[freeIndex];
            if(usedIndex<usedBlocks.size())
                u = &usedBlocks[usedIndex];
            
            bool done=false;
            int type = 0;
            if(u){
                if(u->start==address){
                    address+=u->size;   
                    usedIndex++;
                    done=true;
                    type=0;
                }
            }
            if(f&&!done){
                if(f->start==address){
                    address+=f->size;
                    freeIndex++;
                    done=true;
                    type=1;
                }
            }
            if(!done){
                if(u||f)
                    printf("ERROR with print: BLOCKS ARE LEFT!\n");
                break;
            }
            
            // Todo: KB/MB formatting?
            Block* block = type?f:u;
            log::out << " [ "<<block->start;
            if(type) log::out<<" - "<< block->size<<" - ";
            else log::out << " # "<<block->size<<" # ";
            log::out<<(block->start+block->size-1)<<"]\n";
            // printf("|%llu - %llu ",f->start,f->start+f->size-1);
        }
        // log::out << "\n";
        // for(Block& b : freeBlocks){
        //      printf("[%llu _ %llu]",b.start,b.start+b.size);
        // }
        // printf("\n");
        // for(Block& b : usedBlocks){
        //      printf("[%llu - %llu]",b.start,b.start+b.size);
        // }
        // printf("\n");
    }
    static StateAllocator* s_gameMemory=0;
    StateAllocator* GetStateAllocator() {
        // if(!s_gameMemory){
        //     s_gameMemory = ALLOC_NEW(GameMemory);
        //     s_gameMemory->init((void*)GigaBytes(1000), MegaBytes(10),false);
        // }
        return s_gameMemory;
    }
    void SetStateAllocator(StateAllocator* memory){
        if(!s_gameMemory || !memory) {
            s_gameMemory = memory;
        } else if (s_gameMemory != memory) {
            printf("SetStateAllocator : Cannot overwrite global variable\n");   
        }
    }
    #ifdef gone
    void TestActions(StateAllocator& mem, int count){
        for(int i=0;i<count;i++){
            mem.allocate(10+GetRandom()*10);
        }
    }
    void WriteResult(StateAllocator& memory, const char* path){
        auto file = FileOpen(path,FILE_CLEAR_AND_WRITE);
        
        char buffer[10000];
        int bytes=0;
        auto fb = memory.getFreeBlocks();
        auto ub = memory.getUsedBlocks();
        char varName[100];
        strcpy(varName,path);
        int nameBegin = 0;
        int length = strlen(path);
        for(int i=length-1;i>=0;i--){
            char chr = path[i];
            if(chr=='/'){
                nameBegin = i+1;
                // printf("%s\n",varName+nameBegin);
                break;
            }
            if(chr=='.'){
                varName[i] = 0;
            }
        }
        // printf("%s\n",varName+nameBegin);
        
        bytes+=sprintf(buffer+bytes,"var %s=[[",varName+nameBegin);
        for(int i=0;i<fb.size();i++){
            auto& b = fb[i];
            bytes += sprintf(buffer+bytes,"%llu,%llu,",b.start,b.size);
        }
        bytes+=sprintf(buffer+bytes,"],[");
        for(int i=0;i<ub.size();i++){
            auto& b = ub[i];
            bytes += sprintf(buffer+bytes,"%llu,%llu,",b.start,b.size);
        }
        bytes+=sprintf(buffer+bytes,"]]");
        
        // const char* test="var state0=[[0,7],[8,2]]";
        // FileWrite(file,test,strlen(test));
        
        FileWrite(file,buffer,bytes);
        FileClose(file);
    }
    void TestRandomActions(){
        StateAllocator memory{};
        memory.init((void*)GigaBytes(1000), 1000, true);
        
        std::vector<void*> ptrs;
        int attempts=0;
        
        char buffer[256];
        int states = 0;
        int fails = 0;
        
        float chances[]{0.4,0.2,0.4};
        
        while(attempts<100){
            attempts++;
            
            float chance = GetRandom();
            float total = 0;
            int type = 0;
            while(type<3){
                total+=chances[type];
                if(chance<total)
                    break;
                
                type++;
            }
            
            u64 size = GetRandom()*80;
            int ind = GetRandom()*ptrs.size();
            
            if(type!=0&&ptrs.size()==0){
                fails++;
                continue;
            }
            
            log::out << "Attempt "<<attempts<<" "<<states<<"\n";
            
            u64 memBefore = memory.getUsedMemory();

            if(type==0){
                log::out << "alloc "<<size<<"\n";
                void* ptr = memory.allocate(size);
                if(ptr)
                    ptrs.push_back(ptr);
                else
                    fails++;
            }else if (type==1){
                if(ptrs.size()==0)
                    continue;
                log::out << "free\n";
                void* ptr = ptrs[ind];
                memory.free(ptr);
                ptrs.erase(ptrs.begin()+ind);
            }else {
                if(ptrs.size()==0)
                    continue;
                log::out << "realloc "<<size<<"\n";
                void* ptr = memory.reallocate(size, ptrs[ind], 0);
                if(ptr){
                    ptrs[ind]=ptr;
                }else if(size==0){
                    ptrs.erase(ptrs.begin()+ind);
                } else {
                    printf("hum\n");
                }
            }
            if (memBefore == memory.getUsedMemory()) {
                log::out << "No change\n";
            }
            if(attempts%1==0){
                sprintf(buffer,"../Tests/GameMemory/states/state%d.js",states);
                WriteResult(memory,buffer);
                states++;
            }
            //memory.print();
        }
        sprintf(buffer,"../Tests/GameMemory/states/state%d.js",states);
        states++;
        WriteResult(memory,buffer);
        log::out << "Fails "<<fails<<", States "<<states<<"\n";
    }
    void SpecialTest(){
        StateAllocator& memory = *GetStateAllocator();
        
        TestActions(memory,13);
        
        log::out << "Stage 1\n";
        // memory.print();
        WriteResult(memory,"Tests/GameMemory/states/state0.js");
        
        std::vector<void*> ptrs;
        memory.getPointers(ptrs);
        
        for(int i=0;i<5;i++){
            int index = GetRandom()*ptrs.size();
            
            memory.free(ptrs[index]);
            ptrs.erase(ptrs.begin()+index);
        }
        log::out << "Stage 2\n";
        // memory.print();
        WriteResult(memory,"Tests/GameMemory/states/state1.js");
        
        TestActions(memory,13);
        
        log::out << "Stage 3\n";
        // memory.print();
        WriteResult(memory,"Tests/GameMemory/states/state2.js");
    }
    void GameMemoryTest(){
        // Todo: If you want to be fancy you could write some html and js to view the memory stages.
        //      You could write the memory state to a result.js file and then use it in some other js script.
        
        log::out << "Run GameMemoryTest\n";
        TestRandomActions();
    }
    #endif
}