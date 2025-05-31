/*
Compiler interface

    Structs and functions that are shared between the language and the compiler.
    Functions in the compiler can be called from a program at compile time.
    The compiler embeds structs of data in the data section.
    
    (avoid including CompilerInterface.h in headers because a change in it requires
    a recompile of the whole program)
*/

#pragma once

#include "Engone/PlatformLayer.h"

struct Compiler;
extern Compiler* global_compiler;

extern "C" {
namespace lang {

enum Primitive : u8 {
    NONE            = 0,
    STRUCT          = 1,
    ENUM            = 2,
    SIGNED_INT      = 3,
    UNSIGNED_INT    = 4,
    DECIMAL         = 5,
    CHAR            = 6,
    BOOL            = 7,
    FUNCTION        = 8,
    ARRAY           = 9,
};
struct Range {
    i32 beg;
    i32 end;
};
struct Slice {
    void* ptr;
    i64 len;
};
struct TypeId {
    u16 index0;
    u8 index1;
    u8 ptr_level;
};
struct TypeInfo {
    Primitive type;
    u8 _pad; // reserved?
    u16 size;
    Range name; // refers to lang_strings

    // TODO: enums needs a different kind of members, perhaps TypeMember could be a union
    Range members; // refers to lang_members
};
struct TypeMember {
    Range name; // refers to lang_strings
    
    // TODO: Union for enums and struct members
    TypeId type; // refers to lang_TypeInfo
    u16 offset;

    i64 value;
};
struct BuildUnit {
    char* name;
    i64 length;
    int size;
};

// IMPORTANT: Add function to get_compiler_function so VirtualMachine has access to it!
BuildUnit* create_buildunit();
BuildUnit* current_buildunit();
void set_library_path(BuildUnit* unit, const char* name, const char* path);

engone::VoidFunction get_compiler_function(const char* name, int len);

} // namespace lang
} // extern "C"
