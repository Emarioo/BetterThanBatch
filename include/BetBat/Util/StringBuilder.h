#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/Typedefs.h"

#include <string>

struct Token;
struct TokenRange;

// String inside the builder is null terminated
// TODO: Allocator which the builder uses
struct StringBuilder {
    StringBuilder() = default;
    ~StringBuilder() {
        cleanup();
    }
    StringBuilder& operator=(const StringBuilder& builder) = delete;
    StringBuilder& operator=(StringBuilder& builder) {
        // if(arr.used>0){
            // engone::log::out << "copy "<<arr.used<<"\n";
        // }
        
        _ptr = builder._ptr;
        len = builder.len;
        max = builder.max;
        owner = builder.owner;
        
        builder._ptr=nullptr;
        builder.len=0;
        builder.max=0;
        builder.owner=true;
        // Assert(_reserve(arr.max));
        // Assert(resize(arr.used));
        // for(int i=0;i<used;i++){
        //     _ptr[i] = arr._ptr[i];
        // }
        return *this;
    }

    StringBuilder(const StringBuilder& builder) = delete;
    StringBuilder(StringBuilder& builder){
        // if(arr.used>0){
            // engone::log::out << "copy "<<arr.used<<"\n";
        // }
        _ptr = builder._ptr;
        len = builder.len;
        max = builder.max;
        owner = builder.owner;
        
        builder._ptr=nullptr;
        builder.len=0;
        builder.max=0;
        builder.owner=true;
        // Assert(_reserve(arr.max));
        // Assert(resize(arr.used));
        // for(int i=0;i<used;i++){
        //     _ptr[i] = arr._ptr[i];
        // }
    }
    void cleanup(){
        if(owner)
            _reserve(0);
        else {
            _ptr = nullptr;
            max = 0;
            len = 0;
            owner = true;
        }
    }
    
    private:
    // These are private in case of dramatic changes
    // to how the builder works internally
    char* _ptr = nullptr; // null terminatet
    u32 max = 0; // null termination is excluded
    u32 len = 0; // null termination is excluded
    bool owner=true; // false if the pointer is borrowed
    public:

    char* data() const { return _ptr; }
    u32 size() const { return len; }

    operator std::string() {
        return std::string(_ptr);
    }
    operator const char*(){
        return _ptr;
    }
    #define OP_METHOD(T)\
        StringBuilder& operator +(const T& t){append(t);return *this;}\
        StringBuilder& operator +=(const T& t){append(t);return *this;}\
        StringBuilder& operator <<(const T& t){append(t);return *this;}
        
    #define OP_METHOD2(T)\
        StringBuilder& operator +(T t){append(t);return *this;}\
        StringBuilder& operator +=(T t){append(t);return *this;}\
        StringBuilder& operator <<(T t){append(t);return *this;}

    OP_METHOD(TokenRange)
    OP_METHOD(Token)
    OP_METHOD(std::string)
    OP_METHOD2(const char*)
    OP_METHOD2(u8)
    OP_METHOD2(char)
    OP_METHOD2(u16)
    OP_METHOD2(i16)
    OP_METHOD2(u32)
    OP_METHOD2(i64)
    OP_METHOD2(u64)
    OP_METHOD2(i32)
    OP_METHOD2(float)
    OP_METHOD2(double)

    #undef OP_METHOD

    void append(const TokenRange& tokenRange);
    void append(const Token& token);
    void append(const std::string& str){
        append(str.c_str(), str.length());
    }
    void append(u8 num)  { append((u64)num); }
    void append(u16 num) { append((u64)num); }
    void append(u32 num) { append((u64)num); }
    
    void append(i8 num)  { append((i64)num); }
    void append(i16 num) { append((i64)num); }
    void append(i32 num) { append((i64)num); }

    void append(char chr) {
        append(&chr,1);
    }
    void append(const char* str){
        Assert(str);
        append(str,strlen(str));
    }

    /*
        Base/internal functions
    */
    // str may not be null terminated
    void append(const char* str, u32 length){
        Assert(str);
        if(length==0)
            return;
        ensure(len + length);
        // str may not be null terminated
        memcpy(_ptr + len, str, length);
        len += length;
        *(_ptr + len) = '\0';
    }
    void append(i64 num){
        const u32 length = 30;
        ensure(len + length);
        // str may not be null terminated
        u32 written = snprintf(_ptr + len, length, FORMAT_64"d", num);
        Assert(written < length);
        len += written;
        *(_ptr + len) = '\0';
    }
    void append(u64 num){
        const u32 length = 30;
        ensure(len + length);
        // str may not be null terminated
        u32 written = snprintf(_ptr + len, length, FORMAT_64"u", num);
        Assert(written < length);
        len += written;
        *(_ptr + len) = '\0';
    }
    void append(float num){
        const u32 length = 30;
        ensure(len + length);
        // str may not be null terminated
        u32 written = snprintf(_ptr + len, length, "%f", num);
        Assert(written < length);
        len += written;
        *(_ptr + len) = '\0';
    }
    void append(double num){
        const u32 length = 30;
        ensure(len + length);
        // str may not be null terminated
        u32 written = snprintf(_ptr + len, length, "%lf", num);
        Assert(written < length);
        len += written;
        *(_ptr + len) = '\0';
    }
    // asserts if length couldn't be ensured.
    void ensure(u32 length) {
        if(max>=length)
            return;
        Assert(_reserve(max + length + 10));
    }
    bool _reserve(u32 newMax) {
        Assert(owner);
        if(newMax==0){
            if(max!=0){
                engone::Free(_ptr, max+1);
            }
            _ptr = nullptr;
            max = 0;
            len = 0;
            return true;
        }
        if(!_ptr){
            _ptr = (char*)engone::Allocate(newMax+1);
            Assert(_ptr);
            if(!_ptr)
                return false;
            max = newMax;
            return true;
        } else {
            char* newPtr = (char*)engone::Reallocate(_ptr, max+1, newMax+1);
            Assert(newPtr);
            if(!newPtr)
                return false;
            _ptr = newPtr;
            max = newMax;
            if(len > newMax){
                len = newMax;
            }
            return true;
        }
        return false;
    }

    // call cleanup to make the builder usable again
    // unless you borrow again
    void borrow(const char* str) {
        _ptr = (char*)str;
        len = strlen(str);
        max = len;
        owner = false;
    }
};
engone::Logger& operator<<(engone::Logger& logger, const StringBuilder& stringBuilder);