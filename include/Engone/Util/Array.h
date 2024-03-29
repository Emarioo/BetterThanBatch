#pragma once

#include "Engone/PlatformLayer.h"
// #include "Engone/Logger.h"

// #ifndef MEASURE
// #define MEASURE
// #endif

// #include "BetBat/Util/Perf.h"
#include "BetBat/Util/Tracker.h"

#include "Engone/Asserts.h"

// #define TINY_ARRAY(TYPE,NAME,SIZE) TYPE arr_##NAME[SIZE]; memset((void*)arr_##NAME,0,SIZE*sizeof(TYPE)); TinyArray<TYPE> NAME={}; NAME.initFixedSize(arr_##NAME, SIZE);
#define TINY_ARRAY(TYPE,NAME,SIZE) QuickArray<TYPE> NAME{};
// The purpose of this array is speed.
// It will not call constructors or destructors
// Few allocations
#ifdef gone
template<typename T>
struct TinyArray {
    TinyArray() = default;
    ~TinyArray() { cleanup(); }
    void cleanup(){
        _reserve(0);
    }

    T* _ptr = nullptr;
    u16 used = 0;
    u16 max = 0;
    bool owner = true;

    void initFixedSize(T* ptr, u32 count) {
        _ptr = ptr;
        used = 0;
        max = count;
        owner = false;
    }

    TinyArray<T>& operator=(const TinyArray<T>& arr) = delete;
    TinyArray(const TinyArray<T>& arr) = delete;

    bool add(const T& t){
        if(used + 1 > max){
            if(!_reserve(5 + max * 1.5)){
                return false;
            }
        }
        T* ptr = _ptr + used++;
        // Assert(((u64)ptr % alignof(T)) == 0); // TODO: alignment for placement new?
        *ptr = t;
        // new(ptr)T(t);

        return true;
    }
    // bool add(T t){
    //     return add(*(const T*)&t);
    // }
    bool pop(){
        if(!used) return false;
        T* ptr = _ptr + --used;
        // ptr->~T();
        return true;
    }
    // Shifts all elements to the right one step to the left
    // It isn't recommended to use this function.
    bool removeAt(u32 index){
        Assert(index < used);
        T* ptr = _ptr + index;
        // ptr->~T();
        --used;
        if(index != used){
            memcpy(_ptr + index, _ptr + index + 1, (used-index) * sizeof(T));
        }
        return true;
    }
    T* getPtr(u32 index) const {
        if(index >= used)
            return nullptr;
        return (_ptr + index);
    }
    T& get(u32 index) const {
        Assert(index < used);
        return *(_ptr + index);
    }
    T& operator[](u32 index) const{
        Assert(index < used);
        return *(_ptr + index);
    }
    T& last() const {
        Assert(used>0);
        return *(_ptr + used - 1);
    }
    u32 size() const {
        return used;
    }
    T* data() const {
        return _ptr;
    }
    bool _reserve(u32 newMax){
        // MEASURE
        if(newMax==0){
            if(owner){
                if(max!=0){
                    TRACK_ARRAY_FREE(_ptr, T, max);
                    // engone::Free(_ptr, max * sizeof(T));
                }
                _ptr = nullptr;
                max = 0;
            }
            used = 0;
            return true;
        }
        if(!_ptr){
            _ptr = TRACK_ARRAY_ALLOC(T, newMax);
            // _ptr = (T*)engone::Allocate(sizeof(T) * newMax);
            Assert(_ptr);
            // initialization of elements is done when adding them
            // if(!_ptr)
            //     return false;
            max = newMax;
            owner = true;
            return true;
        } else {
            if(owner) {
                TRACK_DELS(T, max);
                T* newPtr = (T*)engone::Reallocate(_ptr, sizeof(T) * max, sizeof(T) * newMax);
                TRACK_ADDS(T, newMax);
                Assert(newPtr);
                // if(!newPtr)
                //     return false;
                _ptr = newPtr;
                max = newMax;
            } else if(newMax > max) {
                T* newPtr = TRACK_ARRAY_ALLOC(T, newMax);
                // T* newPtr = (T*)engone::Allocate(sizeof(T) * newMax);
                Assert(newPtr);
                memcpy((void*)newPtr, (void*)_ptr, used * sizeof(T));
                
                // initialization of elements is done when adding them
                // if(!_ptr)
                //     return false;
                _ptr = newPtr;
                max = newMax;
                owner = true;
            } else {
                // do nothing if memory isn't owned
            }
            if(used > newMax){
                used = newMax;
            }
            return true;
        }
        return false;
    }
    // Will not shrink alloction to fit the new size
    bool resize(u32 newSize){
        if(newSize>max){
            bool yes = _reserve(newSize);
            if(!yes)
                return false;
        }
        used = newSize;
        return true;
    }
    T* begin() const {
        return _ptr;
    }
    T* end() const {
        return _ptr + used;
    }
};
#endif

// Do not pop or add elements while iterating.
// Be careful at least.
template<typename T>
struct DynamicArray {
    DynamicArray() = default;
    ~DynamicArray() { cleanup(); }
    void cleanup(){
        _reserve(0);
    }

    DynamicArray(DynamicArray<T>& arr) = delete;
    DynamicArray<T>& operator=(const DynamicArray<T>& arr) {
        // if(arr.used>0){
            // engone::log::out << "copy "<<arr.used<<"\n";
        // }
        // bool yes = _reserve(arr.max);
        // Assert(yes);
        bool yes = resize(arr.used);
        Assert(yes);
        for(u32 i=0;i<used;i++){
            _ptr[i] = arr._ptr[i];
        }
        return *this;
    }

    // DynamicArray(const DynamicArray<T>& arr) = delete;
    DynamicArray(const DynamicArray<T>& arr){
        // if(arr.used>0){
            // engone::log::out << "copy "<<arr.used<<"\n";
        // }
        // bool yes = _reserve(arr.max);
        // Assert(yes);
        bool yes = resize(arr.used);
        Assert(yes);
        for(u32 i=0;i<used;i++){
            _ptr[i] = arr._ptr[i];
        }
    }

    T* _ptr = nullptr;
    u32 used = 0;
    u32 max = 0;

    bool add(const T& t){
        if(used + 1 > max){
            if(!_reserve(1 + max * 1.5)){
                return false;
            }
        }
        T* ptr = _ptr + used++;
        Assert(((u64)ptr % alignof(T)) == 0); // TODO: alignment for placement new?
        new(ptr)T(t);

        return true;
    }
    // bool add(T t){
    //     return add(*(const T*)&t);
    // }
    bool pop(){
        if(used==0)
            return false;
        T* ptr = _ptr + --used;
        ptr->~T();
        return true;
    }
    // Shifts all elements to the right one step to the left
    // It isn't recommended to use this function.
    bool removeAt(u32 index){
        Assert(index < used);
        T* ptr = _ptr + index;
        ptr->~T();
        // PROBABLY BUG HERE
        if(index != used - 1){ // if we didn't remove the last element
            // this is not beautiful but required for std::string to work
            new(ptr)T();
            for(u32 i = index; i < used - 1; i++){
                T* a = _ptr + i;
                T* b = _ptr + i + 1;
                *(a) = std::move(*(b));
            }
            T* lastPtr = _ptr + used - 1;
            lastPtr->~T();
            // doesn't work with std::string
            // memcpy((void*)(_ptr + index), _ptr + index + 1, (used-index) * sizeof(T));
        }
        --used;
        return true;
    }
    T* getPtr(u32 index) const {
        if(index >= used)
            return nullptr;
        return (_ptr + index);
    }
    T& get(u32 index) const {
        Assert(index < used);
        return *(_ptr + index);
    }
    T& operator[](u32 index) const{
        Assert(index < used);
        return *(_ptr + index);
    }
    T& last() const {
        Assert(used>0);
        return *(_ptr + used - 1);
    }
    u32 size() const {
        return used;
    }
    T* data() const {
        return _ptr;
    }
    bool _reserve(u32 newMax){
        // MEASURE
        if(newMax==0){
            if(max!=0){
                for(u32 i = 0; i < used; i++){
                    (_ptr + i)->~T();
                }
                // engone::Free(_ptr, max * sizeof(T));
                TRACK_ARRAY_FREE(_ptr, T, max);
            }
            _ptr = nullptr;
            max = 0;
            used = 0;
            return true;
        }
        if(!_ptr){
            // _ptr = (T*)engone::Allocate(sizeof(T) * newMax);
            _ptr = TRACK_ARRAY_ALLOC(T, newMax);
            Assert(_ptr);
            // initialization of elements is done when adding them
            if(!_ptr)
                return false;
            max = newMax;
            return true;
        } else {
            
            // destruct if we down-scale
            // if(newMax < max) {
            //     for(u32 i = newMax; i < used; i++){
            //         (_ptr + i)->~T();
            //     }
            //     used = newMax;
            // }

            // T* newPtr = (T*)engone::Reallocate(_ptr, sizeof(T) * max, sizeof(T) * newMax);
            T* newPtr = (T*)engone::Allocate(sizeof(T) * newMax);
            Assert(newPtr);
            
            for(u32 i = 0; i < used; i++){
                new(newPtr + i)T();
                *(newPtr + i) = std::move(*(_ptr + i));
                (_ptr + i)->~T();
            }
            
            engone::Free(_ptr, sizeof(T) * max);

            if (newMax > max) {
                TRACK_ADDS(T, newMax - max);
            } else if(newMax < max) {
                TRACK_DELS(T, max - newMax);
            }

            if(!newPtr)
                return false;
            _ptr = newPtr;
            max = newMax;
            if(used > newMax){
                used = newMax;
            }
            return true;
        }
        return false;
    }
    // Will not shrink alloction to fit the new size
    bool resize(u32 newSize){
        if(newSize>max){
            bool yes = _reserve(newSize);
            if(!yes)
                return false;
        }
        if(newSize > used) {
            for(u32 i = used; i<newSize;i++){
                new(_ptr+i)T();
            }
        } else if(newSize < used){
            for(u32 i = newSize; i<used;i++){
                (_ptr + i)->~T();
            }
        }
        used = newSize;
        return true;
    }
    T* begin() const {
        return _ptr;
    }
    T* end() const {
        return _ptr + used;
    }
    void stealFrom(DynamicArray<T>& arr){
        cleanup();
        _ptr = arr._ptr;
        used = arr.used;
        max = arr.max;
        arr._ptr = nullptr;
        arr.used = 0;
        arr.max = 0;
    }
    // void stealFrom(TinyArray<T>& arr){
    //     cleanup();
    //     if(!arr.owner) {
    //         // _ptr = (T*)engone::Allocate(arr.used * sizeof(T));
    //         _ptr = TRACK_ARRAY_ALLOC(T, arr.used);
    //         used = arr.used;
    //         max = arr.used;
    //         arr.used = 0;
    //     } else {
    //         _ptr = arr._ptr;
    //         used = arr.used;
    //         max = arr.max;
    //         arr._ptr = nullptr;
    //         arr.used = 0;
    //         arr.max = 0;
    //     }
    // }
};

// Does not call any constructors or destructors.
// good when using integers, floats, pointers...
template<typename T>
struct QuickArray {
    QuickArray() = default;
    ~QuickArray() { cleanup(); }
    void cleanup(){
        _reserve(0);
    }

    // QuickArray(QuickArray<T>& arr) = delete;
    // QuickArray<T>& operator=(const QuickArray<T>& arr) = delete;
    // QuickArray<T>& operator=(const QuickArray<T>& arr) {
    //     // if(arr.used>0){
    //         // engone::log::out << "copy "<<arr.used<<"\n";
    //     // }
    //     bool yes = _reserve(arr.max);
    //     Assert(yes);
    //     yes = resize(arr.used);
    //     Assert(yes);
    //     for(int i=0;i<used;i++){
    //         _ptr[i] = arr._ptr[i];
    //     }
    //     return *this;
    // }

    // DynamicArray(const DynamicArray<T>& arr) = delete;
    // QuickArray(const QuickArray<T>& arr) = delete;
    // {
    //     // if(arr.used>0){
    //         // engone::log::out << "copy "<<arr.used<<"\n";
    //     // }
    //     bool yes = _reserve(arr.max);
    //     Assert(yes);
    //     yes = resize(arr.used);
    //     Assert(yes);
    //     for(int i=0;i<used;i++){
    //         _ptr[i] = arr._ptr[i];
    //     }
    // }

    T* _ptr = nullptr;
    u32 used = 0;
    u32 max = 0;

    bool add(const T& t){
        if(used + 1 > max){
            if(!_reserve(5 + max * 1.5)){
                return false;
            }
        }
        T* ptr = _ptr + used++;
        // Assert(((u64)ptr % alignof(T)) == 0); // TODO: alignment for placement new?
        // new(ptr)T(t);

        *ptr = t;

        return true;
    }
    // bool add(T t){
    //     return add(*(const T*)&t);
    // }
    bool pop(){
        if(!used) return false;
        T* ptr = _ptr + --used;
        // ptr->~T();
        return true;
    }
    // Shifts all elements to the right one step to the left
    // It isn't recommended to use this function.
    bool removeAt(u32 index){
        Assert(index < used);
        T* ptr = _ptr + index;
        // ptr->~T();
        --used;
        if(index != used){
            memcpy(_ptr + index, _ptr + index + 1, (used-index) * sizeof(T));
        }
        return true;
    }
    T* getPtr(u32 index) const {
        if(index >= used)
            return nullptr;
        return (_ptr + index);
    }
    T& get(u32 index) const {
        Assert(index < used);
        return *(_ptr + index);
    }
    T& operator[](u32 index) const{
        Assert(index < used);
        return *(_ptr + index);
    }
    T& last() const {
        Assert(used>0);
        return *(_ptr + used - 1);
    }
    u32 size() const {
        return used;
    }
    T* data() const {
        return _ptr;
    }
    bool _reserve(u32 newMax){
        // MEASURE
        if(newMax==0){
            if(max!=0){
                // for(u32 i = 0; i < used; i++){
                //     (_ptr + i)->~T();
                // }
                TRACK_ARRAY_FREE(_ptr, T, max);
                // engone::Free(_ptr, max * sizeof(T));
            }
            _ptr = nullptr;
            max = 0;
            used = 0;
            return true;
        }
        if(!_ptr){
            _ptr = TRACK_ARRAY_ALLOC(T, newMax);
            // _ptr = (T*)engone::Allocate(sizeof(T) * newMax);
            Assert(_ptr);
            // initialization of elements is done when adding them
            if(!_ptr)
                return false;
            max = newMax;
            return true;
        } else {
            // if(newMax < max) {
            //     for(u32 i = newMax; i < used; i++){
            //         (_ptr + i)->~T();
            //     }
            // }
            TRACK_DELS(T, max);
            T* newPtr = (T*)engone::Reallocate(_ptr, sizeof(T) * max, sizeof(T) * newMax);
            TRACK_ADDS(T, newMax);
            Assert(newPtr);
            if(!newPtr)
                return false;
            _ptr = newPtr;
            max = newMax;
            if(used > newMax){
                used = newMax;
            }
            return true;
        }
        return false;
    }
    // Will not shrink alloction to fit the new size
    bool resize(u32 newSize){
        if(newSize>max){
            bool yes = _reserve(newSize);
            if(!yes)
                return false;
        }
        if(newSize > used) {
            memset((void*)(_ptr+used),0,(newSize-used) * sizeof(T));
        }
        // if(newSize > used) {
        //     for(u32 i = used; i<newSize;i++){
        //         new(_ptr+i)T();
        //     }
        // } else if(newSize < used){
        //     for(u32 i = newSize; i<used;i++){
        //         (_ptr + i)->~T();
        //     }
        // }
        used = newSize;
        return true;
    }
    T* begin() const {
        return _ptr;
    }
    T* end() const {
        return _ptr + used;
    }
    void stealFrom(QuickArray<T>& arr){
        cleanup();
        _ptr = arr._ptr;
        used = arr.used;
        max = arr.max;
        arr._ptr = nullptr;
        arr.used = 0;
        arr.max = 0;
    }
    // void stealFrom(TinyArray<T>& arr){
    //     cleanup();
    //     if(!arr.owner) {
    //         _ptr = (T*)engone::Allocate(arr.used * sizeof(T));
    //         used = arr.used;
    //         max = arr.used;
    //         arr.used = 0;
    //     } else {
    //         _ptr = arr._ptr;
    //         used = arr.used;
    //         max = arr.max;
    //         arr._ptr = nullptr;
    //         arr.used = 0;
    //         arr.max = 0;
    //     }
    // }
};