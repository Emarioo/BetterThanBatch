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
        TOKEN_EOF = 256, TOKEN_TYPE_BEGIN = TOKEN_EOF,

        TOKEN_IDENTIFIER,
        TOKEN_ANNOTATION,
        TOKEN_LITERAL_STRING,
        TOKEN_LITERAL_INTEGER,
        TOKEN_LITERAL_DECIMAL,
        TOKEN_LITERAL_HEXIDECIMAL,
        TOKEN_LITERAL_BINARY,
        TOKEN_LITERAL_OCTAL,

        TOKEN_NULL, TOKEN_KEYWORD_BEGIN = TOKEN_NULL,
        TOKEN_TRUE,
        TOKEN_FALSE,

        TOKEN_SIZEOF,
        TOKEN_NAMEOF,
        TOKEN_TYPEID,

        TOKEN_IF, TOKEN_KEYWORD_FLOW_BEGIN = TOKEN_IF, // Parser can quickly check if token is a flow type keyword
        TOKEN_ELSE,
        TOKEN_WHILE,
        TOKEN_FOR,
        TOKEN_SWITCH,
        TOKEN_DEFER,
        TOKEN_RETURN,
        TOKEN_BREAK,
        TOKEN_CONTINUE, TOKEN_KEYWORD_FLOW_END = TOKEN_CONTINUE, // inclusive

        TOKEN_USING,
        TOKEN_STRUCT,
        TOKEN_FUNCTION,
        TOKEN_OPERATOR,
        TOKEN_ENUM,
        TOKEN_NAMESPACE,
        TOKEN_UNION,
        TOKEN_ASM,
        TOKEN_TEST, 
        TOKEN_TRY,
        TOKEN_CATCH,
        TOKEN_FINALLY,
        TOKEN_KEYWORD_END = TOKEN_FINALLY, // inclusive

        TOKEN_NAMESPACE_DELIM,

        TOKEN_TYPE_END,
    };
    #define TOK_KEYWORD_NAME(TYPE) ((TYPE) < lexer::TOKEN_KEYWORD_BEGIN ? nullptr : lexer::token_type_names[TYPE - lexer::TOKEN_KEYWORD_BEGIN])
    #define TOKEN_IS_KEYWORD(T) (T >= lexer::TOKEN_KEYWORD_BEGIN && T <= lexer::TOKEN_KEYWORD_END)
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
            if(aux_data) {
                engone::Allocator* allocator = engone::GlobalHeapAllocator();
                // TRACK_DELS(u8, aux_max);
                allocator->allocate(0, aux_data, aux_max, HERE_T(u8));
                aux_data = nullptr;
                aux_max = 0;
                aux_used = 0;
            }
        }
        u32 import_id;
        u32 chunk_index; // handy when you only have a chunk pointer
        QuickArray<TokenInfo> tokens; // tokenize function accesses ptr,len,max manually for optimizations, constructor destructor functionality of a DynamicArray would be ignored.
        QuickArray<TokenSource> sources;
        u8* aux_data=nullptr; // auxiliary
        u32 aux_used=0;
        u32 aux_max=0;
        #define AUX_SIZE_OF_LEN_PREFIX 2
        #define AUX_LEN_MAX 0xFFFF
        
        char* get_data(int data_offset) {
            return (char*)aux_data + data_offset + AUX_SIZE_OF_LEN_PREFIX;
        }
        StringView get_string(int data_offset) {
            return { get_data(data_offset), get_len(data_offset) };
        }
        u16 get_len(int data_offset) {
            return *(u16*)((char*)aux_data + data_offset);
        }
        void set_len(int data_offset, u16 val) {
            *(u16*)((char*)aux_data + data_offset)  = val;
        }

        void alloc_aux_space(u32 n) {
            if(aux_used + n > aux_max) {
                engone::Allocator* allocator = engone::GlobalHeapAllocator();
                u32 new_max = 0x1000 + aux_max*1.5 + n;
                // TRACK_ADDS(u8, new_max - aux_max);
                u8* new_ptr = (u8*)allocator->allocate(new_max, aux_data, aux_max, HERE_T(u8));
                Assert(new_ptr);
                aux_data = new_ptr;
                aux_max = new_max;
            }
            aux_used+=n;
        }
    };
    struct Import {
        u32 file_id; // 0 is null, 1 is the first index
        std::string path;
        DynamicArray<u32> chunk_indices;
        DynamicArray<Chunk*> chunks; // chunk pointers are stored here so we don't have to lock the chunks bucket array everytime we need a chunk.

        Token geteof();

        int getTokenCount() {
            if(chunks.size() == 0)
                return -1;
            return (chunks.size()-1) * TOKEN_ORIGIN_TOKEN_MAX + chunks.last()->tokens.size();
        }
        
        // Token getToken(u32 token_index_into_import);

        // interesting but not stricly necessary information
        double last_modified_time;
        int fileSize;
        int lines;
        int comment_lines;
        int blank_lines;
    };
    // static const int yo = sizeof (Import);
    /*
        The lexer class is responsible for managing memory and extra information about all tokens.
        
        Should be thread safe as long as you don't read from imports that are being
        lexed. After imports/chunks have been lexed you can read as many tokens as you want.

        TODO: This class is awfully complicated. Measure current performance of
          lexer, then try to optimize while simplifiying things.
    */
    struct Lexer {
        void cleanup() {
            imports.cleanup();
            _chunks.cleanup();
            for(auto f : virtual_files) {
                f->~VirtualFile();
                TRACK_FREE(f,VirtualFile);
            }
            virtual_files.cleanup();
        }

        // returns file id, 0 means failure
        u32 tokenize(const char* text, u64 length, const std::string& path_name, u32 existing_import_id = 0, int line = 0, int column = 0);
        u32 tokenize(const std::string& text, const std::string& path_name, u32 existing_import_id = 0, int line = 0, int column = 0);
        u32 tokenize(const std::string& path, u32 existing_import_id = 0);

        struct FeedIterator {
            u32 file_id=0;
            u32 file_token_index=0;
            u32 end_file_token_index=0; // exclusive
            
            void clear() {
                buffer.resize(0);
            }
            void append(char chr) {
                buffer += chr;
            }
            void append(const char* ptr, int size) {
                buffer.append(ptr, size);
            }
            char* data() const {
                return (char*)buffer.data();
            }
            u32 len() const {
                return buffer.size();
            }
        private:
            // This is private because we may want to change it to something faster later.
            std::string buffer{};
        };
        FeedIterator createFeedIterator(Token start_token, Token end_token = {});
        FeedIterator createFeedIterator(Import* imp, u32 start, u32 end);
        bool feed(FeedIterator& iterator, bool skipSuffix = false, bool apply_indent = false, bool no_quotes = false);

        void print(u32 fileid);

        TokenInfo* getTokenInfoFromImport(u32 fileid, u32 token_index_into_import, TokenSource** src);
        Token getTokenFromImport(u32 fileid, u32 token_index_into_import, StringView* string = nullptr);
        // same as getDataFromToken but char instead of void
        u32 getStringFromToken(Token tok, const char** ptr);
        u32 getDataFromToken(Token tok, const void** ptr);

        void popTokenFromImport(Import* imp);
        void popMultipleTokensFromImport(Import* imp, int index_of_last_token_to_pop);
        std::string getStdStringFromToken(Token tok);

        std::string getline(SourceLocation location);

        static const int NO_SUFFIX = 1;
        static const int SIGNED_SUFFIX = 1;
        static const int UNSIGNED_SUFFIX = 2;
        // signedness_suffix, 0 = no suffix, 1 = signed, 2 = unsigned
        bool isIntegerLiteral(Token token, i64* value = nullptr, int* significant_digits = nullptr, int* signedness_suffix = nullptr);

        std::string tostring(Token token, bool no_quotes = false);
        std::string tostring(SourceLocation token) { return tostring(token.tok); }

        u32 createImport(const std::string& path, Import** out_import);
        // unsafe if you have an import_id reference hanging somewhere
        void destroyImport_unsafe(u32 import_id);
        
        // void appendToken(Import* imp, TokenInfo* token, StringView* string);
        Token appendToken(Import* imp, Token token, bool compute_source = false, StringView* string = nullptr, int inherited_column = 0, int backup_line = 0, int backup_column = 0);
        // Token appendToken(Import* imp, TokenType type, u32 flags, u32 line, u32 column);
        Token appendToken_auto_source(Import* imp, TokenType type, u32 flags);
        void appendToken(Import* imp, Token tok, StringView* string);
        
        bool equals_identifier(Token token, const char* str);
        
        const BucketArray<Import>& getImports() { return imports; }
        // const DynamicArray<Import*>& getImports() { return _imports; }
        
        bool get_source_information(SourceLocation loc, std::string* path, int* line, int* column, Import** out_imp = nullptr);
        std::string get_source_information_string(SourceLocation loc);

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

        Import* getImportFromTokenPointer(TokenInfo* info, int* out_token_index);
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
        }
        
        MUTEX(lock_imports);
        MUTEX(lock_chunks);

        // Extra data will be appended to the token. If the token had data previously then
        // the data will be appended to the previous data possibly changing the data offset
        // if space isn't available.
        u32 modifyTokenData(Token token, void* ptr, u32 size, bool with_null_termination = false);

    };
    
    double ConvertDecimal(const StringView& view, bool* double_suffix = nullptr);
    int ConvertInteger(const StringView& view);
    // 0x is optional. Asserts if first character is minus
    u64 ConvertHexadecimal(const StringView& view);
}