#pragma once

#include "Engone/PlatformLayer.h"
#include "Engone/Asserts.h"
#include "Engone/Logger.h"

#include <vector>
#include <unordered_map>
#include <string>
#include <typeinfo>

#define HERE engone::DebugLocation{__FILE__,__LINE__}
#define HERE_T(TYPE) engone::DebugLocation{__FILE__,__LINE__, typeid(TYPE), sizeof(TYPE)}

#define CONSTRUCT(TYPE, LOC, ...) (AddTracking(typeid(TYPE), sizeof(TYPE), LOC), new(engone::Allocate(sizeof(TYPE))TYPE(__VA_ARGS__)))
#define CONSTRUCT_ARRAY(TYPE, COUNT, LOC) (AddTracking(typeid(TYPE), sizeof(TYPE), LOC, COUNT), engone::Allocate(sizeof(TYPE) * COUNT))
#define DECONSTRUCT(PTR, TYPE, LOC) (DelTracking(typeid(TYPE), sizeof(TYPE), LOC), PTR->~TYPE(), engone::Free(PTR, sizeof(TYPE)))
#define DECONSTRUCT_ARRAY(PTR, TYPE, COUNT, LOC) (DelTracking(typeid(TYPE), sizeof(TYPE), LOC, COUNT), engone::Free(PTR, sizeof(TYPE) * COUNT))

// #define TRACK_ADDS(TYPE, COUNT)
// #define TRACK_DELS(TYPE, COUNT)

// #define TRACK_ALLOC(TYPE) (TYPE*)engone::Allocate(sizeof(TYPE))
// #define TRACK_FREE(PTR, TYPE) engone::Free(static_cast<TYPE*>(PTR), sizeof(TYPE))

#define TRACK_ALLOC(TYPE) (TYPE*)engone::GlobalHeapAllocator()->allocate(sizeof(TYPE), nullptr, 0, HERE_T(TYPE))
#define TRACK_FREE(PTR, TYPE) engone::GlobalHeapAllocator()->allocate(0, PTR, sizeof(TYPE), HERE_T(TYPE))

#define TRACK_ARRAY_ALLOC(TYPE, COUNT) (TYPE*)engone::GlobalHeapAllocator()->allocate(sizeof(TYPE) * COUNT, nullptr, 0, HERE_T(TYPE))
#define TRACK_ARRAY_REALLOC(PTR, TYPE, COUNT, NEW_COUNT) (TYPE*)engone::GlobalHeapAllocator()->allocate(sizeof(TYPE) * NEW_COUNT, PTR, sizeof(TYPE) * COUNT, HERE_T(TYPE))
#define TRACK_ARRAY_FREE(PTR, TYPE, COUNT) engone::GlobalHeapAllocator()->allocate(0, PTR, sizeof(TYPE) * COUNT, HERE_T(TYPE))


#define SCOPED_ALLOCATOR_MOMENT(ALLOCATOR) int scoped_moment = (ALLOCATOR).save_moment(); defer { (ALLOCATOR).load_moment(scoped_moment); };

namespace engone{

    struct DebugLocation {
        DebugLocation(const char* file = "?", int line = 0, const std::type_info& t = typeid(void), int type_size = 0) : file(file), line(line), type(t), type_size(type_size) {}
        const char* file;
        int line;
        const std::type_info& type;
        int type_size;
    };
    struct Tracker {
        struct TrackLocation {
            const char* file;
            int line, alloc_id;
        };
        struct TrackerType {
            std::string name{};
            int size = 0;
            int count = 0;
            std::unordered_map<void*, TrackLocation> locations; // allocations of this type
        };
        std::unordered_map<size_t, TrackerType> m_trackedTypes{}; // maps typeinfo hashcodes to tracker types
        engone::Mutex mutex{};
        bool m_enableTracking = true;
        int allocated_bytes = 0;

        volatile int allocation_id = 1;
        
        // These are atomic operation (locked with mutex)
        // You can't del and then add because threads may allocate and free pointers in between which the tracker would see as incorrect.
        void add(void* ptr, int bytes, const DebugLocation& debug, bool skip_mutex = false);
        void del(void* ptr, int bytes, const DebugLocation& debug, bool skip_mutex = false);
        void del_add(void* old_ptr, int old_bytes, void* new_ptr, int new_bytes, const DebugLocation& debug);
        int getMemoryUsage();
        void printAllocations();
    };

    struct Allocator;
    // void* FnAllocate(u64 bytes, void* oldPtr, u64 newBytes)
    // allocates, reallocates, and frees
    typedef void* (*FnAllocate)(Allocator*, u64, void*, u64, const DebugLocation&);

    enum AllocatorType : u8 {
        ALLOCATOR_BASE = 0,
        ALLOCATOR_LINEAR,

        ALLOCATOR_NON_VOLATILE,
        ALLOCATOR_HEAP,
        ALLOCATOR_STATE,
    };
    // IMPORTANT TODO: Function pointers may not work with hotreloading (switching dlls)
    // Base allocator, no implementation
    struct Allocator {
        Allocator(AllocatorType type, FnAllocate func) : type(type), m_func(func) { }
        const AllocatorType type;

        // We hope the compiler inlines this function.
        // An extra call instruction would be needed otherwise.
        inline void* allocate(u64 bytes, void* ptr = nullptr, u64 old_bytes = 0, const DebugLocation& debug = {}) {
            return m_func(this, bytes, ptr, old_bytes, debug);
        }
        inline bool isNonVolatile() {
            return type > ALLOCATOR_NON_VOLATILE;
        }

        template <class T>
        inline T* create(const DebugLocation& debug = {}) {
            auto obj = (T*)m_func(this, sizeof(T), nullptr, 0, debug);
            new(obj)T();
            return obj;
        }
        template <class T>
        inline T* create_no_init(const DebugLocation& debug = {}) {
            auto obj = (T*)m_func(this, sizeof(T), nullptr, 0, debug);
            return obj;
        }
        template <class T>
        inline void destroy(T* obj, const DebugLocation& debug = {}) {
            obj->~T();
            m_func(this, 0, obj, sizeof(T), debug);
        }

        Tracker tracker;
    private:
        FnAllocate m_func;
    };
    struct HeapAllocator : public Allocator {
        HeapAllocator() : Allocator(ALLOCATOR_HEAP, (FnAllocate)HeapAllocator::static_allocate) { }
        // ~HeapAllocator() { }
        static inline void* static_allocate(HeapAllocator* allocator, u64 bytes, void* ptr, u64 old_bytes, const DebugLocation& debug = {}) {
            auto new_ptr = engone::Reallocate(ptr, old_bytes, bytes);
            // if(ptr && bytes != 0) {
            //     engone::log::out << engone::log::GRAY<<" "<<ptr<<"->"<<new_ptr<<"\n";
            // }
            if(ptr && bytes != 0) {
                allocator->tracker.del_add(ptr, old_bytes, new_ptr, bytes, debug);
            } else if(ptr) {
                allocator->tracker.del(ptr, old_bytes, debug);
            } else if(bytes != 0)
                allocator->tracker.add(new_ptr, bytes, debug);
            return new_ptr;
            // return allocator->allocate(bytes, ptr, old_bytes);
        }
        // inline void* allocate(u64 bytes, void* ptr, u64 old_bytes, const DebugLocation& debug = {}) {
        // }
    };
    struct LinearAllocator : public Allocator {
        LinearAllocator() : Allocator(ALLOCATOR_LINEAR, (FnAllocate)LinearAllocator::static_allocate) { }
        ~LinearAllocator() { cleanup(); }
        static inline void* static_allocate(LinearAllocator* allocator, u64 bytes, void* ptr, u64 old_bytes, const DebugLocation& debug = {}) {
            return allocator->allocate(bytes, ptr, old_bytes);
        }
        
        void init(int max) {
            Assert(!data);
            data = (u8*)engone::Allocate(max);
            Assert(data);
            used = 0;
            this->max = max;
        }
        void reset() {
            used = 0;   
        }
        void cleanup() {
            engone::Free(data, max);
            data = nullptr;
            max = 0;
            used = 0;
        }
        int save_moment() const {
            return used;
        }
        // some memory at the back of the linear allocator will be "freed".
        // WARNING: Destructors of the allocated objects will not be called,
        //   only use 'moments' for floats and integers or data without contstructor/destructor amd those that own allocations.
        void load_moment(int moment) {
            used = moment;
        }
        inline void* allocate(u64 bytes, void* ptr = nullptr, u64 old_bytes = 0) {
            if(bytes == 0) {
                return nullptr; // we can't free unless we free the last allocated chunk of bytes
            }
            if(ptr != nullptr) {
                // we last chunk is reallocated then
                // we can just expand the allocation
                // instead of creating a whole new one.
                // Also, we can't free memory so many reallocations would quickly use up the linear allocator.

                fix_alignment(8);

                if(used + bytes > max) {
                    Assert(("linear allocator too small",false));
                    return nullptr;
                }

                Assert(old_bytes); // we don't store how many bytes previous allocation had, we rely on user specifying it. It's less book here but it's more responsibility on the user.

                void* new_ptr = (u8*)data + used;
                used += bytes;
                memcpy(new_ptr, ptr, old_bytes);
                return new_ptr;
            }

            fix_alignment(8);


            // linear allocator can't reserve/resize for more memory because it would invalidate the returned pointers.
            if(used + bytes > max) {
                Assert(("linear allocator too small",false));
                return nullptr;
            }
            void* new_ptr = (u8*)data + used;
            used += bytes;
            return new_ptr;
        }
        bool initialized() {
            return max;
        }
        void fix_alignment(int alignment) {
            int misalign = (alignment - (((i64)data + (i64)used) % alignment)) % alignment;
            Assert(misalign >= 0);
            if(misalign) {
                if(used + misalign > max) {
                    Assert(("linear allocator too small",false));
                    return;
                }
                void* new_ptr = (u8*)data + used;
                used += misalign;
                memset(new_ptr, 0, misalign);
            }
        }

    private:
        void* data = nullptr;;
        u64 used = 0;
        u64 max = 0;
        bool ensure_alignment = false;
    };
    struct StateAllocator : public Allocator {
        StateAllocator() : Allocator(ALLOCATOR_STATE, (FnAllocate)StateAllocator::static_allocate) { }
        ~StateAllocator() { cleanup(); }
        static inline void* static_allocate(StateAllocator* allocator, u64 bytes, void* old_ptr, u64 old_bytes) {
            return allocator->reallocate(bytes, old_ptr, old_bytes);
        }

        void cleanup();
        
        bool init(void* baseAddress, u64 bytes, bool noPhysics);
        bool save(const std::string& path);
        bool load(const std::string& path);

        void* allocate(u64 bytes);
        void* reallocate(u64 bytes, void* ptr, u64 old_bytes);
        void free(void* ptr);
        
        u64 getUsedMemory();

        void getPointers(std::vector<void*>& ptrs);
        struct Block {
            u64 start;
            u64 size;
        };
        std::vector<Block>& getFreeBlocks();
        std::vector<Block>& getUsedBlocks();

        void* getBaseAdress();
        u64 getMaxMemory();

        void print();

    private:
        Mutex mutex;

        // GameMemoryID allocate(int bytes);
        // GameMemoryID reallocate(GameMemoryID, int bytes);
        // void free(GameMemoryID id);
        // void* aquire(GameMemoryID id);
        // void* release(GameMemoryID id);
        
        // static GameMemoryID Allocate(int bytes);
        // static GameMemoryID Reallocate(GameMemoryID id, int bytes);
        // void Free(GameMemoryID id);
        // void* Aquire(GameMemoryID id);
        // void* Release(GameMemoryID id);
        // struct GamePointer {
        //     GameMemoryID id;
        //     void* ptr;
        //     int size;
        //     bool inUse;
        // };
        // std::unordered_map<GameMemoryID,GamePointer> pointerMap;
        
        // Memory stuff
        void* baseAllocation=nullptr;
        u64 maxSize=0;
        u64 usedMemory=0;
        
        std::vector<Block> freeBlocks;
        std::vector<Block> usedBlocks;
        
        void insertFreeBlock(Block block);
        void insertUsedBlock(Block block);
    };
    
    HeapAllocator* GlobalHeapAllocator();

    StateAllocator* GetStateAllocator();
    void SetStateAllocator(StateAllocator* memory);
    void GameMemoryTest();
}