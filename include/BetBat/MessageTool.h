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
#define MSG_CODE_LOCATION engone::log::GRAY << __FILE__ << ":"<< __LINE__ << "\n"
#else
#define MSG_CODE_LOCATION 
#endif
#define BASE_SECTION(CODE) engone::log::out << MSG_CODE_LOCATION; info.errors++; StringBuilder err_type{}; err_type += CODE

#define ERR_TYPE(STR) err_type = StringBuilder{} + STR;
#define ERR_HEAD(TR) PrintHead(ERR_HEADER_COLOR, TR, err_type);
#define ERR_MSG(STR) log::out << (StringBuilder{} + STR) << "\n\n";
#define ERR_LINE(TR, STR) PrintCode(TR, StringBuilder{} + STR);
#define ERR_EXAMPLE_TINY(STR) log::out << log::LIME << "Example: " << MESSAGE_COLOR << (StringBuilder{} + STR)<<"\n";
#define ERR_EXAMPLE(LN, STR) log::out << log::LIME << "Example:"; PrintExample(LN, StringBuilder{} + STR);

#define WARN_LINE(TR, STR) PrintCode(TR, StringBuilder{} + STR)

// #define ERR_BUILDER(TR, CODE, MSG) PrintHead(ERR_HEADER_COLOR, TR, CODE, MSG);
// #define WARN_BUILDER(TR, CODE, MSG) PrintHead(WARN_HEADER_COLOR, TR, CODE, MSG);

// TODO: Color support in string builder?

void PrintHead(engone::log::Color color, const TokenRange& tokenRange, const StringBuilder& errorCode);
// , const StringBuilder& stringBuilder);
void PrintHead(engone::log::Color color, const Token& token, const StringBuilder& errorCode);
// , const StringBuilder& stringBuilder);

void PrintCode(const TokenRange& tokenRange, const StringBuilder& stringBuilder);
void PrintCode(const Token& token, const StringBuilder& stringBuilder);

void PrintExample(int line, const StringBuilder& stringBuilder);

/*
    Old messaging code
    Should be deprecated when possible
*/
#define MSG_LOCATION(F,L,C) TrimDir(F)<<":"<<(L)<<":"<<(C)
#define MSG_TYPE(N,T) "("<<N<<", "<<T<<")"
//  engone::log::out.setIndent(2);
#define MSG_CUSTOM(F,L,C,NAME,NUM) MSG_LOCATION(F,L,C) << " "<<MSG_TYPE(NAME,NUM) << ": "<<MESSAGE_COLOR
#define ERR_CUSTOM(F,L,C,NAME,NUM) MSG_CODE_LOCATION << ERR_HEADER_COLOR <<MSG_CUSTOM(F,L,C,NAME,NUM)
#define WARN_CUSTOM(F,L,C,NAME,NUM) MSG_CODE_LOCATION << WARN_HEADER_COLOR <<MSG_CUSTOM(F,L,C,NAME,NUM)

#define MSG_END ; engone::log::out.setIndent(0);

#define ERR_DEFAULT_T(TSTREAM,T,NAME,NUM) ERR_CUSTOM(TSTREAM?TSTREAM->streamName:"", T.line, T.column,NAME,NUM)
#define ERR_DEFAULT_R(R,NAME,NUM) ERR_CUSTOM(R.tokenStream()?R.tokenStream()->streamName:"", R.firstToken.line, R.firstToken.column,NAME,NUM)
#define ERR_DEFAULT_RL(R,NAME,NUM) ERR_CUSTOM(R.tokenStream()?R.tokenStream()->streamName:"", R.firstToken.line, R.firstToken.column + R.firstToken.length,NAME,NUM)

#define WARN_DEFAULT_R(R,NAME,NUM) WARN_CUSTOM(R.tokenStream()?R.tokenStream()->streamName:"", R.firstToken.line, R.firstToken.column,NAME,NUM)

void PrintCode(const TokenRange& tokenRange, const char* message = nullptr);
void PrintCode(int index, TokenStream* stream, const char* message = nullptr);

