#pragma once

#include "Engone/Logger.h"
#include "Engone/Typedefs.h"
#include "Engone/Util/Allocator.h"

#include <string>


struct Token;
struct TokenRange;

struct StringView {
    StringView() {  }
    StringView(const char* cstr) {
        ptr = cstr;
        len = strlen(cstr);
    }
    StringView(const char* cstr, int len) {
        ptr = cstr;
        this->len = len;
    }
    StringView(const std::string& str) {
        // TODO: Memory leak. Some part of the code damaged the heap. I believe this could be a culprit.
        //   There may be more though so I am leaving it like this.
        std::string* s = new std::string(str);
        ptr = s->c_str();
        len = s->length();
        // ptr = str.c_str();
        // len = str.length();
    }

    const char* ptr=nullptr;
    u16 len=0;
    bool equals(const char* str) const {
        int slen = strlen(str);
        if(len!=slen) return false;
        for(int i=0;i<len;i++) if(str[i] != ptr[i]) return false;
        return true;
    }
    bool equals(const char* str, int size) const {
        if(len!=size) return false;
        for(int i=0;i<len;i++) if(str[i] != ptr[i]) return false;
        return true;
    }
    operator std::string() const {
        return std::string(ptr,len);
    }
    int size() const { return len; }


};
bool operator==(const std::string& str, const StringView& view);
bool operator==(const StringView& view, const std::string& str);
bool operator==(const StringView& str, const StringView& view);
bool operator==(const StringView& view, const char* str);

// String inside the builder is null terminated
// TODO: Allocator which the builder uses
struct StringBuilder {
    StringBuilder() = default;
    ~StringBuilder() {
        cleanup();
    }
    // StringBuilder(const char* str) {
    //     borrow(str);
    // }
    StringBuilder& operator=(const StringBuilder& builder) = delete;
    StringBuilder& operator=(StringBuilder& builder) {
        // if(arr.used>0){
            // engone::log::out << "copy "<<arr.used<<"\n";
        // }
        
        _ptr = builder._ptr;
        len = builder.len;
        max = builder.max;
        borrowing = builder.borrowing;
        
        builder._ptr=nullptr;
        builder.len=0;
        builder.max=0;
        builder.borrowing=false;
        // bool yes = _reserve(arr.max);
        // Assert(yes);
        // yes = resize(arr.used);
        // Assert(yes);
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
        borrowing = builder.borrowing;
        
        builder._ptr=nullptr;
        builder.len=0;
        builder.max=0;
        builder.borrowing=false;
        // Assert(bool yes = _reserve(arr.max));
        // Assert(yes = resize(arr.used));
        // for(int i=0;i<used;i++){
        //     _ptr[i] = arr._ptr[i];
        // }
    }
    void cleanup(){
        if(!borrowing)
            _reserve(0);
        else {
            _ptr = nullptr;
            max = 0;
            len = 0;
            borrowing = false;
        }
    }

    char* data() const { return _ptr; }
    u32 size() const { return len; }

    operator std::string() {
        return std::string(_ptr);
    }
    operator const char*(){
        return _ptr;
    }
    #define OP_METHOD_CREF(T)\
        StringBuilder& operator +(const T& t){append(t);return *this;}\
        StringBuilder& operator +=(const T& t){append(t);return *this;}\
        StringBuilder& operator <<(const T& t){append(t);return *this;}
        
    #define OP_METHOD(T)\
        StringBuilder& operator +(T t){append(t);return *this;}\
        StringBuilder& operator +=(T t){append(t);return *this;}\
        StringBuilder& operator <<(T t){append(t);return *this;}

    OP_METHOD_CREF(StringView)
    // OP_METHOD_CREF(TokenRange)
    // OP_METHOD_CREF(Token)
    OP_METHOD_CREF(std::string)
    OP_METHOD(const char*)
    OP_METHOD(u8)
    OP_METHOD(char)
    OP_METHOD(u16)
    OP_METHOD(i16)
    OP_METHOD(u32)
    OP_METHOD(i64)
    OP_METHOD(u64)
    OP_METHOD(i32)
    OP_METHOD(float)
    OP_METHOD(double)

    #undef OP_METHOD

    // void append(const TokenRange& tokenRange);
    // void append(const Token& token);
    void append(const std::string& str){
        append(str.c_str(), str.length());
    }
    void append(const StringView& str){
        append(str.ptr, str.len);
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
        bool yes = _reserve(max + length + 10);
        Assert(yes);
    }
    bool _reserve(u32 newMax) {
        Assert(!borrowing);
        if(newMax==0){
            if(max!=0){
                TRACK_ARRAY_FREE(_ptr, char, max+1);
                // engone::Free(_ptr, max+1);
            }
            _ptr = nullptr;
            max = 0;
            len = 0;
            return true;
        }
        if(!_ptr){
            // _ptr = (char*)engone::Allocate(newMax+1);
            _ptr = TRACK_ARRAY_ALLOC(char, newMax+1);
            Assert(_ptr);
            if(!_ptr)
                return false;
            max = newMax;
            return true;
        } else {
            char* newPtr = TRACK_ARRAY_REALLOC(_ptr, char, max+1, newMax+1);
            // TRACK_DELS(char, max + 1);
            // char* newPtr = (char*)engone::Reallocate(_ptr, max+1, newMax+1);
            // TRACK_ADDS(char, newMax+1);
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
        if(!str) return;
        _ptr = (char*)str;
        len = strlen(str);
        max = len;
        borrowing = true;
    }
    
    void steal(StringBuilder* builder) {
        cleanup();
        if(builder->borrowing) // can't steal from someone who is borrowing
            return;
        
        _ptr = (char*)builder->_ptr;
        len = builder->len;
        max = builder->max;
        borrowing = false;
        
        builder->_ptr = nullptr;
        builder->len = 0;
        builder->max = 0;
    }
private:
    // These are private in case of dramatic changes
    // to how the builder works internally
    char* _ptr = nullptr; // null terminatet
    u32 max = 0; // null termination is excluded
    u32 len = 0; // null termination is excluded
    bool borrowing=false;
};
engone::Logger& operator<<(engone::Logger& logger, const StringBuilder& stringBuilder);