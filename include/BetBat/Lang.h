/*
    This file contains code/structs that are shared within the language.
    The compiler communicates with the users code using these.
*/

#include "Engone/Logger.h"

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
}