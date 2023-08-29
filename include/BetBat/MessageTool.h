#pragma once

#include "BetBat/Util/StringBuilder.h"

struct TokenRange;
struct TokenStream;

/*
    Settings
    Should be handled by user profiles
*/
#define ERR_HEADER_COLOR engone::log::BLOOD
#define WARN_HEADER_COLOR engone::log::YELLOW
// #define INFO_HEADER_COLOR engone::log::LIME
#define MESSAGE_COLOR engone::log::SILVER

/*
    Latest code for message handling
*/
#ifdef LOG_MSG_LOCATION
#define MSG_CODE_LOCATION2 engone::log::GRAY << __FILE__ << ":"<< __LINE__ << "\n" <<
#define MSG_CODE_LOCATION engone::log::out << engone::log::GRAY << __FILE__ << ":"<< __LINE__ << "\n"
#else
#define MSG_CODE_LOCATION2
#define MSG_CODE_LOCATION
#endif
#define BASE_SECTION(CODE) MSG_CODE_LOCATION; info.errors++; TokenStream* prevStream = nullptr; StringBuilder err_type{}; err_type += CODE

#define ERR_TYPE(STR) err_type = StringBuilder{} + STR;
// #define ERR_HEAD(TR) PrintHead(ERR_HEADER_COLOR, TR, err_type, &prevStream);
#define ERR_HEAD(TR,...) PrintHead(ERR_HEADER_COLOR, TR, err_type, &prevStream); info.compileInfo->compileOptions->compileStats.errorTypes.add({__VA_ARGS__});
#define ERR_MSG(STR) engone::log::out << (StringBuilder{} + STR) << "\n";
#define ERR_MSG_LOG(STR) engone::log::out << STR ;
// #define ERR_MSG(STR) log::out << (StringBuilder{} + STR) << "\n\n";
#define ERR_LINE(TR, STR) PrintCode(TR, StringBuilder{} + STR, &prevStream);
#define ERR_EXAMPLE_TINY(STR) engone::log::out << engone::log::LIME << "Example: " << MESSAGE_COLOR << (StringBuilder{} + STR)<<"\n";
#define ERR_EXAMPLE(LN, STR) engone::log::out << engone::log::LIME << "Example:"; PrintExample(LN, StringBuilder{} + STR);

#define WARN_LINE(TR, STR) PrintCode(TR, StringBuilder{} + STR)

// #define ERR_BUILDER(TR, CODE, MSG) PrintHead(ERR_HEADER_COLOR, TR, CODE, MSG);
// #define WARN_BUILDER(TR, CODE, MSG) PrintHead(WARN_HEADER_COLOR, TR, CODE, MSG);

// TODO: Color support in string builder?

void PrintHead(engone::log::Color color, const TokenRange& tokenRange, const StringBuilder& errorCode , TokenStream** prevStream = nullptr);
void PrintHead(engone::log::Color color, const Token& token, const StringBuilder& errorCode, TokenStream** prevStream = nullptr);

void PrintCode(const TokenRange& tokenRange, const StringBuilder& stringBuilder, TokenStream** prevStream = nullptr);
void PrintCode(const Token& token, const StringBuilder& stringBuilder, TokenStream** prevStream = nullptr);

void PrintExample(int line, const StringBuilder& stringBuilder);

/*
    Old messaging code
    Should be deprecated when possible
*/
#define MSG_LOCATION(F,L,C) TrimDir(F)<<":"<<(L)<<":"<<(C)
#define MSG_TYPE(N,T) "("<<N<<", "<<T<<")"
//  engone::log::out.setIndent(2);
#define MSG_CUSTOM(F,L,C,NAME,NUM) MSG_LOCATION(F,L,C) << " "<<MSG_TYPE(NAME,NUM) << ": "<<MESSAGE_COLOR
#define ERR_CUSTOM(F,L,C,NAME,NUM) MSG_CODE_LOCATION2 ERR_HEADER_COLOR <<MSG_CUSTOM(F,L,C,NAME,NUM)
#define WARN_CUSTOM(F,L,C,NAME,NUM) MSG_CODE_LOCATION2 WARN_HEADER_COLOR <<MSG_CUSTOM(F,L,C,NAME,NUM)

#define MSG_END ; engone::log::out.setIndent(0);

#define ERR_DEFAULT_T(TSTREAM,T,NAME,NUM) ERR_CUSTOM(TSTREAM?TSTREAM->streamName:"", T.line, T.column,NAME,NUM)
#define ERR_DEFAULT_R(R,NAME,NUM) ERR_CUSTOM(R.tokenStream()?R.tokenStream()->streamName:"", R.firstToken.line, R.firstToken.column,NAME,NUM)
#define ERR_DEFAULT_RL(R,NAME,NUM) ERR_CUSTOM(R.tokenStream()?R.tokenStream()->streamName:"", R.firstToken.line, R.firstToken.column + R.firstToken.length,NAME,NUM)

#define WARN_DEFAULT_R(R,NAME,NUM) WARN_CUSTOM(R.tokenStream()?R.tokenStream()->streamName:"", R.firstToken.line, R.firstToken.column,NAME,NUM)

enum CompileError : u32 {

};

void PrintCode(const TokenRange& tokenRange, const char* message = nullptr);
void PrintCode(int index, TokenStream* stream, const char* message = nullptr);

