/*
    Part of Lexer.cpp
*/

#include "Lexer.h"

#include "Engone/PlatformLayer.h"

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdint.h"

#ifdef COMPILER_MSVC
    #include "intrin.h"
#else
    #include "immintrin.h"
#endif

typedef uint64_t Key;

// #define string_to_key _string_to_key1
#define string_to_key _string_to_key3
#define HASH_MUL 359426152
#define HASH_SHIFT 31

static inline Key _string_to_key1(const char* text, int len) {
    Key key = 0;
    for (int i = 0; i < len; i++)
        key = (key << 8) | text[i];
    return key;
}

static inline Key _string_to_key3(const char* text, int len) {
    Key key = 0;
    key = *(Key*)text;
    #ifdef COMPILER_MSVC
        key = _byteswap_uint64(key);
    #else
        key = __builtin_bswap64(key);
    #endif
    key = key >> ((8-len)*8);
    return key;
}

// #define string_to_key _string_to_key2
// #define HASH_MUL 674480080
// #define HASH_SHIFT 38
static inline Key _string_to_key2(const char* text, int len) {
    Key key = 0;
    key = *(Key*)text;
    #ifdef __BMI2__
        return (Key)_bzhi_u64(key, len * 8);
    #else
        if(len != 8) {
            Key mask = ((Key)1 << ((len<<3))) - (Key)1;
            key &= mask;
        }
        return key;
    #endif
}

struct Keyword {
    const char* val;
    Key key;
};
struct HashWord {
    Key key;
    lexer::TokenType kind;
};
#define KEYWORD_LEN (uint64_t)26
extern Keyword keywords[KEYWORD_LEN];
static HashWord table[KEYWORD_LEN];
static Key test_keys[KEYWORD_LEN];

static inline int hash(Key key) {
    return ((key * HASH_MUL) >> HASH_SHIFT) % KEYWORD_LEN;
}

int construct_keyword_table() {
    static int* map;
    if(map)
        return 0;

    map = (int*)malloc(4 * KEYWORD_LEN);
    Assert(map);

    for (int i=0;i<KEYWORD_LEN;i++) {
        keywords[i].key = string_to_key(keywords[i].val, strlen(keywords[i].val));
    }

    memset(map, 0, 4*KEYWORD_LEN);
    int collisions = 0;
    for (int i=0;i<KEYWORD_LEN;i++) {
        int index = hash(keywords[i].key);
        
        // printf("%s -> %d\n", keywords[i].val, index);

        table[index].key = keywords[i].key;
        table[index].kind = (lexer::TokenType)(lexer::TOKEN_KEYWORD_BEGIN + i);
        if(++map[index] > 1) {
            collisions++;
        }
    }

    return collisions;
}

static inline int hash_test(Key key, Key mul, Key shift) {
    return ((key * mul) >> shift) % KEYWORD_LEN;
}

#define THREAD_COUNT 31
struct TopContext {
    volatile Key next_mul;
    volatile Key stride;
};
struct Context {
    TopContext* top_context;
    int id;
    Key end_mul;
};

unsigned int thread_work(void* arg) {
    u8* map = (u8*)malloc(KEYWORD_LEN);
    Assert(map);

    Context* context = (Context*)arg;

    Key mul = 0;
    Key best_mul = 1;
    Key best_shift = 0;
    int best_collisions = 999999;
    while(best_collisions != 0) {
        if(mul >= context->end_mul) {
            context->end_mul = engone::atomic_add64((volatile i64*)&context->top_context->next_mul, context->top_context->stride);
            mul = context->end_mul - context->top_context->stride;

            printf("[%d] At mod %llu\n", context->id, mul);
        }

        for(Key shift=0;shift<64;shift++) {
            memset(map, 0, KEYWORD_LEN);

            int collisions = 0;
            // We could use SIMD instructions here
            for(int i=0;i<KEYWORD_LEN;i++) {
                int index = hash_test(test_keys[i], mul, shift);
                if(++map[index] > 1) {
                    collisions++;
                    break;
                }
            }

            if(collisions < best_collisions) {
                printf("[%d] Better mod %llu, shift %llu, with %d collisions\n", context->id, mul, shift, collisions);
                best_collisions = collisions;
                best_mul = mul;
                best_shift = shift;

                if(collisions == 0) {
                    Assert(false);
                    break;
                }
            }
        }

        mul++;
    }

    free(map);
    return 0;
}

void start_threads() {
    using namespace engone;

    Thread threads[THREAD_COUNT] = {};
    Context contexts[THREAD_COUNT] = {};
    TopContext top_context = {};

    top_context.next_mul = 0;
    top_context.stride = 1000000;
    
    for(int i=0;i<THREAD_COUNT;i++) {
        contexts[i].top_context = &top_context;
        contexts[i].id = i;
        threads[i].init(thread_work, (void*)&contexts[i]);
    }

    for(int i=0;i<THREAD_COUNT;i++) {
        threads[i].join();
    }
}
void find_collision_free_table() {
    for (int i=0;i<KEYWORD_LEN;i++) {
        test_keys[i] = string_to_key(keywords[i].val, strlen(keywords[i].val));
    }

    start_threads();
}



// int main() {
//     int collisions = construct_table();
//     collisions++;

//     if(collisions) {
//         printf("%d collisions, finding collision free table\n", collisions);
//         find_collision_free_table();

//         return 0;
//     }



//     printf("No collisions");
//     return 0;
// }


lexer::TokenType convert_token_type(StringView text) {
    Key key = string_to_key(text.ptr, text.size());
    int index = hash(key);

    // printf("%.*s %d %d\n", text.len, text.ptr, index, table[index].kind);

    if(table[index].key == key) {
        return table[index].kind;
    }
    return lexer::TOKEN_EOF;
}

Keyword keywords[KEYWORD_LEN] = {
    { "null" },
    { "true" },
    { "false" },
    { "sizeof" },
    { "nameof" },
    { "typeid" },
    { "if" },
    { "else" },
    { "while" },
    { "for" },
    { "switch" },
    { "defer" },
    { "return" },
    { "break" },
    { "continue" },
    { "using" },
    { "struct" },
    { "fn" },
    { "operator" },
    { "enum" },
    // { "namespace" },
    { "union" },
    { "asm" },
    { "test" }, 
    { "try" },
    { "catch" },
    { "finally" },
};