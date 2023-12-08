#pragma once

#include "Engone/PlatformLayer.h"
#include "Engone/Asserts.h"

#include <vector>

namespace engone{

    struct Allocator;
    // void* FnAllocate(u64 bytes, void* oldPtr, u64 newBytes)
    // allocates, reallocates, and frees
    typedef void* (*FnAllocate)(Allocator*, u64, void*, u64);

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
        inline void* allocate(u64 bytes, void* ptr = nullptr, u64 oldBytes = 0) {
            return m_func(this, bytes, ptr, oldBytes);
        }
        inline bool isNonVolatile() {
            return type > ALLOCATOR_NON_VOLATILE;
        }

    private:
        FnAllocate m_func;
    };
    struct HeapAllocator : public Allocator {
        HeapAllocator() : Allocator(ALLOCATOR_HEAP, (FnAllocate)HeapAllocator::static_allocate) { }
        // ~HeapAllocator() { }
        static inline void* static_allocate(HeapAllocator* allocator, u64 bytes, void* oldPtr, u64 oldBytes) {
            return allocator->allocate(bytes, oldPtr, oldBytes);
        }

        inline void* allocate(u64 bytes, void* oldPtr, u64 oldBytes) {
            return engone::Reallocate(oldPtr, oldBytes, bytes);
        }
    };
    struct LinearAllocator : public Allocator {
        LinearAllocator() : Allocator(ALLOCATOR_LINEAR, (FnAllocate)LinearAllocator::static_allocate) { }
        ~LinearAllocator() { cleanup(); }
        static inline void* static_allocate(LinearAllocator* allocator, u64 bytes, void* oldPtr, u64 oldBytes) {
            return allocator->allocate(bytes, oldPtr, oldBytes);
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
        inline void* allocate(u64 bytes, void* oldPtr = nullptr, u64 oldBytes = 0) {
            Assert(oldPtr == nullptr); // reallocate not allowed
            Assert(bytes != 0); // free not allowed

            // linear allocator can't reserve/resize for more memory because it would invalidate the returned pointers.
            Assert(used + bytes <= max);
            void* ptr = (u8*)data + used;
            used += bytes;
            return ptr;
        }

    private:
        void* data = nullptr;;
        u64 used = 0;
        u64 max = 0;
    };
    struct StateAllocator : public Allocator {
        StateAllocator() : Allocator(ALLOCATOR_STATE, (FnAllocate)StateAllocator::static_allocate) { }
        ~StateAllocator() { cleanup(); }
        static inline void* static_allocate(StateAllocator* allocator, u64 bytes, void* oldPtr, u64 oldBytes) {
            return allocator->reallocate(bytes, oldPtr, oldBytes);
        }

        void cleanup();
        
        bool init(void* baseAddress, u64 bytes, bool noPhysics);
        bool save(const std::string& path);
        bool load(const std::string& path);

        void* allocate(u64 bytes);
        void* reallocate(u64 bytes, void* oldPtr, u64 oldBytes);
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