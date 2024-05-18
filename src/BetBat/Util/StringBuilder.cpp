#include "BetBat/Util/StringBuilder.h"

// #include "BetBat/__old_Tokenizer.h"

// void StringBuilder::append(const TokenRange& tokenRange) {
//     u32 length = tokenRange.queryFeedSize();
//     ensure(len + length);

//     u32 usedLength = tokenRange.feed(_ptr + len, max - len);
//     Assert(length == usedLength);

//     len += length;
//     *(_ptr + len) = '\0';
// }
// void StringBuilder::append(const Token& token) {
//     if(token.str && token.length > 0)
//         append(token.str, token.length);
// }

engone::Logger& operator<<(engone::Logger& logger, const StringBuilder& stringBuilder){
    if(stringBuilder.data() && stringBuilder.size() != 0)
        logger.print(stringBuilder.data(),stringBuilder.size());
    return logger;
}


bool operator==(const std::string& str, const StringView& view) {
    if(str.length() != view.len) return false;
    return view.equals(str.c_str());
}
bool operator==(const StringView& view, const std::string& str) {
    if(str.length() != view.len) return false;
    return view.equals(str.c_str());
}
bool operator==(const StringView& str, const StringView& view) {
    return str.equals(view.ptr,view.len);
}
bool operator==(const StringView& view, const char* str) {
    return view.equals(str);
}