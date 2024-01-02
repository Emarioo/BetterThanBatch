#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/Typedefs.h"

#include "BetBat/Config.h"
#include "BetBat/MessageTool.h"
#include "BetBat/Util/Utility.h"
#include "Engone/Util/Array.h"
#include "BetBat/Util/Perf.h"
#include "BetBat/Util/Profiler.h"
#include "BetBat/Util/Tracker.h"

#define TOKEN_SUFFIX_LINE_FEED 0x1
// SPACE suffic is remove if LINE_FEED is present in mask/flag 
#define TOKEN_SUFFIX_SPACE 0x2
#define TOKEN_MASK_SUFFIX 0x3

#define TOKEN_DOUBLE_QUOTED 0x4
#define TOKEN_SINGLE_QUOTED 0x8
#define TOKEN_MASK_QUOTED 0x0C

#define TOKEN_ALPHANUM 0x10 // token has only characters for names like a-Z 0-9 _ NOTE THAT A QUOTED STRING IS NOT ALPHANUM, its quoted
#define TOKEN_NUMERIC 0x20 // token consists of numbers maybe hex or decimal

struct TextBuffer {
    std::string origin;
    char* buffer = nullptr;
    int size = 0;
    int startLine = 1;
    int startColumn = 1;
};

struct TokenRange;
struct TokenStream;
struct Token {
    Token() = default;
    Token(const char* str) : str((char*)str), length(strlen(str)) {};
    Token(const std::string& str) : str((char*)str.c_str()), length(str.length()) {};
    Token(char* str, int len) : str((char*)str), length(len) {};
    
    char* str=0; // NOT null terminated
    int length=0; // lengt of text data. Not including quotes or other things.
    int line=0;
    union {
        i16 flags=0; // bit mask representing line feed, space and quotation
        struct {
            bool _line_feed : 1;
            bool _space : 1;
            bool _dquote : 1;
            bool _squote : 1;
            bool _alphanum : 1;
            bool _numeric : 1;
        };
    };
    i16 column=0;
    int tokenIndex=0;
    TokenStream* tokenStream = nullptr; // don't keep this here

    // u8 type = 0;
    // u8 flags = 0;
    // u16 line = 0;
    // u16 column = 0;
    // u16 tokenStream = 0;
    // u32 tokenIndex = 0;

    // u32 extraData = 0; // points to extra data
    // str, length
    // integer, float

    int calcLength() const;

    bool operator==(const std::string& str) const;
    bool operator==(const Token& str) const;
    bool operator==(const char* str) const;
    bool operator!=(const char* str) const;

    operator TokenRange() const;
    TokenRange range() const;
    
    operator std::string() const;

    // prints the string, does not print suffix, line or column
    void print(bool skipSuffix = false) const;

    std::string getLine();
};

// #define INT_TO_VERSION(x) ()

#define LAYER_PREPROCESSOR 1
#define LAYER_PARSER 2
#define LAYER_GENERATOR 4
#define LAYER_OPTIMIZER 8
#define LAYER_INTERPRETER 16
engone::Logger& operator<<(engone::Logger& logger, Token& token);
struct TokenRange {
    Token firstToken{};

    // TokenRange& operator=(const TokenRange& r){
    //     r.firstToken = firstToken;
    //     r.endIndex = endIndex;
    //     // TokenRange range{};
    //     // range.firstToken = r.firstToken;
    //     // range.endIndex = r.endIndex;
    //     return r;
    // }
    int endIndex=0; // exclusive
    const int& startIndex() const { return firstToken.tokenIndex; }
    TokenStream* const& tokenStream() const { return firstToken.tokenStream; }
    // const int& startIndex = firstToken.tokenIndex;
    // TokenStream* const& tokenStream = firstToken.tokenStream;

    // TokenStream* stream() const {
    //     return firstToken.tokenStream;
    // }
    // const int& start() const {
    //     return firstToken.tokenIndex;
    // }
    // const int& end() const {
    //     return endIndex;
    // }

    void print(bool skipSuffix=true) const;
    void feed(std::string& outBuffer) const;
    // does not null terminate
    // token_index can be used to perform a feed operation on all the tokens using multiple feed function calls.
    // this is useful if you have a small buffer where you feed it a few tokens at a time.
    // Operation has finished if token_index == endIndex - startIndex(). Or just finishedFeed(token_index)
    u32 feed(char* outBuffer, u32 bufferSize, bool quoted_environment = false, int* token_index = nullptr) const;
    bool finishedFeed(int* token_index) const { return *token_index == endIndex - startIndex(); }
    u32 queryFeedSize(bool quoted_environment = false) const;
};
engone::Logger& operator<<(engone::Logger& logger, TokenRange& tokenRange);
struct TokenStream {
    ~TokenStream() {
        // TODO: cleanup does this too, if new allocations are done it needs
        // to be freed in two locations. careful with memory leaks.
        tokens.resize(0);
        tokenData.resize(0);
    }
    static TokenStream* Create();
    static void Destroy(TokenStream* stream);
    static TokenStream* Tokenize(const std::string& filePath);
    // nullptr is never returned, data can always be processed into tokens.
    // There may be errors in the stream though.
    // Allocations in optionalBase will be used if not null
    // static TokenStream* Tokenize(const char* text, u64 length, TokenStream* optionalBase=0);
    static TokenStream* Tokenize(TextBuffer* textBuffer, TokenStream* optionalBase=0);
    void cleanup(bool leaveAllocations=false);
    
    //-- extra info like @disable/enable and @version
    // TODO: version with major, minor and revision
    bool isVersion(const char* ver){return !strcmp(version,ver);}
    
    // token count
    int length() const;
    
    // The add functions return false if reallocation fails.

    // Only adds data not tokens
    bool addData(char chr);
    bool addData(const char* data);
    bool addData(Token token);
    char* addData_late(u32 size);
    
    // Only adds tokens not data
    bool addToken(Token token);

    // Adds both token and data
    // Side note: You may think the function name is verbose
    //  but it had ambiguous naming before and I paid the price.
    bool addTokenAndData(const char* token);
    // bool addTokenAndData(Token token);

    Token& get(u32 index) const;

    //-- For parsing

    // for these to work you would need an index that they are based on.
    // this index cannot exist in this struct (readHead for example) because
    // multiple threads wouldn't be able to use  these functions.
    // Token& now();
    // Token& next(); // move forward
    // int at();
    // bool end();
    // void finish();

    // TokenRange getLine(int index);
    
    //-- Extra

    // flags is a bitmask, TOKEN_PRINT_...
    void printTokens(int tokensPerLine = 14, bool showlncol=false);
    void printData(int charsPerLine = 40);
    
    // normal print of all tokens
    void print();
    // copies everything except tokens
    bool copyInfo(TokenStream& out);
    bool copy(TokenStream& out);

    void writeToFile(const std::string& path);
    
    TokenStream* copy();

    void finalizePointers();
    bool isFinialized(){
        if(tokens.used==0)
            return true;
        return (u64)((Token*)tokens.data)->str >= (u64)tokenData.data;
    }

    std::string streamName; // filename/importname SHOULD BE FULL PATH
    
    void addImport(const std::string& name, const std::string& as);
    struct Import {
        std::string name;
        std::string as="";
    };
    DynamicArray<Import> importList;
    int lines=0; // processed lines excluding whitespace and comments.
    int blankLines=0; // lines with only whitespace or comments.
    int commentCount=0; // multiline slash counts as one comment
    int readBytes=0;
    // new line in the middle of a token is not counted, multiline strings doesn't work very well
    int enabled=0;
    int64 _2; // offsset tracking
    static const int VERSION_MAX = 5;
    char version[VERSION_MAX+1]{0};

    // TODO: Replace with QuickArray? Tracker doesn't exist in Memory
    engone::Memory<Token> tokens{}; // the tokens themselves
    engone::Memory<char> tokenData{}; // the data the tokens refer to
    
    // int readHead=0; // doesn't work when using multiple threads
};
double ConvertDecimal(const Token& token);
bool IsInteger(const Token& token);
int ConvertInteger(const Token& token);
bool IsName(const Token& token);
bool IsAnnotation(const Token& token);
// Can also be an integer
bool IsDecimal(const Token& token);
bool IsHexadecimal(const Token& token);
u64 ConvertHexadecimal(const Token& token);
u64 ConvertHexadecimal_content(char* str, int length);
// does not include 0x at the beginning
std::string NumberToHex(u64 number, bool withPrefix = false);

bool Equal(const Token& token, const char* str);
bool StartsWith(const Token& token, const char* str);

// I would recommend testing on a large text for more accurate results.
void PerfTestTokenize(const engone::Memory<char>& textData, int times=1);
void PerfTestTokenize(const char* file, int times=1);