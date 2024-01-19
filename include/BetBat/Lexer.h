/*
    Version 2 of the tokenizer/lexer
*/
#pragma once

#include "Engone/Logger.h"
#include "Engone/Typedefs.h"
#include "Engone/Util/Allocator.h"
#include "Engone/Util/Array.h"
#include "Engone/Util/BucketArray.h"

#include "BetBat/Config.h"
#include "BetBat/Util/Utility.h"
#include "BetBat/Util/Perf.h"

// #define LEXER_DEBUG_DETAILS

namespace lexer {

    enum TokenFlags : u16 {
        TOKEN_FLAG_NEWLINE          = 0x1,
        TOKEN_FLAG_SPACE            = 0x2,
        TOKEN_FLAG_SINGLE_QUOTED    = 0x4,
        TOKEN_FLAG_DOUBLE_QUOTED    = 0x8,
        TOKEN_FLAG_HAS_DATA         = 0x10,
        TOKEN_FLAG_NULL_TERMINATED  = 0x20,
        
        // for convenience
        TOKEN_FLAG_ANY_SUFFIX       = TOKEN_FLAG_NEWLINE|TOKEN_FLAG_SPACE,
    };
    enum TokenType : u16 {
        TOKEN_NONE = 0,
        // 0-255, ascii
        TOKEN_TYPE_BEGIN = 256,

        TOKEN_EOF,
        TOKEN_IDENTIFIER,
        TOKEN_ANNOTATION,
        TOKEN_LITERAL_STRING,
        TOKEN_LITERAL_INTEGER,
        TOKEN_LITERAL_HEXIDECIMAL,
        TOKEN_LITERAL_BINARY,
        TOKEN_LITERAL_NULL,
        TOKEN_LITERAL_BOOL,

        TOKEN_TYPE_END,
    };
    extern const char* token_type_names[];
    TokenType StringToTokenType(const char* str, int len);

    #define TOKEN_ORIGIN_CHUNK_BITS 17
    #define TOKEN_ORIGIN_TOKEN_BITS 15
    #define TOKEN_ORIGIN_CHUNK_MAX (1<<TOKEN_ORIGIN_CHUNK_BITS)
    #define TOKEN_ORIGIN_TOKEN_MAX (1<<TOKEN_ORIGIN_TOKEN_BITS)
    typedef u32 TokenOrigin; // change to u64 when changing chunk/token bits
    // struct TokenOrigin {
    //     u32 data[(TOKEN_ORIGIN_CHUNK_BITS + TOKEN_ORIGIN_TOKEN_BITS) / 32
    //         + ((TOKEN_ORIGIN_CHUNK_BITS + TOKEN_ORIGIN_TOKEN_BITS) % 32 == 0 ? 0 : 1)];
    // };
    // struct TokenString {
    //     char* str;
    //     int len;  
    // };
    struct Token {
        Token() = default;
        Token(TokenType type) : type(type) {}
        TokenType type=TOKEN_NONE;
        u16 flags=0;
        TokenOrigin origin={}; // decode to get chunk and token index
        #ifdef LEXER_DEBUG_DETAILS
        const char* s = nullptr;
        char c = 0;
        #endif
    };
    struct TokenRange {
        u32 importId;
        u32 token_index_start;
        u32 token_index_end;
    };

    /*
        The lexer class is responsible for managing memory and extra information about all tokens.
        
        Should be thread safe as long as you don't read from imports that are being
        lexed. After imports/chunks have been lexed you can read as many tokens as you want.
    */
    struct Lexer {
        struct TokenInfo {
            TokenType type;
            u16 flags;
            u16 line;
            u16 column;
            u32 data_offset;
        };
        struct Chunk {
            ~Chunk() {
                engone::Allocator* allocator = engone::GlobalHeapAllocator();
                allocator->allocate(0, aux_data, aux_max);
            }
            u32 import_id;
            u32 chunk_index; // handy when you only have a chunk pointer
            QuickArray<TokenInfo> tokens; // tokenize function accesses ptr,len,max manually for optimizations, constructor destructor functionality of a DynamicArray would be ignored.
            // TokenInfo* tokens_data=nullptr;
            // u32 tokens_used=0;
            // u32 tokens_max=0;
            u8* aux_data=nullptr; // auxiliary
            u32 aux_used=0;
            u32 aux_max=0;
            
            void alloc_aux_space(u32 n) {
                if(aux_used + n > aux_max) {
                    engone::Allocator* allocator = engone::GlobalHeapAllocator();
                    u32 new_max = 0x1000 + aux_max*1.5 + n;
                    u8* new_ptr = (u8*)allocator->allocate(new_max, aux_data, aux_max);
                    Assert(new_ptr);
                    aux_data = new_ptr;
                    aux_max = new_max;
                }
                aux_used+=n;
            }
        };
        struct Import {
            u32 file_id;
            std::string path;
            DynamicArray<u32> chunk_indices;
            DynamicArray<Chunk*> chunks; // chunk pointers are stored here so we don't have to lock the chunks bucket array everytime we need a chunk.
            
            // Token getToken(u32 token_index_into_import);

            // interesting but not stricly necessary information
            int fileSize;
            int lines; // non blank/comment lines
            int comment_lines;
            int blank_lines;
        };

        // returns file id, 0 means failure
        u32 tokenize(char* text, u64 length, const std::string& path_name, u32 prepared_import_id = 0);
        u32 tokenize(const std::string& path, u32 existing_import_id = 0);

        struct FeedIterator {
            u32 file_id=0;

            u32 char_index=0;
            u32 file_token_index=0;
            
            u32 end_file_token_index=0; // exclusive
        };
        FeedIterator createFeedIterator(Token start_token, Token end_token = {});
        u32 feed(char* buffer, u32 buffer_max, FeedIterator& iterator);

        void print(u32 fileid);

        Token getTokenFromImport(u32 fileid, u32 token_index_into_import);
        // same as getDataFromToken but char instead of void
        u32 getStringFromToken(Token tok, const char** ptr);
        u32 getDataFromToken(Token tok, const void** ptr);
        
        std::string getStdStringFromToken(Token tok);

        u32 createImport(const std::string& path, Import** out_import);
        // unsafe if you have an import_id reference hanging somewhere
        void destroyImport_unsafe(u32 import_id);
        
        void appendToken(Import* imp, TokenInfo* token, StringView* string);
        Token appendToken(u32 fileId, Token token);
        Token appendToken(u32 fileId, TokenType type, u32 flags, u32 line, u32 column);
        
        bool equals_identifier(Token token, const char* str);
        
        const BucketArray<Import>& getImports() { return imports; }
        
        Import* getImport_unsafe(u32 import_id) {
            lock_imports.lock();
            auto imp = imports.get(import_id-1);
            lock_imports.unlock();
            return imp;
        }
        Chunk* getChunk_unsafe(u32 chunk_index) {
            lock_chunks.lock();
            auto imp = chunks.get(chunk_index);
            lock_chunks.unlock();
            return imp;
        }
        
        // handy functions, the implementation details of tokens per chunk, chunk index and token index may change in the future
        // so we hide it behine encode and decode functions.
        // u32 encode_origin(u32 token_index);
        static TokenOrigin encode_origin(u32 chunk_index, u32 tiny_token_index);
        static u32 encode_import_token_index(u32 file_chunk_index, u32 tiny_token_index);
        static void decode_origin(TokenOrigin origin, u32* chunk_index, u32* tiny_token_index);
        static void decode_import_token_index(u32 token_index_into_file, u32* file_chunk_index, u32* tiny_token_index);
    private:
        // TODO: Bucket array
        BucketArray<Import> imports{256};
        BucketArray<Chunk> chunks{256};
        
        engone::Mutex lock_imports;
        engone::Mutex lock_chunks;

        // Extra data will be appended to the token. If the token had data previously then
        // the data will be appended to the previous data possibly changing the data offset
        // if space isn't available.
        u32 modifyTokenData(Token token, void* ptr, u32 size, bool with_null_termination = false);
        TokenInfo* getTokenInfo_unsafe(Token token);

    };
}