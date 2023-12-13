/*
    
*/
#pragma once

#include "Engone/Util/Array.h"
#include "Engone/Util/Allocator.h"

// An artificial array of bytes. The allocations is handled within.
// Most functions return bool which indicates whether the function succeded or not.
struct ByteStream {
    static ByteStream* Create(engone::Allocator* allocator) {
        if(!allocator)
            allocator = engone::GlobalHeapAllocator();
        
        auto ptr = (ByteStream*)allocator->allocate(sizeof(ByteStream));
        new(ptr)ByteStream();
        
        ptr->m_allocator = allocator;
        return ptr;
    }
    static void Destroy(ByteStream* stream, engone::Allocator* allocator) {
        stream->~ByteStream();
        if(!allocator)
            allocator = engone::GlobalHeapAllocator();
        allocator->allocate(0, stream, sizeof(ByteStream));
    }
    ~ByteStream() {
        cleanup();
    }
    void cleanup() {
        for(int i=0;i<(int)allocations.size();i++) {
            auto& a = allocations[i];
            m_allocator->allocate(0, a.ptr, a.max);
        }
        allocations.cleanup();
        writtenBytes = 0;
    }
    bool write(const void* ptr, u32 size) {
        Assert(ptr && size != 0);
        // TODO: You can optimize memory utilization in allocations by splitting the memcpy into two.
        //  Half of ptr data goes into last allocation, the data that doesn't fit goes into a new one.
        //  write_late cannot do this optimization since the user will do the memcpy on the ONE pointer it returns.
        void* reserved_ptr = nullptr;
        bool suc = write_late(&reserved_ptr, size);
        if(!suc)
            return false;
        memcpy(reserved_ptr, ptr, size);
        return true;
    }
    bool write_late(void** out_ptr, u32 size) {
        Assert(size != 0);
        Assert(!unknown_state);
        if(out_ptr)
            *out_ptr = nullptr;
        
        Allocation* allocation = nullptr;
        if(allocations.size() > 0)
            allocation = &allocations.last();
        
        if(!allocation || allocation->writtenBytes + size > allocation->max) {
            bool suc = allocations.add({});
            if(!suc)
                return false;
            
            allocation = &allocations.last();
            allocation->max = size*2 + writtenBytes * 1.5; // MEMORY GROWTH
            allocation->ptr = (u8*)m_allocator->allocate(allocation->max);
            if(!allocation->ptr) {
                allocations.removeAt(allocations.size()-1);
                return false;   
            }
        }
        
        if(out_ptr)
            *out_ptr = allocation->ptr + allocation->writtenBytes;
        allocation->writtenBytes += size;
        writtenBytes+=size;
        return true;
    }
    // ALWAYS call wrote_unknown after calling this
    bool write_unknown(void** out_ptr, u32 max_size) {
        Assert(size != 0);
        if(out_ptr)
            *out_ptr = nullptr;
        
        Allocation* allocation = nullptr;
        if(allocations.size() > 0)
            allocation = &allocations.last();
        
        if(!allocation || allocation->writtenBytes + max_size > allocation->max) {
            bool suc = allocations.add({});
            if(!suc)
                return false;
            
            allocation = &allocations.last();
            allocation->max = max_size*2 + writtenBytes * 1.5; // MEMORY GROWTH
            allocation->ptr = (u8*)m_allocator->allocate(allocation->max);
            if(!allocation->ptr) {
                allocations.removeAt(allocations.size()-1);
                return false;   
            }
        }
        
        if(out_ptr)
            *out_ptr = allocation->ptr + allocation->writtenBytes;
        unknown_state = true;
        return true;
    }
    // you must have called wrote_unknown before calling this
    void wrote_unknown(u32 size) {
        Assert(size != 0);
        Assert(unknown_state);
        
        Allocation* allocation = nullptr;
        if(allocations.size() > 0)
            allocation = &allocations.last();
        Assert(allocation);
        Assert(allocation->writtenBytes + size <= allocation->max);
        
        allocation->writtenBytes += size;
        writtenBytes += size;
        unknown_state = false;
    }
    bool read(u32 offset, void* ptr, u32 size) {
        int head = 0;
        for(int i=0;i<allocations.size();i++) {
            Allocation* all = allocations[i];
            
            int rel_off = offset - head; // relative offset in the allocation
            int rem_size = all->writtenBytes - rel_off; // remaining size of the allocation from the relative offset
            
            if(rem_size <= 0) {
                head += all->writtenBytes;
                continue;
            }
            
            int real_size = size;
            if(real_size > rem_size) {
                real_size = rem_size;   
            }
            
            memcpy(ptr + head - offset, all->ptr + rel_off, real_size);
            offset += real_size;
            size -= real_size;
            
            head += all->writtenBytes;
        }
        
        return size == 0;
    }
    u32 getWriteHead() const { return writtenBytes; }
    bool steal_from(ByteStream* stream) {
        Assert(stream);
        Assert(m_allocator == stream->m_allocator);
        // TODO: The last allocation of "this" and the first allocation of "stream" can be combined somewhat
        
        for(int i=0;i<(int)stream->allocations.size();i++) {
            auto& all = stream->allocations[i];
            allocations.add(all);
            writtenBytes += all.writtenBytes;
        }
        stream->allocations.cleanup();
        stream->writtenBytes = 0;
        return true;
    }
    // combines all "stray" allocations into one
    // 
    bool finalize(void** out_ptr, u32* out_size) {
        Assert(out_ptr && out_size != 0);
        if(out_ptr)
            *out_ptr = nullptr;
        if(out_size)
            *out_size = 0;
        
        int max = writtenBytes;
        u8* ptr = (u8*)m_allocator->allocate(max);
        if(!ptr)
            return false;
            
        // combine allocations into one
        int offset = 0;
        for(int i=0;i<(int)allocations.size();i++) {
            auto& a = allocations[i];
            Assert(a.writtenBytes != 0 && a.ptr);
            memcpy(ptr + offset, a.ptr, a.writtenBytes);
            offset += a.writtenBytes;
            
            m_allocator->allocate(0, a.ptr, a.max);
        }
        Assert(offset == writtenBytes);
        
        allocations.resize(1);
        allocations.last().ptr = ptr;
        allocations.last().max = max;
        allocations.last().writtenBytes = writtenBytes;
        
         if(out_ptr)
            *out_size = writtenBytes;
         if(out_size)
            *out_ptr = ptr;
        return true;
    }
    struct Iterator {
        u8* ptr = nullptr;
        u32 size = 0;
    
        u32 m_readBytes = 0;
        u32 m_index = 0;
    };
    // Iterates thorugh all allocations in a contiguous fashion.
    // Do not use a write operation such as write, write_late, finalize
    // while iterating. The iterator may crash.
    bool iterate(Iterator& iterator, u32 byteLimit = -1) {
        iterator.ptr = nullptr;
        iterator.size = 0;
        
        while(true) {
            Allocation* all = nullptr;
            
            if((u32)iterator.m_index < allocations.size()) {
                all = &allocations[iterator.m_index];
            } else {
                break;
            }
            
            if(all->writtenBytes == iterator.m_readBytes) {
                iterator.m_readBytes = 0;
                iterator.m_index++;
                continue;
            }
                
            iterator.size = all->writtenBytes - iterator.m_readBytes;
            if(iterator.size > byteLimit)
                iterator.size = byteLimit;
                
            iterator.ptr = all->ptr + iterator.m_readBytes;
            iterator.m_readBytes += iterator.size;
            break;
        }
        return iterator.size != 0;
    }
    bool reserve(u32 size) {
        bool suc = allocations.add({});
        if(!suc)
            return false;
        
        auto allocation = &allocations.last();
        allocation->max = size;
        allocation->ptr = (u8*)m_allocator->allocate(allocation->max);
        if(!allocation->ptr) {
            allocations.removeAt(allocations.size()-1);
            return false;   
        }
        return true;
    }
    
    static void Test_iterator() {
        ByteStream* stream = Create(nullptr);
        
        int data[]{41,51902,51891,289,5189,251};
        
        int off = 0;
        #define ADD(S) stream->write(data + off, S); off += S; 
        ADD(1);
        ADD(2);
        ADD(1);
        ADD(1);
        ADD(4);
        ADD(6);
        ADD(9);
        ADD(3);
        #undef ADD
        
        stream->finalize(nullptr, nullptr);
        
        Iterator iterator{};
        while(stream->iterate(iterator)) {
            printf("%d: %u bytes - %p\n", iterator.m_index, iterator.size, iterator.ptr);
        }
    }
    
    // Useful methods
    bool write(const char* ptr) {
        Assert(ptr);
        int size = strlen(ptr) + 1;
        void* reserved_ptr = nullptr;
        bool suc = write_late(&reserved_ptr, size);
        if(!suc)
            return false;
        memcpy(reserved_ptr, ptr, size);
        return true;
    }
    bool write8(u64 value) {
        u8* ptr = (u8*)&value;
        int size = sizeof(value);
        void* reserved_ptr = nullptr;
        bool suc = write_late(&reserved_ptr, size);
        if(!suc)
            return false;
        memcpy(reserved_ptr, ptr, size);
        return true;
    }
    bool write4(u32 value) {
        u8* ptr = (u8*)&value;
        int size = sizeof(value);
        void* reserved_ptr = nullptr;
        bool suc = write_late(&reserved_ptr, size);
        if(!suc)
            return false;
        memcpy(reserved_ptr, ptr, size);
        return true;
    }
    bool write2(u16 value) {
        u8* ptr = (u8*)&value;
        int size = sizeof(value);
        void* reserved_ptr = nullptr;
        bool suc = write_late(&reserved_ptr, size);
        if(!suc)
            return false;
        memcpy(reserved_ptr, ptr, size);
        return true;
    }
    bool write1(u8 value) {
        u8* ptr = (u8*)&value;
        int size = sizeof(value);
        void* reserved_ptr = nullptr;
        bool suc = write_late(&reserved_ptr, size);
        if(!suc)
            return false;
        memcpy(reserved_ptr, ptr, size);
        return true;
    }
    u64 read8(u32 offset, bool* success = nullptr) {
        u64 val = 0;
        bool suc = read(offset, &val, 8);
        if(success)
            *success = suc;
        return val;
    }
    u32 read4(u32 offset, bool* success = nullptr) {
        u32 val = 0;
        bool suc = read(offset, &val, 4);
        if(success)
            *success = suc;
        return val;
    }
    u16 read2(u32 offset, bool* success = nullptr) {
        u16 val = 0;
        bool suc = read(offset, &val, 2);
        if(success)
            *success = suc;
        return val;
    }
    u8 read1(u32 offset, bool* success = nullptr) {
        u8 val = 0;
        bool suc = read(offset, &val, 1);
        if(success)
            *success = suc;
        return val;
    }
    
private:
    engone::Allocator* m_allocator = nullptr;
    struct Allocation {
        u8* ptr = nullptr;
        u32 writtenBytes = 0; // writeHead relative to this allocation
        u32 max = 0; // size of allocation
    };
    DynamicArray<Allocation> allocations;
    int writtenBytes = 0; // next byte to write to
    
    bool unknown_state = false;
};
