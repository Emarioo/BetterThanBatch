#pragma once

#include "Engone/Util/Stream.h"
#include "BetBat/Util/StringBuilder.h"
#include "BetBat/Lexer.h"
// #include "BetBat/__old_Tokenizer.h"

// struct TokenRange;
// struct TokenStream;

/*
    Settings
    Should be handled by user profiles
*/
#define ERR_HEADER_COLOR engone::log::BLOOD
#define WARN_HEADER_COLOR engone::log::YELLOW
// #define INFO_HEADER_COLOR engone::log::LIME
#define MESSAGE_COLOR engone::log::NO_COLOR

/*
    Latest code for message handling
*/
#ifdef LOG_MSG_LOCATION
#ifndef COMPILER_GNU
#define MSG_CODE_LOCATION2 engone::log::GRAY << __FILE__ << ":"<< __LINE__ << "\n" <<
#define MSG_CODE_LOCATION engone::log::out << engone::log::GRAY << __FILE__ << ":"<< __LINE__ << "\n"
#else
#define MSG_CODE_LOCATION2 engone::log::GRAY << "./"<< __FILE__ << ":"<< __LINE__ << "\n" <<
#define MSG_CODE_LOCATION engone::log::out << engone::log::GRAY <<"./"<< __FILE__ << ":"<< __LINE__ << "\n"
#endif
#else
#define MSG_CODE_LOCATION2
#define MSG_CODE_LOCATION
#endif

#define BASE_SECTION(CODE, CONTENT) { if(!info.ignoreErrors) info.errors++; int base_column = -1; StringBuilder err_type{}; err_type += CODE; if(info.compiler) info.compiler->reporter.start_report(); MSG_CODE_LOCATION; CONTENT; if(info.compiler) info.compiler->reporter.end_report(); }

#define BASE_SECTION2(CONTENT) if(info.showErrors) { if(!info.ignoreErrors) info.errors++; info.reporter->start_report(); MSG_CODE_LOCATION; CONTENT; info.reporter->end_report(); }

#ifndef COMPILER_GNU
#define ERR_HEAD2(T,...) info.reporter->err_head(T, __VA_ARGS__); info.compiler->addError(T, __VA_ARGS__);
#else
#define ERR_HEAD2(T, ...) info.reporter->err_head(T __VA_OPT__(,) __VA_ARGS__); info.compiler->addError(T __VA_OPT__(,) __VA_ARGS__);
#endif
#define ERR_LINE2(T, STR) info.reporter->err_mark(T, StringBuilder{} << STR); 

#define BASE_WARN_SECTION(CODE, CONTENT) { info.compiler->options->compileStats.warnings++; int base_column = -1; TokenStream* prevStream = nullptr; StringBuilder warn_type{}; warn_type += CODE; if(info.compiler) info.compiler->reporter.start_report(); MSG_CODE_LOCATION; CONTENT; if(info.compiler) info.compiler->reporter.end_report(); }

// #define ERR_TYPE(STR) err_type = StringBuilder{} + STR;
// #define ERR_HEAD(TR) PrintHead(ERR_HEADER_COLOR, TR, err_type, &prevStream);
#ifdef OS_WINDOWS
#define ERR_HEAD(TR,...) err_type += ToCompileErrorString({true, __VA_ARGS__}); PrintHead(ERR_HEADER_COLOR, TR, err_type, &prevStream); info.compiler->options->compileStats.addError(TR, __VA_ARGS__);
#else
#define ERR_HEAD(TR,...) err_type += ToCompileErrorString({true __VA_OPT__(,) __VA_ARGS__}); PrintHead(ERR_HEADER_COLOR, TR, err_type, &prevStream); info.compileInfo->options->compileStats.addError(TR __VA_OPT__(,) __VA_ARGS__);
#endif
#define ERR_MSG(STR) engone::log::out << (StringBuilder{} + STR) << "\n\n";
#define ERR_MSG_LOG(STR) engone::log::out << STR;
#define ERR_MSG_COLORED(STR) engone::log::out << STR << "\n\n";
// #define ERR_MSG(STR) log::out << (StringBuilder{} + STR) << "\n\n";
#define ERR_LINE(TR, STR) PrintCode(TR, StringBuilder{} + STR, &prevStream, &base_column);
#define ERR_EXAMPLE_TINY(STR) engone::log::out << engone::log::LIME << "Example: " << MESSAGE_COLOR << (StringBuilder{} + STR)<<"\n";
// #define ERR_EXAMPLE(LN, STR) engone::log::out << engone::log::LIME << "Example:"; PrintExample(LN, StringBuilder{} + STR);
#define ERR_EXAMPLE(LN, STR) PrintExample(LN, StringBuilder{} + STR);

#define WARN_HEAD(TR,...) warn_type+="W0"; PrintHead(WARN_HEADER_COLOR, TR, warn_type, &prevStream);

#define WARN_MSG(STR) engone::log::out << (StringBuilder{} + STR) << "\n";
#define WARN_LINE(TR, STR) PrintCode(TR, StringBuilder{} + STR, &prevStream, &base_column);

// #define ERR_BUILDER(TR, CODE, MSG) PrintHead(ERR_HEADER_COLOR, TR, CODE, MSG);
// #define WARN_BUILDER(TR, CODE, MSG) PrintHead(WARN_HEADER_COLOR, TR, CODE, MSG);

// TODO: Color support in string builder?

// void PrintHead(engone::log::Color color, const TokenRange& tokenRange, const StringBuilder& errorCode , TokenStream** prevStream = nullptr);
// void PrintHead(engone::log::Color color, const Token& token,           const StringBuilder& errorCode, TokenStream** prevStream = nullptr);

// void PrintCode(const TokenRange& tokenRange, const StringBuilder& stringBuilder, TokenStream** prevStream = nullptr, int* base_column = nullptr);
// void PrintCode(const Token& token,           const StringBuilder& stringBuilder, TokenStream** prevStream = nullptr, int* base_column = nullptr);
// void PrintCode(const TokenRange& tokenRange, const char* message,                TokenStream** prevStream);

void PrintExample(int line, const StringBuilder& stringBuilder);

// namespace lexer {
//     struct Token;
//     struct Lexer;
//     struct Import;
// }

/*
    Old messaging code
    Should be deprecated when possible
*/
#define MSG_LOCATION(F,L,C) TrimDir(F)<<":"<<(L)<<":"<<(C)
#define MSG_TYPE(N,T) "("<<N<<", "<<T<<")"
//  engone::log::out.setIndent(2);
#define MSG_CUSTOM(F,L,C,NAME,NUM) MSG_LOCATION(F,L,C) << " "<<MSG_TYPE(NAME,NUM) << ": "<<MESSAGE_COLOR
#define WARN_CUSTOM(F,L,C,NAME,NUM) MSG_CODE_LOCATION2 WARN_HEADER_COLOR <<MSG_CUSTOM(F,L,C,NAME,NUM)

#define WARN_DEFAULT_R(R,NAME,NUM) WARN_CUSTOM(R.tokenStream()?R.tokenStream()->streamName:"", R.firstToken.line, R.firstToken.column,NAME,NUM)

// IMPORTANT: DO NOT, UNDER ANY CIRCUMSTANCES, CHANGE THE ID/NUMBER OF THE ERRORS. THEY MAY BE USED IN TEST CASES.
enum CompileError : u32 {
    ERROR_NONE = 0,
    ERROR_UNSPECIFIED = 1, // This is used for old errors which didn't have error types. New errors should not use this. Or if you are lazy i guess?
    ERROR_CASTING_TYPES = 1001,
    ERROR_UNDECLARED = 1002,
    ERROR_TYPE_MISMATCH = 1003, // generic type mismatch
    ERROR_INVALID_TYPE = 1004, // generic invalid type, usually void being used when it can't
    ERROR_TOO_MANY_VARIABLES = 1005, // usually with more assignment variables than return values resulting in some variables missing a type
    
    ERROR_INCOMPLETE_FIELD = 1100, // usually when structs depend on each other recursively

    ERROR_AMBIGUOUS_PARSING = 1200,

    // ERROR_POSSIBLE_USER_MISTAKE = 1200,

    ERROR_DUPLICATE_CASE = 2101,
    ERROR_DUPLICATE_DEFAULT_CASE = 2102,
    ERROR_C_STYLED_DEFAULT_CASE = 2103,
    ERROR_BAD_TOKEN_IN_SWITCH = 2104,
    ERROR_MISSING_ENUM_MEMBERS_IN_SWITCH = 2105,
    ERROR_AMBIGUOUS_IF_ELSE = 2106, 
    
    ERROR_OVERLOAD_MISMATCH = 3001,
    
    ERROR_UNKNOWN = 99999,
};
struct temp_compile_error {
    bool shortVersion = false;
    CompileError err = ERROR_NONE;
};
CompileError ToCompileError(const char* str);
std::string ToCompileErrorString(temp_compile_error stuff);
// const char* ToCompileErrorString(temp_compile_error stuff);

// void PrintCode(const TokenRange& tokenRange, const char* message = nullptr);
// void PrintCode(int index, TokenStream* stream, const char* message = nullptr);

struct Reporter {
    lexer::Lexer* lexer=nullptr;

    bool instant_report = true;
    
    void start_report();
    void end_report();

    lexer::Import* prev_import=nullptr; // err_head resets this
    int base_column = -1; // err_head resets this too

    void err_head(lexer::Token token, CompileError errcode = ERROR_UNSPECIFIED);
    void err_head(lexer::SourceLocation loc, CompileError errcode = ERROR_UNSPECIFIED) {
        err_head(loc.tok,errcode);
    }
    // void err_desc(const StringBuilder& text);
    void err_mark(lexer::Token token, const StringBuilder& text);
    void err_mark(lexer::TokenRange range, const StringBuilder& text);
    void err_mark(lexer::SourceLocation loc, const StringBuilder& text) {
        err_mark(loc.tok, text);
    }


    ByteStream stream{engone::GlobalHeapAllocator()};
};