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

namespace lexer {

    enum TokenFlags : u16 {
        TOKEN_FLAG_NEWLINE          = 1,
        TOKEN_FLAG_SPACE            = 2,
        TOKEN_FLAG_SINGLE_QUOTED    = 4,
        TOKEN_FLAG_DOUBLE_QUOTED    = 8,
        TOKEN_FLAG_HAS_DATA         = 16,
    };
    enum TokenType : u16 {
        TOKEN_NONE = 0,
        // 0-255, ascii
        TOKEN_TYPE_BEGIN = 256,

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
    typedef u32 TokenOrigin; // change to u64 when changing chunk/token bits
    // struct TokenOrigin {
    //     u32 data[(TOKEN_ORIGIN_CHUNK_BITS + TOKEN_ORIGIN_TOKEN_BITS) / 32
    //         + ((TOKEN_ORIGIN_CHUNK_BITS + TOKEN_ORIGIN_TOKEN_BITS) % 32 == 0 ? 0 : 1)];
    // };
    struct Token {
        TokenType type=TOKEN_NONE;
        u16 flags=0;
        TokenOrigin origin={}; // decode to get chunk and token index
    };
    // struct TokenRange {
    //     // u16 chunk_id;
    //     u16 token_id_start;
    //     u16 token_id_end;
    //     // flags?
    //     // TokenChunk* chunk;
    // };

    /*
        The lexer class is responsible for managing memory and extra information about all tokens.
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
            u32 file_id;
            DynamicArray<TokenInfo> tokens;
            DynamicArray<u8> auxiliary_data;
        };
        struct File {
            std::string path;
            DynamicArray<u32> chunk_indices;

            // interesting but not stricly necessary information
            int fileSize;
            int lines; // non blank/comment lines
            int comment_lines;
            int blank_lines;
        };

        // returns file id, 0 means failure
        u32 tokenize(char* text, u64 length, const std::string& path_name);
        u32 tokenize(const std::string& path);

        struct FeedIterator {
            u32 file_id=0;

            u32 char_index=0;
            u32 file_token_index=0;
            
            u32 end_file_token_index=0; // exclusive
        };
        FeedIterator createFeedIterator(Token start_token, Token end_token = {});
        u32 feed(char* buffer, u32 buffer_max, FeedIterator& iterator);

        void print(u32 fileid);

        Token getTokenFromFile(u32 fileid, u32 token_index_into_file);
        u32 getStringFromToken(Token tok, char** ptr);
        u32 getDataFromToken(Token tok, void** ptr);

        // struct Iterator {

        // };
        // bool iterate(Iterator& iter);

    private:
        // TODO: Bucket array
        BucketArray<File> files{256};
        BucketArray<Chunk> chunks{256};

        u16 createFile(File** out_file);
        Token appendToken(u16 fileId, TokenType type, u32 flags, u32 line, u32 column);
        // Extra data will be appended to the token. If the token had data previously then
        // the data will be appended to the previous data possibly changing the data offset
        // if space isn't available.
        u32 modifyTokenData(Token token, void* ptr, u32 size, bool with_null_termination = false);
        TokenInfo* getTokenInfo_unsafe(Token token);

        // handy functions, the implementation details of tokens per chunk, chunk index and token index may change in the future
        // so we hide it behine encode and decode functions.
        // u32 encode_origin(u32 token_index);
        TokenOrigin encode_origin(u32 chunk_index, u32 tiny_token_index);
        u32 encode_file_token_index(u32 file_chunk_index, u32 tiny_token_index);
        void decode_origin(TokenOrigin origin, u32* chunk_index, u32* tiny_token_index);
        void decode_file_token_index(u32 token_index_into_file, u32* file_chunk_index, u32* tiny_token_index);
    };
}