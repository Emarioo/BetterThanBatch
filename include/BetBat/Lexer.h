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
#include "BetBat/Util/StringBuilder.h"

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

        TOKEN_EOF = TOKEN_TYPE_BEGIN,
        TOKEN_IDENTIFIER,
        TOKEN_ANNOTATION,
        TOKEN_LITERAL_STRING,
        TOKEN_LITERAL_INTEGER,
        TOKEN_LITERAL_DECIMAL,
        TOKEN_LITERAL_HEXIDECIMAL,
        TOKEN_LITERAL_BINARY,

        TOKEN_KEYWORD_BEGIN,

        TOKEN_NULL = TOKEN_KEYWORD_BEGIN,
        TOKEN_TRUE,
        TOKEN_FALSE,

        TOKEN_SIZEOF,
        TOKEN_NAMEOF,
        TOKEN_TYPEID,

        TOKEN_KEYWORD_FLOW_BEGIN, // Parser can quickly check if token is a flow type keyword
        TOKEN_IF = TOKEN_KEYWORD_FLOW_BEGIN,
        TOKEN_ELSE,
        TOKEN_WHILE,
        TOKEN_FOR,
        TOKEN_SWITCH,
        TOKEN_DEFER,
        TOKEN_RETURN,
        TOKEN_BREAK,
        TOKEN_CONTINUE,
        TOKEN_KEYWORD_FLOW_END = TOKEN_CONTINUE, // inclusive

        TOKEN_USING,
        TOKEN_STRUCT,
        TOKEN_FUNCTION,
        TOKEN_OPERATOR,
        TOKEN_ENUM,
        TOKEN_NAMESPACE,
        TOKEN_UNION,
        TOKEN_ASM,
        TOKEN_TEST,

        TOKEN_NAMESPACE_DELIM,

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
        // Token(TokenType type, u16 flags, TokenOrigin origin) : type(type), flags(flags), origin(origin) {}
        union {
            TokenType type=TOKEN_NONE;
            char c_type; // debug purpose
        };
        u16 flags=0;
        TokenOrigin origin={}; // decode to get chunk and token index
        #ifdef LEXER_DEBUG_DETAILS
        const char* s = nullptr;
        #endif
    };
    struct TokenRange {
        u32 importId;
        u32 token_index_start;
        u32 token_index_end;
    };
    struct SourceLocation {
        lexer::Token tok; // source location may not just be a token in the future
        // TokenOrigin origin;
        // u32 import_id;
        // u16 line;
        // u16 column;
    };

    struct TokenInfo {
        union {
            TokenType type;
            char c_type; // debug purpose
        };
        u16 flags;
        u32 data_offset; // can we move this elsewhere? hashmap with the token index? We use data_offset quite often (StringView) so it might slow us done?
        #ifdef LEXER_DEBUG_DETAILS
        const char* s;
        #endif
        // TODO: Optimize by moving line and column out of TokenInfo to fit more tokens into cache lines when we parse/read tokens. However, the lexer and preprocessor would need to write two distant memory locations per token which would be slower. Perhaps that is worth since write is generally faster than read?
        // u16 line;
        // u16 column;
    };
    struct TokenSource {
        u16 line;
        u16 column;
    };
    struct Chunk {
        ~Chunk() {
            engone::Allocator* allocator = engone::GlobalHeapAllocator();
            allocator->allocate(0, aux_data, aux_max);
        }
        u32 import_id;
        u32 chunk_index; // handy when you only have a chunk pointer
        QuickArray<TokenInfo> tokens; // tokenize function accesses ptr,len,max manually for optimizations, constructor destructor functionality of a DynamicArray would be ignored.
        QuickArray<TokenSource> sources;
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

        Token geteof();
        
        // Token getToken(u32 token_index_into_import);

        // interesting but not stricly necessary information
        int fileSize;
        int lines; // non blank/comment lines
        int comment_lines;
        int blank_lines;
    };
    /*
        The lexer class is responsible for managing memory and extra information about all tokens.
        
        Should be thread safe as long as you don't read from imports that are being
        lexed. After imports/chunks have been lexed you can read as many tokens as you want.
    */
    struct Lexer {

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
        FeedIterator createFeedIterator(Import* imp, u32 start, u32 end);
        u32 feed(char* buffer, u32 buffer_max, FeedIterator& iterator, bool skipSuffix = false);

        void print(u32 fileid);

        TokenInfo* getTokenInfoFromImport(u32 fileid, u32 token_index_into_import, TokenSource** src);
        Token getTokenFromImport(u32 fileid, u32 token_index_into_import, StringView* string = nullptr);
        // same as getDataFromToken but char instead of void
        u32 getStringFromToken(Token tok, const char** ptr);
        u32 getDataFromToken(Token tok, const void** ptr);
        
        std::string getStdStringFromToken(Token tok);

        std::string getline(SourceLocation location);

        std::string tostring(Token token);
        std::string tostring(SourceLocation token) { return tostring(token.tok); }

        u32 createImport(const std::string& path, Import** out_import);
        // unsafe if you have an import_id reference hanging somewhere
        void destroyImport_unsafe(u32 import_id);
        
        // void appendToken(Import* imp, TokenInfo* token, StringView* string);
        Token appendToken(Import* imp, Token token, bool compute_source = false, StringView* string = nullptr);
        // Token appendToken(Import* imp, TokenType type, u32 flags, u32 line, u32 column);
        Token appendToken_auto_source(Import* imp, TokenType type, u32 flags);
        void appendToken(Import* imp, Token tok, StringView* string);
        
        bool equals_identifier(Token token, const char* str);
        
        const BucketArray<Import>& getImports() { return imports; }
        // const DynamicArray<Import*>& getImports() { return _imports; }
        
        
        Import* getImport_unsafe(SourceLocation location) {
            u32 cindex, tindex;
            decode_origin(location.tok.origin, &cindex,&tindex);
            auto chunk = getChunk_unsafe(cindex);
            auto imp = getImport_unsafe(chunk->import_id);
            return imp;
        }
        Import* getImport_unsafe(u32 import_id) {
            lock_imports.lock();
            auto imp = imports.get(import_id-1);
            // auto imp = _imports.get(import_id-1);
            lock_imports.unlock();
            return imp;
        }
        Chunk* getChunk_unsafe(u32 chunk_index) {
            lock_chunks.lock();
            // auto imp = chunks.get(chunk_index);
            auto imp = _chunks.get(chunk_index);
            lock_chunks.unlock();
            return imp;
        }
        TokenInfo* getTokenInfo_unsafe(Token token) { return getTokenInfo_unsafe(token.origin); }
        TokenInfo* getTokenInfo_unsafe(TokenOrigin origin);
        TokenInfo* getTokenInfo_unsafe(SourceLocation location);
        TokenSource* getTokenSource_unsafe(SourceLocation location);
        
        struct VirtualFile {
            std::string virtual_path;
            StringBuilder builder;
        };
        bool createVirtualFile(const std::string& virtual_path, StringBuilder* builder);
        VirtualFile* findVirtualFile(const std::string& virtual_path);
        
        // handy functions, the implementation details of tokens per chunk, chunk index and token index may change in the future
        // so we hide it behine encode and decode functions.
        // u32 encode_origin(u32 token_index);
        static TokenOrigin encode_origin(u32 chunk_index, u32 tiny_token_index);
        static u32 encode_import_token_index(u32 file_chunk_index, u32 tiny_token_index);
        static void decode_origin(TokenOrigin origin, u32* chunk_index, u32* tiny_token_index);
        static void decode_import_token_index(u32 token_index_into_file, u32* file_chunk_index, u32* tiny_token_index);
    private:
        BucketArray<Import> imports{256};
        BucketArray<Chunk> _chunks{256};
        DynamicArray<VirtualFile*> virtual_files;

        // NOTE: I read/heard something about false thread cache sharing which could drop performance
        //  data in cache has to be shared between threads? I probably misunderstood BUT we shall
        //  heap allocate chunk and see if anything happens. No, I totally misunderstood or this is
        //  at least not a problem in slightest at this moment in time. -Emarioo, 2024-01-28

        // DynamicArray<Chunk*> _chunks;
        u32 addChunk(Chunk** out) {
            // Assert(lock_chunks.getOwner() != 0); // make sure we locked first
            return _chunks.add(nullptr, out);

            // Chunk* ptr = new Chunk(); // nocheckin
            // // lock_chunks.lock();
            // u32 index = _chunks.size();
            // _chunks.add(ptr);
            // // lock_chunks.unlock();
            // if(out)
            //     *out = ptr;
            // return index;
        }
        
        MUTEX(lock_imports);
        MUTEX(lock_chunks);

        // Extra data will be appended to the token. If the token had data previously then
        // the data will be appended to the previous data possibly changing the data offset
        // if space isn't available.
        u32 modifyTokenData(Token token, void* ptr, u32 size, bool with_null_termination = false);

    };
    
    double ConvertDecimal(const StringView& view);
    int ConvertInteger(const StringView& view);
    // 0x is optional. Asserts if first character is minus
    u64 ConvertHexadecimal(const StringView& view);
}