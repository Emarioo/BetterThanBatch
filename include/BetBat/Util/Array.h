#include "Engone/PlatformLayer.h"

// Do not pop or add elements while iterating
template<typename T>
struct DynamicArray {
    DynamicArray() = default;
    ~DynamicArray() { cleanup(); }
    void cleanup(){
        _reserve(0);
    }

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
        Assert(((u64)ptr % alignof(T)) == 0) // TODO: alignment for placement new?
        new(ptr)T(t);

        return true;
    }
    // bool add(T t){
    //     return add(*(const T*)&t);
    // }
    bool pop(){
        if(!used) return false;
        T* ptr = _ptr + used--;
        ptr->~T();
        return true;
    }
    T* getPtr(u32 index) const {
        if(index >= used)
            return nullptr;
        return *(_ptr + index);
    }
    T& get(u32 index) const {
        Assert(index < used);
        return *(_ptr + index);
    }
    T& operator[](u32 index) const{
        Assert(index < used);
        return *(_ptr + index);
    }
    u32 size() const {
        return used;
    }

    bool _reserve(u32 newMax){
        if(newMax==0){
            if(max!=0){
                for(u32 i = 0; i < used; i++){
                    (_ptr + i)->~T();
                }
                engone::Free(_ptr, max * sizeof(T));
            }
            _ptr = nullptr;
            max = 0;
            used = 0;
            return true;
        }
        if(!_ptr){
            _ptr = (T*)engone::Allocate(sizeof(T) * newMax);
            // initialization of elements is done when adding them
            if(!_ptr)
                return false;
            max = newMax;
            return true;
        } else {
            if(newMax < max) {
                for(u32 i = newMax; i < used; i++){
                    (_ptr + i)->~T();
                }
            }
            T* newPtr = (T*)engone::Reallocate(_ptr, sizeof(T) * max, sizeof(T) * newMax);
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
        for(u32 i = used-1; i<newSize;i++){
            Assert(((u64)(_ptr+i) % alignof(T)) == 0) // TODO: alignment for placement new?
            new(_ptr+i)T();
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
};