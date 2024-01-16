#include "BetBat/Lexer.h"

namespace lexer {
u32 Lexer::tokenize(const std::string& path){
    u64 fileSize = 0;
    auto file = engone::FileOpen(path, engone::FILE_READ_ONLY, &fileSize);
    if(!file) return 0;
    defer {
        if(file)
            engone::FileClose(file);
    };

    // TODO: Reuse buffers
    char* buffer = TRACK_ARRAY_ALLOC(char, fileSize);
    u64 size = fileSize;
    if(!buffer) return 0;

    defer {
        TRACK_ARRAY_FREE(buffer, char, size);
    };

    u64 readBytes = engone::FileRead(file, buffer, size);
    Assert(readBytes != -1);
    Assert(readBytes == fileSize);

    engone::FileClose(file); // close file as soon as possible so that other code can read or write to it if they want
    file = {};
        
    u32 file_id = tokenize(buffer,size,path);

    return file_id;
}
u32 Lexer::tokenize(char* text, u64 length, const std::string& path_name){
    using namespace engone;
    ZoneScopedC(tracy::Color::Honeydew);

    File* file=nullptr;
    u32 file_id = createFile(&file);

    const char* specials = "+-*/%=<>!&|~" "$@#{}()[]" ":;.,";
    int specialLength = strlen(specials);

    // TODO: Optimize by keeping traack of chunks ids and their pointers here instead of using
    //  appendToken which queries file and chunk id every time.

    auto update_flags = [&](u32 set, u32 clear = 0){
        if(file->chunk_indices.size()!=0) {
            Chunk* chunk = chunks.get(file->chunk_indices.last());
            chunk->tokens.last().flags = (chunk->tokens.last().flags&~clear) | set;
        }
    };

    Token token = {};
    int token_line = 0;
    int token_column = 0;
    int str_start = 0;
    int str_end = 0;

    u16 line=1; // TODO: TestSuite needs functionality to set line and column through function arguments.
    u16 column=1;
    
    bool inQuotes = false;
    bool isSingleQuotes = false;
    bool inComment = false;
    bool inEnclosedComment = false;
    
    bool isNumber = false;
    bool isAlpha = false; // alpha and then alphanumeric

    bool inHexidecimal = false;
    // int commentNestDepth = 0; // C doesn't have this and it's not very good. /*/**/*/
    
    bool canBeDot = false; // used to deal with decimals (eg. 255.92) as one token
    
    // preprocessor stuff
    bool foundHashtag=false;

    bool foundNonSpaceOnLine = false;

    u64 index=0;
    while(index<length) {
        char prevChr = 0;
        char nextChr = 0;
        char nextChr2 = 0;
        if(index>0)
            prevChr = text[index-1];
        if(index+1<length)
            nextChr = text[index+1];
        if(index+2<length)
            nextChr2 = text[index+2];
        char chr = text[index];
        index++;
        
        int col = column;
        int ln = line;
        if(chr=='\t')
            column+=4;
        else
            column++;
        if(chr == '\n'){
            line++;
            column=1;
        }
        
        if(chr=='\r'&&(nextChr=='\n'||prevChr=='\n')){
            continue;// skip \r and process \n next time
        }
        if(inComment){
            if(inEnclosedComment){
                if(chr=='\n'){
                    // Questionable decision to use lines counting code here
                    // AND else where too. A bug is bound to happen by how spread out it is.
                    // Bug counter: 1
                    if(!foundNonSpaceOnLine) {
                        file->blank_lines++;
                        // outStream->blankLines++;
                        foundNonSpaceOnLine = false;
                    } else
                        file->lines++;
                        // outStream->lines++;
                    update_flags(TOKEN_FLAG_NEWLINE);
                    // if(file->chunk_indices.size()!=0) {
                    // // if(outStream->length()!=0)
                    //     Chunk* chunk = chunks.get(file->chunk_indicesds.last()-1);
                    //     chunk->tokens.last().flags |= TOKEN_FLAG_NEWLINE;
                    //     // outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;   
                    // }
                }
                // @optimize TODO: move forward to characters at a time
                //   since */ has two characters. By jumping two characters
                //   we are faster and still guarrranteed to hit either * or /.
                //   Then do some checks to sync back to normal. This won't work.
                //   When counting lines since each character has to be checked.
                //   But you can also consider using a loop here which runs very litte and
                //   predictable code.

                if(chr=='*'&&nextChr=='/'){
                    index++;
                    inComment=false;
                    inEnclosedComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
            }else{
                // MEASURE;

                // #define OPTIMIZE_COMMENT
                // I thought this code would be faster but it seems
                // like it's slower.
                #ifdef OPTIMIZE_COMMENT
                if(chr=='\n'||index==length){
                    if(!foundNonSpaceOnLine)
                        outStream->blankLines++;
                    else
                        outStream->lines++;
                    if(outStream->length()!=0)
                        outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
                    inComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                } else {
                    index++;
                    while(true){
                        char chr = text[index];
                        // do it in batches?
                        // not worth because comments have
                        // to few characters?
                        // char ch1 = text[index];
                        // char ch2 = text[index];
                        // char ch3 = text[index];
                        if(chr=='\t')
                            column+=4;
                        else
                            column++;
                        index++;
                        if(chr=='\n'||index==length){
                            line++;
                            column=1;
                            if(!foundNonSpaceOnLine)
                                outStream->blankLines++;
                            else
                                outStream->lines++;
                            foundNonSpaceOnLine=false;
                            if(outStream->length()!=0)
                                outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;

                            if(index + 1 < length){
                                char chr = text[index];
                                char chr2 = text[index+1];
                                if(chr == '/' && chr2 == '/'){
                                    // log::out << "ha\n";
                                    index+=2;
                                    continue;
                                }
                            }
                            inComment=false;
                            _TLOG(log::out << "// : End comment\n";)
                            break;
                        }
                    }
                }
                #else
                if(chr=='\n'||index==length){
                    if(!foundNonSpaceOnLine)
                        file->blank_lines++;
                        // outStream->blankLines++;
                    else
                        file->lines++;
                        // outStream->lines++;
                    foundNonSpaceOnLine=false;
                    // if(outStream->length()!=0)
                    //     outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
                    update_flags(TOKEN_FLAG_NEWLINE);
                    inComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
                #endif
            }
            continue;
        } else if(inQuotes) {
            if((!isSingleQuotes && chr == '"') || 
                (isSingleQuotes && chr == '\'')){
                // Stop reading string token
                inQuotes=false;
                
                token.flags = 0;
                if(nextChr=='\n')
                    token.flags |= TOKEN_FLAG_NEWLINE;
                else if(nextChr==' ')
                    token.flags |= TOKEN_FLAG_SPACE;
                if(isSingleQuotes)
                    token.flags |= TOKEN_FLAG_SINGLE_QUOTED;
                else
                    token.flags |= TOKEN_FLAG_DOUBLE_QUOTED;
                
                // _TLOG(log::out << " : Add " << token << "\n";)
                _TLOG(if(str_start != str_end) log::out << "\n"; log::out << (isSingleQuotes ? '\'' : '"') <<" : End quote\n";)

                
                token.type = TOKEN_LITERAL_STRING;
                Token tok = appendToken(file_id, token.type, token.flags, token_line, token_column);
                modifyTokenData(tok, text + str_start, str_end - str_start, true);
                

                // outStream->addToken(token);

                token = {};
            } else {
                if(str_start != str_end){
                    _TLOG(log::out << "-";)
                }
                
                char tmp = chr;
                if(chr=='\\'){
                    index++;
                    if(nextChr=='n'){
                        tmp = '\n';
                        _TLOG(log::out << "\\n";)
                    } else if(nextChr=='\\'){
                        tmp = '\\';
                        _TLOG(log::out << "\\\\";)
                    } else if(!isSingleQuotes && nextChr=='"'){
                        tmp = '"';
                        _TLOG(log::out << "\"";)
                    } else if(isSingleQuotes && nextChr=='\''){
                        tmp = '\'';
                        _TLOG(log::out << "'";)
                    } else if(nextChr=='t'){
                        tmp = '\t';
                        _TLOG(log::out << "\\t";)
                    } else if(nextChr=='0'){
                        tmp = '\0';
                        _TLOG(log::out << "\\0";)
                    } else if(nextChr=='r'){
                        tmp = '\r';
                        _TLOG(log::out << "\\r";)
                    } else if(nextChr=='e'){
                        tmp = '\x1b'; // '\e' is not supported in some compilers
                        _TLOG(log::out << "\\e";)
                    } else if(nextChr=='x'){
                        char hex0 = text[index];
                        index++;
                        char hex1 = text[index];
                        index++;

                        // NOTE: Currently, \x3k is seen as \x03. Is this okay?
                        tmp = 0;
                        bool valid_hex = false;
                        if(hex0 >= '0' && hex0 <= '9') {
                            tmp = hex0 - '0';
                        } else if((hex0|32) >= 'a' && (hex0|32) <= 'f') {
                            tmp = (hex0|32) - 'a' + 10;
                        }
                        if(hex1 >= '0' && hex1 <= '9') {
                            tmp = (tmp << 4) + hex1 - '0';
                        } else if((hex1|32) >= 'a' && (hex1|32) <= 'f') {
                            tmp = (tmp << 4) + (hex1|32) - 'a' + 10;
                        }

                        // tmp = '\r';
                        _TLOG(log::out << "\\x" << hex0 << hex1;)
                    } else{
                        tmp = '?';
                        _TLOG(log::out << tmp;)
                    }
                }else{
                    _TLOG(log::out << chr;)
                }
                
                Assert(false); // we need the ability to add new characters not from text with a range str_start,str_end
                // outStream->addData(tmp);
                // token.length++;
                str_end++;
            }
            continue;
        }
        bool isQuotes = chr == '"' ||chr == '\'';
        bool isComment = chr=='/' && (nextChr == '/' || nextChr=='*');
        bool isDelim = chr==' ' || chr=='\t' || chr=='\n';
        if(!isDelim && !isComment){
            foundNonSpaceOnLine = true;
        }
        if(chr == '\n' || index == length) {
            if(foundNonSpaceOnLine){
                file->lines++;
                // outStream->lines++;
                foundNonSpaceOnLine = false;
            } else
                file->blank_lines++;
                // outStream->blankLines++;
        }
        bool isSpecial = false;
        if(!isQuotes&&!isComment&&!isDelim){ // early check for non-special
            char tmp = chr | 32;
            isSpecial = (tmp<'a'||tmp>'z')
                && (chr<'0'||chr>'9')
                && (chr!='_');
        }
        
        if(chr=='\n'){
            if(str_start != str_end){
               token.flags = (token.flags&(~TOKEN_FLAG_SPACE)) | TOKEN_FLAG_NEWLINE;
            }else {
                update_flags(TOKEN_FLAG_NEWLINE,TOKEN_FLAG_SPACE);
            }
            // else if(outStream->length()>0){
            //     Token& last = outStream->get(outStream->length()-1);
            //     last.flags = (last.flags&(~TOKEN_SUFFIX_SPACE)) | TOKEN_SUFFIX_LINE_FEED;
            //     // _TLOG(log::out << ", line feed";)
            // }
        }
        if(str_end == str_start && isDelim)
            continue;
        
        // Treat decimals with . as one token
        if(canBeDot && chr=='.' && nextChr != '.'){
            isSpecial = false;
            canBeDot=false;
        }
        if(!isDelim&&!isSpecial&&!isQuotes&&!isComment){
            if(str_end != str_start){
                _TLOG(log::out << "-";)
            }
            _TLOG(log::out << chr;)
            if(str_end == str_start){
                str_start = index;
                str_end = index+1;
                token_line = ln;
                token_column = col;
                // token.str = (char*)outStream->tokenData.used;
                // token.line = ln;
                // token.column = col;
                token.flags = 0;
                
                if(chr>='0'&&chr<='9') {
                    canBeDot=true;
                    isNumber=true;
                    if(chr == '0' && nextChr == 'x') {
                        inHexidecimal = true;
                    }
                } else {
                    canBeDot = false;
                    isAlpha = true;
                }
            }
            if(!(chr>='0'&&chr<='9') && chr!='.') // TODO: how does it work with hexidecimals?
                canBeDot=false;
            if(!isNumber || chr!='_') {
                // outStream->addData(chr);
                str_end++;
                // token.length++;
            }
        }
        
        bool nextLiteralSuffix = false;
        {
            char tmp = nextChr & ~32;
            if(isNumber && !inHexidecimal) {
                nextLiteralSuffix = !inHexidecimal && isNumber && ((tmp>='A'&&tmp<='Z') || nextChr == '_');
            }
        }
        if(str_start != str_end && (isDelim || isQuotes || isComment || isSpecial || nextLiteralSuffix || index==length)){
            token.flags = 0;
            // TODO: is checking line feed necessary? line feed flag of last token is set further up.
            if(chr=='\n')
                token.flags |= TOKEN_FLAG_NEWLINE;
            else if(chr==' ')
                token.flags |= TOKEN_FLAG_SPACE;
            // if(isAlpha)
            //     token.flags |= TOKEN_ALPHANUM;
            // if(isNumber)
            //     token.flags |= TOKEN_NUMERIC;
            // _TLOG(log::out << " : Add " << token << "\n";)
            
            canBeDot=false;
            isNumber=false;
            isAlpha=false;
            inHexidecimal = false;
            
            // TODO: Set token types, literals...

            token.type = TOKEN_IDENTIFIER;
            Token tok = appendToken(file_id, token.type, token.flags, token_line, token_column);
            modifyTokenData(tok, text + str_start, str_end - str_start, true);
            

            // outStream->addToken(token);
            
            token = {};
            _TLOG(log::out << "\n";)
        }
        // if(!inComment&&!inQuotes){
        if(isComment) {
            inComment=true;
            if(chr=='/' && nextChr=='*'){
                inEnclosedComment=true;
            }
            token_line = ln;
            token_column = col;
            file->comment_lines++;
            // outStream->commentCount++;
            index++; // skip the next slash
            _TLOG(log::out << "// : Begin comment\n";)
            continue;
        } else if(isQuotes){
            inQuotes = true;
            isSingleQuotes = chr == '\'';
            str_start = index+1;
            str_end = index+1; // we haven't parsed a character in the quotes yet
            // token.str = (char*)outStream->tokenData.used;
            // token.length = 0;
            token_line = ln;
            token_column = col;
            _TLOG(log::out << "\" : Begin quote\n";)
            continue;
        }
        // }
        if(isSpecial){
            if(chr=='#'){
                foundHashtag=true; // if not found then preprocessor isn't necessary
                
                // static const char* const str_import = "import";
                // static const int str_import_len = strlen(str_import);
                // int correct = 0;
                // int originalIndex = index;
                // bool readingQuote = false;
                // bool readingPath = false;
                // bool readingAs = false;
                // bool readingAsName = false;
                // int startOfPath = 0;
                // int endOfPath = 0;
                // int startOfAsName = 0;
                // int endOfAsName = 0;
                // int prev_import_count = outStream->importList.size();
                // while(index < length) {
                //     char prevChr = 0;
                //     char nextChr = 0;
                //     if(index>0)
                //         prevChr = text[index-1];
                //     if(index+1<length)
                //         nextChr = text[index+1];
                //     char chr = text[index];
                //     index++;
                    
                //     if(chr=='\t')
                //         column+=4;
                //     else
                //         column++;
                //     if(chr == '\n'){
                //         line++;
                //         column=1;
                //     }
                    
                //     if(readingAsName) {
                //         if(startOfAsName == 0) { // the name after as can never be at 0, we can therefore use it for another meaning
                //             if(chr == ' ' || chr== '\t') {
                //                 // whitespace is okay
                //             } else if(chr == '\n') {
                //                 const char* str = text + startOfPath;
                //                 char prev = text[endOfPath];
                //                 text[endOfPath] = '\0';
                //                 outStream->addImport(str,"");
                //                 text[endOfPath] = prev;
                //                 break;
                //             } else {
                //                 if(((chr|32) >= 'a' && (chr|32) <= 'z') || chr == '_') {
                //                     startOfAsName = index-1;
                //                 } else {
                //                     Assert(("bad as name, fix proper error",false));
                //                     break;  
                //                 }
                //             }
                //         } else {
                //             if(((chr|32) >= 'a' && (chr|32) <= 'z') || (chr >= '0' && chr <= '9') || chr == '_') {
                //                 // continue parsing
                //             } else {
                //                 // We are finally done
                //                 const char* str = text + startOfPath;
                //                 char prev = text[endOfPath];
                //                 text[endOfPath] = '\0';
                                
                //                 endOfAsName = index-1;
                //                 const char* str_as = text + startOfAsName;
                //                 char prev2 = text[endOfAsName];
                //                 text[endOfAsName] = '\0';
                                
                //                 outStream->addImport(str,str_as);
                //                 text[endOfPath] = prev;
                //                 text[endOfAsName] = prev2;
                //                 break;
                //             }
                //         }
                //     } else if (readingAs) {
                //         if(chr == 'a') {
                //             if(nextChr == 's') {
                //                 index++;
                //                 readingAsName = true;
                //                 readingAs = false;
                //                 continue;
                //             }
                //             const char* str = text + startOfPath;
                //             char prev = text[endOfPath];
                //             text[endOfPath] = '\0';
                //             outStream->addImport(str,"");
                //             text[endOfPath] = prev;
                //             break;
                //         } else if(chr == ' ' || chr== '\t') {
                //             // whitespace is okay
                //         } else if(chr == '\n') {
                //             const char* str = text + startOfPath;
                //             char prev = text[endOfPath];
                //             text[endOfPath] = '\0';
                //             outStream->addImport(str,"");
                //             text[endOfPath] = prev;
                //             break;
                //         }
                //     } else if(readingPath) {
                //         if(chr == '"') {
                //             endOfPath = index - 1;
                //             readingAs = true;
                //             readingPath = false;
                //             if(index == length) {
                //                 const char* str = text + startOfPath;
                //                 char prev = text[endOfPath];
                //                 text[endOfPath] = '\0';
                //                 outStream->addImport(str,"");
                //                 text[endOfPath] = prev;
                //                 break;
                //             }
                //         }
                //         if(chr == '\n') {
                //             // TODO: Print error
                //             break;
                //         }
                //     } else if(readingQuote) {
                //         if (chr == '"') {
                //             readingQuote = false;
                //             readingPath = true;   
                //             startOfPath = index;
                //         } else if(chr == ' ' || chr== '\t' || chr == '\n') {
                //             // whitespace is okay
                //         } else {
                //             break;   
                //         }
                //     } else {
                //         if(str_import[correct] == chr) {
                //             correct++;
                //             if(correct == str_import_len) {
                //                 readingQuote = true;
                //             }
                //         } else {
                //             correct=0;
                //             break;
                //         }
                //     }
                    
                //     if(chr=='\r'&&(nextChr=='\n'||prevChr=='\n')){
                //         continue;// skip \r and process \n next time
                //     }
                // }
                // if(prev_import_count == outStream->importList.size()) {
                //     // revert reading
                //     index = originalIndex;
                // } else {
                //     continue;
                // }
            }
            if(chr=='@'){
                Token anot = {};
                int anot_start = index;
                // anot.str = (char*)outStream->tokenData.used;
                int anot_line = ln;
                int anot_column = col;
                int anot_end = index+1;
                // anot.length = 1;
                // anot.flags = TOKEN_SUFFIX_SPACE;
                
                // outStream->addData(chr);
                bool bad=false;
                // TODO: Code below WILL have some edge cases not accounted for. Check it out.
                while(index<length){
                    char chr = text[index];
                    char nc = 0;
                    char pc = 0;

                    if(index>0)
                        pc = text[index-1];
                    if(index+1<length)
                        nc = text[index+1];
                    index++;
                    if(chr=='\t')
                        column+=4;
                    else
                        column++;
                    if(chr == '\n'){
                        line++;
                        column=1;
                    }
                    register char tmp = chr | 32;
                    if(!(tmp >= 'a' && tmp <= 'z') && !(chr == '_') && !(chr >= '0' && chr <= '9') ) {
                    // if(c==' '||c=='\t'||c=='\r'||c=='\n'){
                        if(chr=='\n')
                            anot.flags |= TOKEN_FLAG_NEWLINE;
                        else if(chr==' ')
                            anot.flags |= TOKEN_FLAG_SPACE;
                        else
                            index--; // go back, we didn't consume the characters
                        break;
                    }
                    // outStream->addData(chr);
                    // anot.length++;
                    anot_end++;
                    if (index==length){
                        break;
                    }
                }

                anot.type = TOKEN_ANNOTATION;
                Token tok = appendToken(file_id, anot.type, anot.flags, anot_line, anot_column);
                modifyTokenData(tok, text + anot_start, anot_end - anot_start, true);
                
                // outStream->addToken(anot);
                continue;
            }
            
            // this scope only adds special token
            _TLOG(log::out << chr;)
            token = {};
            str_start = index;
            str_end = index+1;
            // token.str = (char*)outStream->tokenData.used;
            // token.length = 1;
            // outStream->addData(chr);
            if(chr=='.'&&nextChr=='.'&&nextChr2=='.'){
                index+=2;
                column+=2;
                str_end+=2;
                // token.length+=2;
                _TLOG(log::out << nextChr;)
                _TLOG(log::out << nextChr2;)
                // outStream->addData(nextChr);
                // outStream->addData(nextChr2);
            }else if((chr=='='&&nextChr=='=')||
                (chr=='!'&&nextChr=='=')||
                // (chr=='+'&&nextChr=='=')||
                // (chr=='-'&&nextChr=='=')||
                // (chr=='*'&&nextChr=='=')||
                // (chr=='/'&&nextChr=='=')||
                (chr=='<'&&nextChr=='=')||
                (chr=='>'&&nextChr=='=')||
                (chr=='+'&&nextChr=='+')||
                (chr=='-'&&nextChr=='-')||
                (chr=='&'&&nextChr=='&')||
                // (chr=='>'&&nextChr=='>')|| // Arrows can be combined because they are used with polymorphism.
                // (chr=='<'&&nextChr=='<')|| // It would mess up the parsing.
                (chr==':'&&nextChr==':')||
                (chr=='.'&&nextChr=='.')||
                (chr=='|'&&nextChr=='|')
                ){
                index++;
                column++;
                // token.length++;
                str_end++;
                _TLOG(log::out << nextChr;)
                // outStream->addData(nextChr);
            }
            if(index<length)
                nextChr = text[index]; // code below needs the updated nextChr
            else
                nextChr = 0;
            
            token_line = ln;
            token_column = col;
            if(nextChr =='\n')
                token.flags = TOKEN_FLAG_NEWLINE;
            else if(nextChr==' ')
                token.flags = TOKEN_FLAG_SPACE;
            // _TLOG(log::out << " : Add " << token <<"\n";)
            _TLOG(log::out << " : special\n";)
            // outStream->addToken(token);
            token.type = TOKEN_IDENTIFIER;
            Token tok = appendToken(file_id, token.type, token.flags, token_line, token_column);
            modifyTokenData(tok, text + str_start, str_end - str_start, true);
                
            token = {};
        }
    }
    // outStream->addTokenAndData("$");
    if(!foundHashtag){
        // preprocessor not necessary, save time by not running it.
        // outStream->enabled &= ~LAYER_PREPROCESSOR;
        // _TLOG(log::out<<log::LIME<<"Couldn't find #. Disabling preprocessor.\n";)
    }
    
    // _LOG(LOG_IMPORTS,
    //     for(auto& str : outStream->importList){
    //         engone::log::out << log::LIME<<" #import '"<<str.name << "' as "<<str.as<<"'\n";
    //     }   
    // )
    // token.str = (char*)outStream->tokenData.data + (u64)token.str;
    // token.tokenIndex = outStream->length()-1;
    // token.tokenStream = outStream;
    // outStream->finalizePointers();
    
    // if(token.length!=0){
    //     // error happened in a way where we need to add a line feed
    //     _TLOG(log::out << "\n";)
    // }

    // README: Seemingly strange tokens can appear if you miss a quote somewhere.
    //  This is not a bug. It happens since quoted tokens are allowed across lines.
    
    if(inQuotes){
        // TODO: Improve error message for tokens
        log::out<<log::RED<<"TokenError: Missing end quote at "<<path_name <<":"<< token_line<<":"<<token_column<<"\n";
        // outStream->tokens.used--; // last token is corrupted and should be removed
        goto Tokenize_END;
    }
    if(inComment){
        log::out<<log::RED<<"TokenError: Missing end comment for "<<token_line<<":"<<token_column<<"\n";
        // outStream->tokens.used--; // last token is corrupted and should be removed
        goto Tokenize_END;
    }
    // if(str_end != str_end){
    //     log::out << log::RED<<"TokenError: Token '" << token << "' was left incomplete\n";
    //     goto Tokenize_END;
    // }

    // Last token should have line feed.
    update_flags(TOKEN_FLAG_NEWLINE);
    // if(outStream->length()>0)
    //     outStream->get(outStream->length()-1).flags |=TOKEN_SUFFIX_LINE_FEED;

    // if(outStream->tokens.used!=0){
    //     Token* lastToken = ((Token*)outStream->tokens.data+outStream->tokens.used-1);
    //     int64 extraSpace = (int64)outStream->tokenData.data + outStream->tokenData.max - (int64)lastToken->str - lastToken->length;
    //     int64 extraSpace2 = outStream->tokenData.max-outStream->tokenData.used;
        
    //     if(extraSpace!=extraSpace2)
    //         log::out << log::RED<<"TokenError: Used memory in tokenData does not match last token's used memory ("<<outStream->tokenData.used<<" != "<<((int64)lastToken->str - (int64)outStream->tokenData.data + lastToken->length)<<")\n";
            
    //     if(extraSpace<0||extraSpace2<0)
    //         log::out << log::RED<<"TokenError: Buffer of tokenData was to small (estimated "<<outStream->tokenData.max<<" but needed "<<outStream->tokenData.used<<" bytes)\n'";
        
    //     if(extraSpace!=extraSpace2 || extraSpace<0 || extraSpace2<0)
    //         goto Tokenize_END;
    // }
    
Tokenize_END:
    // log::out << "Last: "<<outStream->get(outStream->length()-1)<<"\n";
    return file_id;
}

Token Lexer::appendToken(u16 fileId, TokenType type, u32 flags, u32 line, u32 column){
    Assert(flags < 0x10000 && line < 0x10000 && column < 0x10000);

    File* file = files.get(fileId-1);
    Assert(file);
    
    Chunk* chunk = nullptr;
    u32 cindex = 0;
    if(file->chunk_indices.size() > 0) {
        cindex = file->chunk_indices.last();
        chunk = chunks.get(cindex);
        if(chunk->tokens.size()+1 >= 0x10000) {
            chunk = nullptr; // chunk is full!
            cindex = 0;
        }
    }
    if(!chunk) {
        cindex = chunks.add(nullptr,&chunk);
        chunk->file_id = fileId;
        file->chunk_indices.add(cindex);
    }
    
    chunk->tokens.add({});
    auto& info = chunk->tokens.last();
    info.type = type;
    info.flags = flags;
    info.line = line;
    info.column = column;
    info.data_offset = 0;

    Token tok{};
    tok.type = type;
    tok.flags = flags;
    tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
    return tok;
}
Lexer::TokenInfo* Lexer::getTokenInfo_unsafe(Token token){
    u32 cindex;
    u32 tindex;
    decode_origin(token.origin, &cindex, &tindex);

    Chunk* chunk = chunks.get(cindex);
    Assert(chunk);
    if(!chunk) return nullptr;
    return chunk->tokens.getPtr(tindex);
}
u32 Lexer::modifyTokenData(Token token, void* ptr, u32 size, bool with_null_termination) {
    u32 cindex;
    u32 tindex;
    decode_origin(token.origin, &cindex, &tindex);

    Chunk* chunk = chunks.get(cindex);
    Assert(chunk);
    if(!chunk) return 0;
    auto info = chunk->tokens.getPtr(tindex);

    if(info->flags & TOKEN_FLAG_HAS_DATA) {
        Assert(false);
    } else {
        Assert(size <= 0x100);
        info->flags |= TOKEN_FLAG_HAS_DATA;
        info->data_offset = chunk->auxiliary_data.size();
        if(with_null_termination) {
            chunk->auxiliary_data.resize(chunk->auxiliary_data.size() + size + 2);
            chunk->auxiliary_data[info->data_offset] = size;
            memcpy(chunk->auxiliary_data.data() + info->data_offset+1, ptr, size);
            chunk->auxiliary_data[info->data_offset + 1 + size] = '\0';
        } else {
            chunk->auxiliary_data.resize(chunk->auxiliary_data.size() + size + 1);
            chunk->auxiliary_data[info->data_offset] = size;
            memcpy(chunk->auxiliary_data.data() + info->data_offset+1, ptr, size);
        }
    }
    return 0; // TODO: Don't always return 0, what should we do?
}
u16 Lexer::createFile(File** file) {
    using namespace engone;
    Assert(file);
    if(files.getCount() >= 0xFFFF) {
        *file = nullptr;
        return 0;
    }
    u32 file_index = files.add(nullptr, file);

    if(file_index+1 >= 0x10000) {
        log::out << log::RED << "COMPILER BUG: The lexer design cannot support more than "<<0xFFFF<<" files.\n";
        // unsigned 16-bit integer can't support the file id
        files.removeAt(file_index);
        *file = nullptr;
        return 0;
    }

    return file_index+1;
}
Token Lexer::getTokenFromFile(u32 fileid, u32 token_index_into_file) {
    File* file = files.get(fileid-1);
    Assert(file);

    u32 fcindex,tindex;
    decode_file_token_index(token_index_into_file, &fcindex,&tindex);

    Assert(file->chunk_indices.size() > fcindex);

    u32 cindex = file->chunk_indices.get(fcindex);
    Chunk* chunk = chunks.get(cindex);

    auto info = chunk->tokens.getPtr(tindex);
    Token out{};
    out.flags = info->flags;
    out.type = info->type;
    out.origin = encode_origin(cindex,tindex);
    return out;
}
Lexer::FeedIterator Lexer::createFeedIterator(Token start_token, Token end_token) {
    FeedIterator iter{};

    u32 cindex,tindex;
    decode_origin(start_token.origin,&cindex,&tindex);

    Chunk* first_chunk = chunks.get(cindex);
    Assert(first_chunk);

    iter.file_id = first_chunk->file_id;
    File* file = files.get(iter.file_id-1);
    Assert(file);

    bool found_id = false;
    for(int i=0;i<file->chunk_indices.size();i++){
        if(file->chunk_indices[i] == cindex) {
            iter.file_token_index = encode_file_token_index(i,tindex);
            found_id = true;
            break;
        }
    }
    Assert(found_id);

    if(end_token.type == TOKEN_NONE) {
        iter.end_file_token_index = iter.file_token_index + 1;
    } else {
        u32 cindex,tindex;
        decode_origin(start_token.origin,&cindex,&tindex);

        Chunk* last_chunk = chunks.get(cindex);
        Assert(("COMPILER BUG: tokens must come from the same file when iterating", last_chunk->file_id == first_chunk->file_id));

        bool found_id = false;
         for(int i=0;i<file->chunk_indices.size();i++){
            if(file->chunk_indices[i] == cindex) {
                iter.end_file_token_index = encode_file_token_index(i,tindex);
                found_id = true;
                break;
            }
        }
        Assert(found_id);
    }
    return iter;
}
u32 Lexer::feed(char* buffer, u32 buffer_max, FeedIterator& iterator) {
    using namespace engone;
    Assert(iterator.file_id != 0);

    #define ENSURE(N) if (written + N >= buffer_max) return written;

    int written = 0;
    while(true){
        if(iterator.file_token_index == iterator.end_file_token_index)
            return written;
        Token tok = getTokenFromFile(iterator.file_id, iterator.file_token_index);
        
        if(tok.type == TOKEN_LITERAL_STRING) {

        } else if(tok.type == TOKEN_IDENTIFIER) {
            char* str;
            u8 real_len = getStringFromToken(tok,&str);

            if(iterator.char_index >= real_len) {
                // we wrote the content last feed
                // len = 0;
            } else {
                u8 len = real_len - iterator.char_index;
                str += iterator.char_index;
                if(written + len > buffer_max) {
                    // clamp, not enough space in buffer
                    len = buffer_max - written;
                }
                if(len!=0){
                    memcpy(buffer + written, str, len);
                    iterator.char_index += len;
                    written += len;
                }
                if(len != real_len) {
                    // return, caller needs to try again, buffer too small
                    return written;
                }
            }
        }

        // TODO: Don't do suffix if we processed the last token or if suffix should be skipped (skipSuffix)
        if(tok.flags & TOKEN_FLAG_NEWLINE) {
            ENSURE(1)
            // TODO: Check iterator.char_index, we may already have written this suffix
            *(buffer + written) = '\n';
            written++;
            iterator.char_index++;
        } else if(tok.flags & TOKEN_FLAG_SPACE) {
            ENSURE(1)
            // TODO: Check iterator.char_index, we may already have written this suffix
            *(buffer + written) = ' ';
            written++;
            iterator.char_index++;
            // TODO: Calculate space based on the token's column
        }

        iterator.char_index = 0;
        iterator.file_token_index++;


        // // ENSURE(2) // queryFeedSize makes the caller use a specific bufferSize.
        // //  Ensuring and returning when we THINK we can't fit characters would skip some remaining tokens even though they would actually fit if we tried.
        // if(*charIndex == 0) {
        //     if(tok.flags&TOKEN_DOUBLE_QUOTED){
        //         if(quoted_environment) {
        //             ENSURE(1)
        //             *(outBuffer + written_temp++) = '\\';
        //         }
        //         ENSURE(1)
        //         *(outBuffer + written_temp++) = '"';
        //     }
        //     else if(tok.flags&TOKEN_SINGLE_QUOTED){
        //         if(quoted_environment) {
        //             ENSURE(1)
        //             *(outBuffer + written_temp++) = '\\';
        //         }
        //         ENSURE(1)
        //         *(outBuffer + written_temp++) = '\'';
        //     }
        // }
        // for(int j=*charIndex;j<tok.length;j++){
        //     char chr = *(tok.str+j);
        //     // ENSURE(4) // 0x03 may occur which needs 4 bytes
        //     if(chr=='\n'){
        //         ENSURE(2)
        //         *(outBuffer + written_temp++) = '\\';
        //         *(outBuffer + written_temp++) = 'n';
        //     } else if(chr=='\t'){
        //         ENSURE(2)
        //         *(outBuffer + written_temp++) = '\\';
        //         *(outBuffer + written_temp++) = 't';
        //     } else if(chr=='\r'){
        //         ENSURE(2)
        //         *(outBuffer + written_temp++) = '\\';
        //         *(outBuffer + written_temp++) = 'r';
        //     } else if(chr=='\0'){
        //         ENSURE(2)
        //         *(outBuffer + written_temp++) = '\\';
        //         *(outBuffer + written_temp++) = '0';
        //     } else if(chr=='\x1b'){ // '\e' becomes 'e' in some compilers (MSVC, sigh)
        //         ENSURE(2)
        //         *(outBuffer + written_temp++) = '\\';
        //         *(outBuffer + written_temp++) = 'e';
        //     } else if(0 == (chr&0xE0)){ // chr < 32
        //         ENSURE(4)
        //         *(outBuffer + written_temp++) = '\\';
        //         *(outBuffer + written_temp++) = 'x';
        //         *(outBuffer + written_temp++) = (chr >> 4) < 10 ? (chr >> 4) + '0' : (chr >> 4) + 'a';
        //         *(outBuffer + written_temp++) = (chr & 0xF) < 10 ? (chr & 0xF) + '0' : (chr & 0xF) + 'a';;
        //     } else {
        //         ENSURE(1)
        //         *(outBuffer + written_temp++) = chr;
        //     }
        // }
        // // ENSURE(4)
        // if(tok.flags&TOKEN_DOUBLE_QUOTED){
        //     if(quoted_environment) {
        //         ENSURE(1)
        //         *(outBuffer + written_temp++) = '\\';
        //     }
        //     ENSURE(1)
        //     *(outBuffer + written_temp++) = '"';
        // } else if(tok.flags&TOKEN_SINGLE_QUOTED){
        //     if(quoted_environment) {
        //         ENSURE(1)
        //         *(outBuffer + written_temp++) = '\\';
        //     }
        //     ENSURE(1)
        //     *(outBuffer + written_temp++) = '\'';
        // }
        // if(!skipSuffix || i+1 != endIndex) {
        //     if((tok.flags&TOKEN_SUFFIX_LINE_FEED)){
        //         ENSURE(1)
        //         *(outBuffer + written_temp++) = '\n';
        //     } else if((tok.flags&TOKEN_SUFFIX_SPACE)){ // don't write space if we wrote newline
        //         ENSURE(1)
        //         *(outBuffer + written_temp++) = ' ';
        //     }
        // }
        // *charIndex = 0;
        // written = written_temp;
    }
    #undef ENSURE
    return written;
}
u32 Lexer::getStringFromToken(Token tok, char** ptr) {
    return getDataFromToken(tok, (void**)ptr);
}
u32 Lexer::getDataFromToken(Token tok, void** ptr){
    auto info = getTokenInfo_unsafe(tok);
    u32 cindex,tindex;
    decode_origin(tok.origin,&cindex,&tindex);
    Chunk* chunk = chunks.get(cindex);

    *ptr = chunk->auxiliary_data.data() + info->data_offset + 1;
    u8 len = chunk->auxiliary_data[info->data_offset];
    return len;
}
void Lexer::print(u32 fileid) {
    using namespace engone;
    File* file = files.get(fileid-1);
    Assert(file);
    if(file->chunk_indices.size()==0)
        return;

    u32 cindex = file->chunk_indices.last();
    Chunk* last = chunks.get(cindex);

    FeedIterator iter{};
    iter.file_id = fileid;
    iter.file_token_index = 0;
    // We don't encode_file_token_index because integer overflow. If you know how not to create a bug then
    // you are free to use the encode function instead.
    iter.end_file_token_index = (file->chunk_indices.size()-1) * 0x10000 + last->tokens.size();

    char buffer[256];
    u32 written = 0;
    while((written = feed(buffer,sizeof(buffer),iter))) {
        log::out.print(buffer,written);
    }
}
#define TOK_NAME(TYPE) (TYPE < 256 ? (char)TYPE : lexer::token_type_names[TYPE - TOKEN_TYPE_BEGIN])
const char* token_type_names[] {
    "none",
    "tok_id",
    "tok_anot",
    "tok_lit",
};
TokenType StringToTokenType(const char* str, int len) {
    

    return TOKEN_NONE;
}

// u32 Lexer::encode_origin(u32 token_index){
//     return token_index;
// }
TokenOrigin Lexer::encode_origin(u32 chunk_index, u32 tiny_token_index) {
    Assert(chunk_index < (1<<TOKEN_ORIGIN_CHUNK_BITS));
    Assert(tiny_token_index < (1<<TOKEN_ORIGIN_TOKEN_BITS));
    return (chunk_index<<TOKEN_ORIGIN_TOKEN_BITS) | tiny_token_index;
}
void Lexer::decode_origin(TokenOrigin origin, u32* chunk_index, u32* tiny_token_index){
    Assert(chunk_index && tiny_token_index);
    // #if TOKEN_ORIGIN_CHUNK_BITS + TOKEN_ORIGIN_TOKEN_BITS > 32
    // *chunk_index = *(u64*)origin.data >> TOKEN_ORIGIN_TOKEN_BITS;
    // *tiny_token_index = *(u64*)origin.data & ((1<<TOKEN_ORIGIN_TOKEN_BITS) - 1);
    // #else
    // *chunk_index = *(u32*)origin >> TOKEN_ORIGIN_TOKEN_BITS;
    // *tiny_token_index = *(u32*)origin & ((1<<TOKEN_ORIGIN_TOKEN_BITS) - 1);
    // #endif
    
    *chunk_index = origin >> TOKEN_ORIGIN_TOKEN_BITS;
    *tiny_token_index = origin & ((1<<TOKEN_ORIGIN_TOKEN_BITS) - 1);
}
u32 Lexer::encode_file_token_index(u32 file_chunk_index, u32 tiny_token_index){
    return encode_origin(file_chunk_index,tiny_token_index);
}
void Lexer::decode_file_token_index(u32 token_index_into_file, u32* file_chunk_index, u32* tiny_token_index) {
    return decode_origin(TokenOrigin{token_index_into_file}, file_chunk_index, tiny_token_index);
}
}