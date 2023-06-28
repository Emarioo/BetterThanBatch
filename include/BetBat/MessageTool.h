#pragma once

#define ERR_HEADER_COLOR engone::log::BLOOD
#define WARN_HEADER_COLOR engone::log::YELLOW
// #define INFO_HEADER_COLOR engone::log::LIME
#define MESSAGE_COLOR engone::log::SILVER

#define MSG_LOCATION(F,L,C) TrimDir(F)<<":"<<(L)<<":"<<(C)
#define MSG_TYPE(N,T) "("<<N<<", "<<T<<")"
//  engone::log::out.setIndent(2);
#ifdef LOG_MSG_LOCATION
#define MSG_CODE_LOCATION engone::log::GRAY << __FILE__ << ":"<< __LINE__ << "\n" <<
#else
#define MSG_CODE_LOCATION 
#endif
#define MSG_CUSTOM(F,L,C,NAME,NUM) MSG_LOCATION(F,L,C) << " "<<MSG_TYPE(NAME,NUM) << ": "<<MESSAGE_COLOR
#define ERR_CUSTOM(F,L,C,NAME,NUM) MSG_CODE_LOCATION ERR_HEADER_COLOR <<MSG_CUSTOM(F,L,C,NAME,NUM)
#define WARN_CUSTOM(F,L,C,NAME,NUM) MSG_CODE_LOCATION WARN_HEADER_COLOR <<MSG_CUSTOM(F,L,C,NAME,NUM)

#define MSG_END ; engone::log::out.setIndent(0);

#define ERR_DEFAULT_T(TSTREAM,T,NAME,NUM) ERR_CUSTOM(TSTREAM?TSTREAM->streamName:"", T.line, T.column,NAME,NUM)
#define ERR_DEFAULT_R(R,NAME,NUM) ERR_CUSTOM(R.tokenStream?R.tokenStream->streamName:"", R.firstToken.line, R.firstToken.column,NAME,NUM)
#define ERR_DEFAULT_RL(R,NAME,NUM) ERR_CUSTOM(R.tokenStream?R.tokenStream->streamName:"", R.firstToken.line, R.firstToken.column + R.firstToken.length,NAME,NUM)

#define WARN_DEFAULT_R(R,NAME,NUM) WARN_CUSTOM(R.tokenStream?R.tokenStream->streamName:"", R.firstToken.line, R.firstToken.column,NAME,NUM)

struct TokenRange;
void PrintCode(TokenRange* tokenRange, const char* message = nullptr);
struct TokenStream;
void PrintCode(int index, TokenStream* stream, const char* message = nullptr);