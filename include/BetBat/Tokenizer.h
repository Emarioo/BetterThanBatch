#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/Typedefs.h"

#include "BetBat/Config.h"
#include "BetBat/MessageTool.h"
#include "BetBat/Util/Utility.h"
#include "BetBat/Util/Perf.h"

#define TOKEN_SUFFIX_LINE_FEED 0x1
// SPACE suffic is remove if LINE_FEED is present in mask/flag 
#define TOKEN_SUFFIX_SPACE 0x2
#define TOKEN_QUOTED 0x4
#define TOKEN_DOUBLE_QUOTED 0x8

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
    i16 flags=0; // bit mask representing line feed, space and quotation
    i16 column=0;
    int tokenIndex=0;
    TokenStream* tokenStream = nullptr; // don't keep this here

    int calcLength();

    bool operator==(const std::string& str);
    bool operator==(const Token& str);
    bool operator==(const char* str);
    bool operator!=(const char* str);

    operator TokenRange() const;
    TokenRange range() const;
    
    operator std::string() const;

    // prints the string, does not print suffix, line or column
    void print(bool skipSuffix = false);
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

    TokenRange operator=(const TokenRange& r){
        TokenRange range{};
        range.firstToken = r.firstToken;
        range.endIndex = r.endIndex;
        return range;
    }

    const int& startIndex = firstToken.tokenIndex;
    int endIndex=0; // exclusive
    TokenStream* const& tokenStream = firstToken.tokenStream;

    // TokenStream* stream() const {
    //     return firstToken.tokenStream;
    // }
    // const int& start() const {
    //     return firstToken.tokenIndex;
    // }
    // const int& end() const {
    //     return endIndex;
    // }

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
    int length() const;
    
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

    Token& get(u32 index) const;

    //-- For parsing

    Token& now();
    Token& next(); // move forward
    int at();
    bool end();
    void finish();

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
bool IsAnnotation(Token& token);
// Can also be an integer
bool IsDecimal(Token& token);
bool IsHexadecimal(Token& token);
int ConvertHexadecimal(Token& token);
bool Equal(Token& token, const char* str);

// I would recommend testing on a large text for more accurate results.
void PerfTestTokenize(const engone::Memory& textData, int times=1);
void PerfTestTokenize(const char* file, int times=1);