#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/Typedefs.h"

#include "BetBat/Util/Utility.h"
#include "BetBat/Util/Perf.h"
#include "BetBat/Config.h"

#define TOKEN_SUFFIX_LINE_FEED 0x1
// SPACE suffic is remove if LINE_FEED is present in mask/flag 
#define TOKEN_SUFFIX_SPACE 0x2
#define TOKEN_QUOTED 0x4
#define TOKEN_DOUBLE_QUOTED 0x8

struct Token {
    Token() = default;
    Token(const char* str) : str((char*)str), length(strlen(str)) {};
    Token(const std::string& str) : str((char*)str.c_str()), length(str.length()) {};
    
    char* str=0; // NOT null terminated
    int length=0;
    int flags=0; // bit mask representing line feed, space and quotation
    int line=0;
    int column=0;

    bool operator==(const std::string& str);
    bool operator==(const char* str);
    bool operator!=(const char* str);
    
    operator std::string();

    // prints the string, does not print suffix, line or column
    void print(int printFlags = 0);
};

// #define INT_TO_VERSION(x) ()

#define LAYER_PREPROCESSOR 1
#define LAYER_PARSER 2
#define LAYER_GENERATOR 4
#define LAYER_OPTIMIZER 8
#define LAYER_INTERPRETER 16
engone::Logger& operator<<(engone::Logger& logger, Token& token);
struct TokenStream;
struct TokenRange {
    Token firstToken{};
    int startIndex=0;
    int endIndex=0; // exclusive
    TokenStream* tokenStream=0; // should probably not be here

    void print();
    void feed(std::string& outBuffer);
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
    // May return nullptr if file could not be accessed.
    static TokenStream* Tokenize(const std::string& filePath);
    // nullptr is never returned, data can always be processed into tokens.
    // There may be errors in the stream though.
    // Allocations in optionalBase will be used if not null
    static TokenStream* Tokenize(const char* text, int length, TokenStream* optionalBase=0);
    void cleanup(bool leaveAllocations=false);
    
    //-- extra info like @disable/enable and @version
    // TODO: version with major, minor and revision
    bool isVersion(const char* ver){return !strcmp(version,ver);}
    
    // token count
    int length();
    
    // The add functions return false if reallocation fails.

    // Only adds data not tokens
    bool addData(char chr);
    bool addData(const char* data);
    bool addData(Token token);
    
    // Only adds tokens not data
    bool addToken(Token token);

    // Adds both token and data
    // Side note: You may think the function name is verbose
    //  but it had ambiguous naming before and I paid the price.
    bool addTokenAndData(const char* token);
    // bool addTokenAndData(Token token);

    Token& get(u32 index);

    //-- For parsing

    Token& now();
    Token& next(); // move forward
    int at();
    bool end();
    void finish();

    TokenRange getLine(int index);
    
    //-- Extra

    // flags is a bitmask, TOKEN_PRINT_...
    void printTokens(int tokensPerLine = 14, bool showlncol=false);
    void printData(int charsPerLine = 40);
    
    // normal print of all tokens
    void print();
    // copies everything except tokens
    bool copyInfo(TokenStream& out);
    bool copy(TokenStream& out);
    
    TokenStream* copy();

    void finalizePointers();

    std::string streamName; // filename/importname SHOULD BE FULL PATH
    
    void addImport(const std::string& name, const std::string& as);
    struct Import {
        std::string name;
        std::string as="";
    };
    std::vector<Import> importList;
    int lines=0; // total number of lines excluding whitespace and comments.
    int blankLines=0; // lines with only whitespace or comments.
    int commentCount=0; // multiline slash counts as one comment
    int readBytes=0;
    // new line in the middle of a token is not counted, multiline strings doesn't work very well
    int enabled=0;
    int64 _2; // offsset tracking
    static const int VERSION_MAX = 5;
    char version[VERSION_MAX+1]{0};

    engone::Memory tokens{sizeof(Token)}; // the tokens themselves
    engone::Memory tokenData{1}; // the data the tokens refer to
    
    int readHead=0;
};
double ConvertDecimal(Token& token);
bool IsInteger(Token& token);
int ConvertInteger(Token& token);
bool IsName(Token& token);
// Can also be an integer
bool IsDecimal(Token& token);
bool IsHexadecimal(Token& token);
int ConvertHexadecimal(Token& token);
bool Equal(Token& token, const char* str);

// I would recommend testing on a large text for more accurate results.
void PerfTestTokenize(const engone::Memory& textData, int times=1);
void PerfTestTokenize(const char* file, int times=1);