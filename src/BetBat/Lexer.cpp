#include "BetBat/Lexer.h"

#include "BetBat/Util/Perf.h"

namespace lexer {
u32 Lexer::tokenize(const std::string& path, u32 existing_import_id){
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
        
    u32 file_id = tokenize(buffer,size,path, existing_import_id);

    return file_id;
}
u32 Lexer::tokenize(char* text, u64 length, const std::string& path_name, u32 existing_import_id){
    using namespace engone;
    ZoneScopedC(tracy::Color::Gold);

    Import* lexer_import=nullptr;
    u32 file_id = 0;
    
    if(existing_import_id==0){
        file_id = createImport(path_name, &lexer_import);
    } else {
        // use existing/prepared import
        file_id = existing_import_id;
        lock_imports.lock();
        lexer_import = imports.get(existing_import_id-1);
        lock_imports.unlock();
    }
    Assert(lexer_import && file_id != 0);
    
    
    lexer_import->fileSize = length;

    // TODO: Optimize by keeping traack of chunks ids and their pointers here instead of using
    //  appendToken which queries file and chunk id every time.


    // Token token = {};
    // int token_line = 0;
    // int token_column = 0;
    
    int str_start = 0;
    int str_end = 0;
    
    TokenInfo* prev_token = nullptr; // quite useful
    TokenInfo* new_tokens = nullptr;
    int new_tokens_len = 0;
    Chunk* last_chunk = nullptr;
    
    {
        lock_chunks.lock();
        int cindex = chunks.add(nullptr,&last_chunk);
        lock_chunks.unlock();
        
        last_chunk->chunk_index = cindex;
        last_chunk->import_id = file_id;
        lexer_import->chunk_indices.add(cindex);
        lexer_import->chunks.add(last_chunk);
    }
    
    auto update_flags = [&](u32 set, u32 clear = 0){
        if(prev_token) {
            prev_token->flags = (prev_token->flags&~clear) | set;
        }
        // if(lexer_import->chunk_indices.size()!=0) {
        //     lock_imports.lock();
        //     Chunk* chunk = chunks.get(lexer_import->chunk_indices.last());
        //     lock_imports.unlock();
        //     chunk->tokens.last().flags = (chunk->tokens.last().flags&~clear) | set;
        // }
    };
    auto reserv_tokens=[&](){
        Assert(last_chunk);
        Assert(new_tokens_len == 0);
        
        int n = 20;
        if(last_chunk->tokens.size() + n < last_chunk->tokens.max) {
            // enough space
        } else if(last_chunk->tokens.size() == TOKEN_ORIGIN_TOKEN_MAX) {
            // chunk is full
            lock_chunks.lock();
            int cindex = chunks.add(nullptr,&last_chunk);
            lock_chunks.unlock();
            
            last_chunk->chunk_index = cindex;
            last_chunk->import_id = file_id;
            lexer_import->chunk_indices.add(cindex);
            lexer_import->chunks.add(last_chunk);
            
            last_chunk->tokens._reserve(TOKEN_ORIGIN_TOKEN_MAX);
        } else {
            int prev_index=0;
            if(prev_token)
                prev_index = prev_token - last_chunk->tokens.data(); 
            last_chunk->tokens._reserve(TOKEN_ORIGIN_TOKEN_MAX);
            if(prev_token)
                prev_token = prev_index + last_chunk->tokens.data(); 
        }
        // we can't fit all N requested tokens so we clamp
        if(last_chunk->tokens.used + n > TOKEN_ORIGIN_TOKEN_MAX) {
            n = TOKEN_ORIGIN_TOKEN_MAX - last_chunk->tokens.used;   
        }
        
        new_tokens = last_chunk->tokens.data() + last_chunk->tokens.used;
        last_chunk->tokens.used += n;
        new_tokens_len = n;
        
        *new_tokens = {};
    };
    
    reserv_tokens(); // make sure we start with a token so that the lexer doesn't have to check if a token has been requested or not everytime it needs to modify the current/next token
    auto INCREMENT_TOKEN = [&](){
        prev_token = new_tokens;
        new_tokens++;
        new_tokens_len--;
        if(new_tokens_len == 0)
            reserv_tokens();
        *new_tokens = {};
    };
    
    auto APPEND_DATA = [&](void* ptr, int size) {
        auto info = new_tokens;
        // TODO: You can probably optimize this function
        if(info->type & (TOKEN_LITERAL_STRING|TOKEN_ANNOTATION|TOKEN_IDENTIFIER))
            info->flags |= TOKEN_FLAG_NULL_TERMINATED;
        if(info->flags & TOKEN_FLAG_HAS_DATA) {
            u8 len = last_chunk->aux_data[info->data_offset];
            // If we had null termination first time we added data then we should have specified it now to.
            // probably bug otherwise.
            if(last_chunk->aux_used == info->data_offset + 1 + len + (info->flags&TOKEN_FLAG_NULL_TERMINATED?1:0)) {
                Assert(len+size < 0x100); // check integer overflow
                // Previous data exists at the end so we can just append the new data.
                last_chunk->alloc_aux_space(size);
                // chunk->auxiliary_data.resize(chunk->auxiliary_data.size() + size);
                last_chunk->aux_data[info->data_offset] = len + size;
                memcpy(last_chunk->aux_data + info->data_offset + 1 + len, ptr, size);
                if(info->flags&TOKEN_FLAG_NULL_TERMINATED) {
                    last_chunk->aux_data[info->data_offset + 1 + len + size] = '\0';
                }
            } else {
                // We have move memory because previous data wasn't at the end and there is probably not any space.
                // TODO: Just moving data to the end will cause fragmentation. Should we perhaps not allow this
                //   or do we do something about it?
                Assert(false);
            }
        } else {
            Assert(size <= 0x100);
            info->flags |= TOKEN_FLAG_HAS_DATA;
            info->data_offset = last_chunk->aux_used;
            if(info->flags&TOKEN_FLAG_NULL_TERMINATED) {
                info->flags |= TOKEN_FLAG_NULL_TERMINATED;
                last_chunk->alloc_aux_space(size + 2);
                last_chunk->aux_data[info->data_offset] = size;
                memcpy(last_chunk->aux_data + info->data_offset+1, ptr, size);
                last_chunk->aux_data[info->data_offset + 1 + size] = '\0';
            } else {
                last_chunk->alloc_aux_space(size + 1);
                last_chunk->aux_data[info->data_offset] = size;
                memcpy(last_chunk->aux_data + info->data_offset+1, ptr, size);
            }
        }
    };
    
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
    
    // Token quote_token{};

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
                        lexer_import->blank_lines++;
                        // outStream->blankLines++;
                        foundNonSpaceOnLine = false;
                    } else
                        lexer_import->lines++;
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
                // I tried to optimize comment parsing at one point but it wasn't any faster.
                // Also, comments is not the slow part of the compiler so please don't waste time optimizing them.
                if(chr=='\n'||index==length){
                    if(!foundNonSpaceOnLine)
                        lexer_import->blank_lines++;
                        // outStream->blankLines++;
                    else
                        lexer_import->lines++;
                        // outStream->lines++;
                    foundNonSpaceOnLine=false;
                    // if(outStream->length()!=0)
                    //     outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
                    update_flags(TOKEN_FLAG_NEWLINE);
                    inComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
            }
            continue;
        } else if(inQuotes) {
            if((!isSingleQuotes && chr == '"') || 
                (isSingleQuotes && chr == '\'')){
                // Stop reading string token
                inQuotes=false;
                
                // new_tokens->flags = 0;
                if(nextChr=='\n')
                    new_tokens->flags |= TOKEN_FLAG_NEWLINE;
                else if(nextChr==' ')
                    new_tokens->flags |= TOKEN_FLAG_SPACE;
                if(isSingleQuotes)
                    new_tokens->flags |= TOKEN_FLAG_SINGLE_QUOTED;
                else
                    new_tokens->flags |= TOKEN_FLAG_DOUBLE_QUOTED;
                
                // _TLOG(log::out << " : Add " << token << "\n";)
                _TLOG(if(str_start != str_end) log::out << "\n"; log::out << (isSingleQuotes ? '\'' : '"') <<" : End quote\n";)

                
                // token.type = TOKEN_LITERAL_STRING;
                // Token tok = appendToken(file_id, token.type, token.flags, token_line, token_column);
                // modifyTokenData(tok, text + str_start, str_end - str_start, true);
                
                // new_tokens->flags |= ;
                
                // auto info = getTokenInfo_unsafe(quote_token);
                // info->flags |= new_tokens->flags;
                
                INCREMENT_TOKEN();

                // outStream->addToken(token);

                // token = {};
                str_start = 0;
                str_end = 0;
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
                // str_end++;
                APPEND_DATA(&tmp,1);
                // modifyTokenData(quote_token, &tmp, 1, true);
                // Assert(false); // we need the ability to add new characters not from text with a range str_start,str_end
                // outStream->addData(tmp);
                // token.length++;
                // str_end++;
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
                lexer_import->lines++;
                // outStream->lines++;
                foundNonSpaceOnLine = false;
            } else
                lexer_import->blank_lines++;
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
               new_tokens->flags = (new_tokens->flags&(~TOKEN_FLAG_SPACE)) | TOKEN_FLAG_NEWLINE;
            }else {
                update_flags(TOKEN_FLAG_NEWLINE,TOKEN_FLAG_SPACE);
            }
            // else if(outStream->length()>0){
            //     Token& last = outStream->get(outStream->length()-1);
            //     last.flags = (last.flags&(~TOKEN_SUFFIX_SPACE)) | TOKEN_SUFFIX_LINE_FEED;
            //     // _TLOG(log::out << ", line feed";)
            // }
        }
        if(str_start == str_end && isDelim)
            continue;
        
        // Treat decimals with . as one token
        if(canBeDot && chr=='.' && nextChr != '.'){
            isSpecial = false;
            canBeDot=false;
        }
        if(!isDelim&&!isSpecial&&!isQuotes&&!isComment){
            if(str_start != str_end){
                _TLOG(log::out << "-";)
            }
            _TLOG(log::out << chr;)
            if(str_start == str_end){
                str_start = index-1;
                str_end = str_start;
                new_tokens->line = ln;
                new_tokens->column = col;
                // token.str = (char*)outStream->tokenData.used;
                // token.line = ln;
                // token.column = col;
                new_tokens->flags = 0;
                
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
            new_tokens->flags = 0;
            // TODO: is checking line feed necessary? line feed flag of last token is set further up.
            if(chr=='\n')
                new_tokens->flags |= TOKEN_FLAG_NEWLINE;
            else if(chr==' ')
                new_tokens->flags |= TOKEN_FLAG_SPACE;
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

            new_tokens->type = TOKEN_IDENTIFIER;
            
            // Even faster code
            APPEND_DATA(text+str_start, str_end - str_start);
            INCREMENT_TOKEN();
            
            // Slow code
            // Token tok = appendToken(file_id, token.type, token.flags, token_line, token_column);
            // modifyTokenData(tok, text + str_start, str_end - str_start, true);
            
            // { // Faster code by inlining functions by hand
            //     Chunk* chunk = nullptr;
            //     u32 cindex = 0;
            //     if(lexer_import->chunk_indices.size() > 0) {
            //         cindex = lexer_import->chunk_indices.last();
                    
            //         lock_chunks.lock();
            //         chunk = chunks.get(cindex);
            //         lock_chunks.unlock();
                    
            //         if(chunk->tokens.size()+1 >= TOKEN_ORIGIN_TOKEN_MAX) {
            //             chunk = nullptr; // chunk is full!
            //             cindex = 0;
            //         }
            //     }
            //     if(!chunk) {
            //         lock_chunks.lock();
            //         cindex = chunks.add(nullptr,&chunk);
            //         lock_chunks.unlock();
                    
            //         chunk->chunk_index = cindex;
            //         chunk->import_id = file_id;
            //         lexer_import->chunk_indices.add(cindex);
            //     }
            //     chunk->tokens.add({});
            //     auto& info = chunk->tokens.last();
            //     info.type = token.type;
            //     info.flags = token.flags;
            //     info.line = token_line;
            //     info.column = token_column;
            //     info.data_offset = 0;
                
            //     void* ptr = text+str_start;
            //     int size = str_end - str_start;
            //     Assert(size <= 0x100);
            //     info.flags |= TOKEN_FLAG_HAS_DATA;
            //     info.data_offset = chunk->aux_used;
                
            //     info.flags |= TOKEN_FLAG_NULL_TERMINATED;
            //     chunk->alloc_aux_space(size + 2);
            //     chunk->aux_data[info.data_offset] = size;
            //     memcpy(chunk->aux_data + info.data_offset+1, ptr, size);
            //     chunk->aux_data[info.data_offset + 1 + size] = '\0';
            // }

            // token = {};
            str_start = 0;
            str_end = 0;
            _TLOG(log::out << "\n";)
        }
        // if(!inComment&&!inQuotes){
        if(isComment) {
            inComment=true;
            if(chr=='/' && nextChr=='*'){
                inEnclosedComment=true;
            }
            new_tokens->line = ln;
            new_tokens->column = col;
            lexer_import->comment_lines++;
            // outStream->commentCount++;
            index++; // skip the next slash
            _TLOG(log::out << "// : Begin comment\n";)
            continue;
        } else if(isQuotes){
            inQuotes = true;
            isSingleQuotes = chr == '\'';
            // str_start = index-1+1;
            // str_end = index-1+1; // we haven't parsed a character in the quotes yet
            // token.str = (char*)outStream->tokenData.used;
            // token.length = 0;
            new_tokens->line = ln;
            new_tokens->column = col;
            
            new_tokens->type = TOKEN_LITERAL_STRING;
            // quote_token = appendToken(file_id, token.type, token.flags, token_line, token_column);
            
            _TLOG(log::out << "\" : Begin quote\n";)
            continue;
        }
        // }
        if(isSpecial){
            if(chr=='#'){
                foundHashtag=true; // if not found then preprocessor isn't necessary
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
                
                new_tokens->type = TOKEN_ANNOTATION;
                new_tokens->flags = anot.flags;
                new_tokens->line = anot_line;
                new_tokens->column = anot_column;
                
                APPEND_DATA(text+anot_start, anot_end - anot_start);
                
                INCREMENT_TOKEN();

                // anot.type = TOKEN_ANNOTATION;
                // Token tok = appendToken(file_id, anot.type, anot.flags, anot_line, anot_column);
                // modifyTokenData(tok, text + anot_start, anot_end - anot_start, true);
                
                // outStream->addToken(anot);
                continue;
            }
            
            // this scope only adds special token
            _TLOG(log::out << chr;)
            // token = {};
            str_start = index-1;
            str_end = str_start+1;
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
            
            new_tokens->line = ln;
            new_tokens->column = col;
            if(nextChr =='\n')
                new_tokens->flags = TOKEN_FLAG_NEWLINE;
            else if(nextChr==' ')
                new_tokens->flags = TOKEN_FLAG_SPACE;
            // _TLOG(log::out << " : Add " << token <<"\n";)
            _TLOG(log::out << " : special\n";)
            // outStream->addToken(token);
            
            if(str_end - str_start == 1) {
                new_tokens->type = (TokenType)chr;
                
                // Even faster code
                INCREMENT_TOKEN();
                
                // Token tok = appendToken(file_id, token.type, token.flags, token_line, token_column);
                
                // { // Manual inlined code, a bit faster (one profiler test showed 350ms -> 280ms)
                //     Chunk* chunk = nullptr;
                //     u32 cindex = 0;
                //     if(lexer_import->chunk_indices.size() > 0) {
                //         cindex = lexer_import->chunk_indices.last();
                        
                //         lock_chunks.lock();
                //         chunk = chunks.get(cindex);
                //         lock_chunks.unlock();
                        
                //         if(chunk->tokens.size()+1 >= TOKEN_ORIGIN_TOKEN_MAX) {
                //             chunk = nullptr; // chunk is full!
                //             cindex = 0;
                //         }
                //     }
                //     if(!chunk) {
                //         lock_chunks.lock();
                //         cindex = chunks.add(nullptr,&chunk);
                //         lock_chunks.unlock();
                        
                //         chunk->chunk_index = cindex;
                //         chunk->import_id = file_id;
                //         lexer_import->chunk_indices.add(cindex);
                //     }
                    
                //     chunk->tokens.add({});
                //     auto& info = chunk->tokens.last();
                //     info.type = token.type;
                //     info.flags = token.flags;
                //     info.line = token_line;
                //     info.column = token_column;
                //     info.data_offset = 0;
                // }
                
            } else {
                new_tokens->type = TOKEN_IDENTIFIER;
                
                APPEND_DATA(text+str_start, str_end - str_start);
                
                INCREMENT_TOKEN();
                
                // token.type = TOKEN_IDENTIFIER;
                // Token tok = appendToken(file_id, token.type, token.flags, token_line, token_column);
                // modifyTokenData(tok, text + str_start, str_end - str_start, true);
            }
                
            // token = {};
            str_start = 0;
            str_end = 0;
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
    
    Assert(last_chunk->tokens.used >= new_tokens_len); // I have a feeling there is a bug here.
    last_chunk->tokens.used -= new_tokens_len; // we didn't use all tokens we requested so we "give them back"
    
    TokenInfo* last_token = &last_chunk->tokens.last(); // TODO: can this crash?
    
    if(inQuotes){
        // TODO: Improve error message for tokens
        log::out<<log::RED<<"TokenError: Missing end quote at "<<path_name <<":"<< last_token->line<<":"<<last_token->column<<"\n";
        // outStream->tokens.used--; // last token is corrupted and should be removed
        goto Tokenize_END;
    }
    if(inComment){
        log::out<<log::RED<<"TokenError: Missing end comment for "<<last_token->line<<":"<<last_token->column<<"\n";
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
void Lexer::appendToken(Import* imp, TokenInfo* from_info, StringView* string) {
    // ZoneScoped;
    Assert(imp);
    
    Chunk* chunk = nullptr;
    u32 cindex = -1;
    // Assert(imp->chunk_indices.size() > 0);
    if(imp->chunk_indices.size() > 0) {
        // cindex = imp->chunk_indices.last();
        
        // lock_chunks.lock();
        // chunk = chunks.get(cindex);
        // lock_chunks.unlock();
        chunk = imp->chunks.last();
        
        if(chunk->tokens.size()+1 >= TOKEN_ORIGIN_TOKEN_MAX) {
            chunk = nullptr; // chunk is full!
            cindex = 0;
        }
    }
    if(!chunk) {
        lock_chunks.lock();
        cindex = chunks.add(nullptr,&chunk);
        lock_chunks.unlock();
        
        chunk->chunk_index = cindex;
        chunk->import_id = imp->file_id;
        imp->chunk_indices.add(cindex);
        imp->chunks.add(chunk);
    }
    
    chunk->tokens.add({});
    auto info = &chunk->tokens.last();
    
    *info = *from_info;
    info->flags = from_info->flags;
    info->data_offset = 0;
    
    if(from_info->flags & TOKEN_FLAG_HAS_DATA) {
        Assert(string);
        const void* ptr=string->ptr;
        u32 size = string->len;
        
        info->data_offset = chunk->aux_used;
        if(from_info->flags & TOKEN_FLAG_NULL_TERMINATED) {
            chunk->alloc_aux_space(size + 2);
            chunk->aux_data[info->data_offset] = size;
            memcpy(chunk->aux_data + info->data_offset+1, ptr, size);
            chunk->aux_data[info->data_offset + 1 + size] = '\0';
        } else {
            chunk->alloc_aux_space(size + 1);
            chunk->aux_data[info->data_offset] = size;
            memcpy(chunk->aux_data + info->data_offset+1, ptr, size);
        }
    }
    Assert(cindex != 0);
    // Token tok{};
    // tok.type = info->type;
    // tok.flags = info->flags;
    // tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
    // return tok;
}
Token Lexer::appendToken(u32 fileId, Token token) {
    // ZoneScoped;
    lock_imports.lock();
    Import* imp = imports.get(fileId-1);
    lock_imports.unlock();
    Assert(imp);
    
    Chunk* chunk = nullptr;
    u32 cindex = 0;
    if(imp->chunk_indices.size() > 0) {
        cindex = imp->chunk_indices.last();
        
        lock_chunks.lock();
        chunk = chunks.get(cindex);
        lock_chunks.unlock();
        
        if(chunk->tokens.size()+1 >= TOKEN_ORIGIN_TOKEN_MAX) {
            chunk = nullptr; // chunk is full!
            cindex = 0;
        }
    }
    if(!chunk) {
        lock_chunks.lock();
        cindex = chunks.add(nullptr,&chunk);
        lock_chunks.unlock();
        
        chunk->chunk_index = cindex;
        chunk->import_id = fileId;
        imp->chunk_indices.add(cindex);
        imp->chunks.add(chunk);
    }
    
    chunk->tokens.add({});
    auto info = &chunk->tokens.last();
    auto from_info = getTokenInfo_unsafe(token);
    
    *info = *from_info;
    info->flags = (from_info->flags & (TOKEN_FLAG_HAS_DATA|TOKEN_FLAG_NULL_TERMINATED|TOKEN_FLAG_DOUBLE_QUOTED|TOKEN_FLAG_SINGLE_QUOTED));
    info->flags |= token.flags&(TOKEN_FLAG_NEWLINE|TOKEN_FLAG_SPACE);
    info->data_offset = 0;
    
    if(from_info->flags & TOKEN_FLAG_HAS_DATA) {
        const void* ptr=nullptr;
        u32 size = getDataFromToken(token, &ptr);
        
        info->data_offset = chunk->aux_used;
        if(from_info->flags & TOKEN_FLAG_NULL_TERMINATED) {
            chunk->alloc_aux_space(size + 2);
            chunk->aux_data[info->data_offset] = size;
            memcpy(chunk->aux_data + info->data_offset+1, ptr, size);
            chunk->aux_data[info->data_offset + 1 + size] = '\0';
        } else {
            chunk->alloc_aux_space(size + 1);
            chunk->aux_data[info->data_offset] = size;
            memcpy(chunk->aux_data + info->data_offset+1, ptr, size);
        }
    }

    Token tok{};
    tok.type = info->type;
    tok.flags = info->flags;
    tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
    return tok;
}
Token Lexer::appendToken(u32 fileId, TokenType type, u32 flags, u32 line, u32 column){
    // ZoneScoped;
    Assert(flags < 0x10000 && line < 0x10000 && column < 0x10000);

    // Make sure we don't add an identifier that is quoted.
    // shouldn't happen, bug in the code if it happens.
    Assert(type != TOKEN_IDENTIFIER || !(flags & (TOKEN_FLAG_DOUBLE_QUOTED|TOKEN_FLAG_SINGLE_QUOTED)));

    lock_imports.lock();
    Import* imp = imports.get(fileId-1);
    lock_imports.unlock();
    Assert(imp);
    
    Chunk* chunk = nullptr;
    u32 cindex = 0;
    if(imp->chunk_indices.size() > 0) {
        cindex = imp->chunk_indices.last();
        
        lock_chunks.lock();
        chunk = chunks.get(cindex);
        lock_chunks.unlock();
        
        if(chunk->tokens.size()+1 >= TOKEN_ORIGIN_TOKEN_MAX) {
            chunk = nullptr; // chunk is full!
            cindex = 0;
        }
    }
    if(!chunk) {
        lock_chunks.lock();
        cindex = chunks.add(nullptr,&chunk);
        lock_chunks.unlock();
        
        chunk->chunk_index = cindex;
        chunk->import_id = fileId;
        imp->chunk_indices.add(cindex);
        imp->chunks.add(chunk);
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
    // ZoneScoped;
    u32 cindex;
    u32 tindex;
    decode_origin(token.origin, &cindex, &tindex);
    
    lock_chunks.lock();
    Chunk* chunk = chunks.get(cindex);
    lock_chunks.unlock();
    
    Assert(chunk);
    if(!chunk) return nullptr;
    return chunk->tokens.getPtr(tindex);
}
// lexer::Token Lexer::Import::getToken(u32 token_index_into_import) {
//     Assert(false); // incomplete, chunks pointers are not added to import.chunks
//     u32 fcindex;
//     u32 tindex;
//     Lexer::decode_import_token_index(token_index_into_import, &fcindex, &tindex);
    
//     Assert(chunks.size() > fcindex);
//     Chunk* chunk = chunks.get(fcindex);
//     Assert(chunk);
    
//     Token out{};
//     auto info = chunk->tokens.getPtr(tindex);
//     if(!info) {
//         out.type = TOKEN_EOF;
//         return out;
//     }
    
//     out.flags = info->flags;
//     out.type = info->type;
//     out.origin = encode_origin(chunk->chunk_index,tindex);
//     #ifdef LEXER_DEBUG_DETAILS
//     if(out.flags & TOKEN_FLAG_HAS_DATA) {
//         u32 str_len = getStringFromToken(out,&out.s);
//     } else if(out.type < 256) {
//         out.c = (char)out.type;
//     }
//     #endif
//     return out;
// }
u32 Lexer::modifyTokenData(Token token, void* ptr, u32 size, bool with_null_termination) {
    ZoneScoped;
    u32 cindex;
    u32 tindex;
    decode_origin(token.origin, &cindex, &tindex);

    lock_chunks.lock();
    Chunk* chunk = chunks.get(cindex);
    lock_chunks.unlock();
    Assert(chunk);
    
    if(!chunk) return 0;
    auto info = chunk->tokens.getPtr(tindex);

    if(info->flags & TOKEN_FLAG_HAS_DATA) {
        u8 len = chunk->aux_data[info->data_offset];
        // If we had null termination first time we added data then we should have specified it now to.
        // probably bug otherwise.
        Assert((bool)with_null_termination == (bool)(info->flags&TOKEN_FLAG_NULL_TERMINATED));
        if(chunk->aux_used == info->data_offset + 1 + len + (info->flags&TOKEN_FLAG_NULL_TERMINATED?1:0)) {
            Assert(len+size < 0x100); // check integer overflow
            // Previous data exists at the end so we can just append the new data.
            chunk->alloc_aux_space(size);
            // chunk->auxiliary_data.resize(chunk->auxiliary_data.size() + size);
            chunk->aux_data[info->data_offset] = len + size;
            memcpy(chunk->aux_data + info->data_offset + 1 + len, ptr, size);
            if(with_null_termination) {
                chunk->aux_data[info->data_offset + 1 + len + size] = '\0';
            }
        } else {
            // We have move memory because previous data wasn't at the end and there is probably not any space.
            // TODO: Just moving data to the end will cause fragmentation. Should we perhaps not allow this
            //   or do we do something about it?
            Assert(false);
        }
    } else {
        Assert(size <= 0x100);
        info->flags |= TOKEN_FLAG_HAS_DATA;
        info->data_offset = chunk->aux_used;
        if(with_null_termination) {
            info->flags |= TOKEN_FLAG_NULL_TERMINATED;
            chunk->alloc_aux_space(size + 2);
            chunk->aux_data[info->data_offset] = size;
            memcpy(chunk->aux_data + info->data_offset+1, ptr, size);
            chunk->aux_data[info->data_offset + 1 + size] = '\0';
        } else {
            chunk->alloc_aux_space(size + 1);
            chunk->aux_data[info->data_offset] = size;
            memcpy(chunk->aux_data + info->data_offset+1, ptr, size);
        }
    }
    return 0; // TODO: Don't always return 0, what should we do?
}
u32 Lexer::createImport(const std::string& path, Import** file) {
    using namespace engone;
    
    lock_imports.lock();
    if(imports.getCount() >= 0xFFFF) {
        Assert(false);
        lock_imports.unlock();
        if(file)
            *file = nullptr;
        return 0;
    }
    Import* _imp;
    u32 file_index = imports.add(nullptr, &_imp);
    if(_imp) {
        _imp->path = path;
        _imp->file_id = file_index+1;
    }
    if(file)
        *file = _imp;
    lock_imports.unlock();

    if(file_index+1 >= 0x10000) {
        log::out << log::RED << "COMPILER BUG: The lexer design cannot support more than "<<0xFFFF<<" files.\n";
        // unsigned 16-bit integer can't support the file id
        lock_imports.lock();
        imports.removeAt(file_index);
        lock_imports.unlock();
        if(file)
            *file = nullptr;
        return 0;
    }

    return file_index+1;
}
void Lexer::destroyImport_unsafe(u32 import_id) {
    lock_imports.lock();
    Import* imp = imports.get(import_id-1);
    lock_imports.unlock();
    Assert(imp);
    
    lock_chunks.lock();
    for(int i=0;i<imp->chunk_indices.size();i++) {
        u32 cindex = imp->chunk_indices[i];
        Chunk* chunk = chunks.get(cindex);
        chunk->~Chunk();
        chunks.removeAt(cindex);
    }
    lock_chunks.unlock();
    
    imp->~Import();
    
    lock_imports.lock();
    imports.removeAt(import_id-1);
    lock_imports.unlock();
}
bool Lexer::equals_identifier(Token token, const char* str) {
    if(token.type == TOKEN_IDENTIFIER) {
        const char* tstr;
        u32 len = getStringFromToken(token,&tstr);
        return !strcmp(tstr,str);
    } else {
        // Assert(false);
        return false;   
    }
}
Lexer::TokenInfo* Lexer::getTokenInfoFromImport(u32 fileid, u32 token_index_into_import) {
    static TokenInfo eof{TOKEN_EOF};
    // NOTE: We assume that the content inside the imports won't be modified.
    //   Otherwise, we would need to lock the content too: file->lock_chunk_indices.lock().
    lock_imports.lock();
    Import* file = imports.get(fileid-1);
    lock_imports.unlock();
    Assert(file);

    u32 fcindex,tindex;
    decode_import_token_index(token_index_into_import, &fcindex,&tindex);

    if(file->chunk_indices.size() <= fcindex) {
        return &eof;
    }

    u32 cindex = file->chunk_indices.get(fcindex);
    // NOTE: We assume that the chunk won't be modified. Only read.
    lock_chunks.lock();
    Chunk* chunk = chunks.get(cindex);
    lock_chunks.unlock();
    Assert(chunk);
    
    auto info = chunk->tokens.getPtr(tindex);
    if(!info)
        return &eof;
    return info;
}
Token Lexer::getTokenFromImport(u32 fileid, u32 token_index_into_file) {
    // ZoneScoped;
    Token out{};
    
    // NOTE: We assume that the content inside the imports won't be modified.
    //   Otherwise, we would need to lock the content too: file->lock_chunk_indices.lock().
    lock_imports.lock();
    Import* file = imports.get(fileid-1);
    lock_imports.unlock();
    Assert(file);

    u32 fcindex,tindex;
    decode_import_token_index(token_index_into_file, &fcindex,&tindex);

    if(file->chunk_indices.size() <= fcindex) {
        out.type = TOKEN_EOF;
        return out;
    }

    u32 cindex = file->chunk_indices.get(fcindex);
    // NOTE: We assume that the chunk won't be modified. Only read.
    lock_chunks.lock();
    Chunk* chunk = chunks.get(cindex);
    lock_chunks.unlock();
    Assert(chunk);
    
    auto info = chunk->tokens.getPtr(tindex);
    if(!info) {
        out.type = TOKEN_EOF;
        return out;
    }
    
    out.flags = info->flags;
    out.type = info->type;
    out.origin = encode_origin(cindex,tindex);
    #ifdef LEXER_DEBUG_DETAILS
    if(out.flags & TOKEN_FLAG_HAS_DATA) {
        u32 str_len = getStringFromToken(out,&out.s);
    } else if(out.type < 256) {
        out.c = (char)out.type;
    }
    #endif
    return out;
}
Lexer::FeedIterator Lexer::createFeedIterator(Token start_token, Token end_token) {
    FeedIterator iter{};

    u32 cindex,tindex;
    decode_origin(start_token.origin,&cindex,&tindex);

    Chunk* first_chunk = chunks.get(cindex);
    Assert(first_chunk);

    iter.file_id = first_chunk->import_id;
    Import* file = imports.get(iter.file_id-1);
    Assert(file);

    bool found_id = false;
    for(int i=0;i<file->chunk_indices.size();i++){
        if(file->chunk_indices[i] == cindex) {
            iter.file_token_index = encode_import_token_index(i,tindex);
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
        Assert(("COMPILER BUG: tokens must come from the same file when iterating", last_chunk->import_id == first_chunk->import_id));

        bool found_id = false;
         for(int i=0;i<file->chunk_indices.size();i++){
            if(file->chunk_indices[i] == cindex) {
                iter.end_file_token_index = encode_import_token_index(i,tindex);
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
        Token tok = getTokenFromImport(iterator.file_id, iterator.file_token_index);
        
        // TODO: Add indent spacing. We need the previous token and it's length which makes
        //   it a little tedious to implement.
        
        if(tok.type < TOKEN_TYPE_BEGIN) { // ascii
            // TODO: What if character is a control character? (value 0-31)
            ENSURE(1)
            *(buffer + written) = (char)tok.type;
            written++;
            iterator.char_index++;
        } else if(tok.type == TOKEN_LITERAL_STRING) {
            Assert(tok.flags & (TOKEN_FLAG_DOUBLE_QUOTED|TOKEN_FLAG_SINGLE_QUOTED));
            ENSURE(1)
            if(tok.flags & TOKEN_FLAG_SINGLE_QUOTED)
                *(buffer + written) = '\'';
            else
                *(buffer + written) = '"';
            written++;
            iterator.char_index++;
            
            const char* str;
            u8 real_len = getStringFromToken(tok,&str);
            
            if(iterator.char_index >= real_len + 1) { // +1 because "
                // we wrote the content last feed
                // len = 0;
            } else {
                u8 len = real_len - (iterator.char_index-1); // -1 because "
                str += (iterator.char_index-1);
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
            
            ENSURE(1)
            if(tok.flags & TOKEN_FLAG_SINGLE_QUOTED)
                *(buffer + written) = '\'';
            else
                *(buffer + written) = '"';
            iterator.char_index++;
            written++;
        } else if(tok.type == TOKEN_IDENTIFIER) {
            const char* str;
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
u32 Lexer::getStringFromToken(Token tok, const char** ptr) {
    return getDataFromToken(tok, (const void**)ptr);
}
std::string Lexer::getStdStringFromToken(Token tok) {
    const void* ptr;
    u32 len = getDataFromToken(tok, &ptr);
    return std::string((const char*)ptr, len);
}
u32 Lexer::getDataFromToken(Token tok, const void** ptr){
    // ZoneScoped;
    auto info = getTokenInfo_unsafe(tok);
    Assert(info->flags & TOKEN_FLAG_HAS_DATA);
    u32 cindex,tindex;
    decode_origin(tok.origin,&cindex,&tindex);
    lock_chunks.lock();
    Chunk* chunk = chunks.get(cindex);
    lock_chunks.unlock();

    *ptr = chunk->aux_data + info->data_offset + 1;
    u8 len = chunk->aux_data[info->data_offset];
    return len;
}
void Lexer::print(u32 fileid) {
    using namespace engone;
    lock_imports.lock();
    Import* imp = imports.get(fileid-1);
    lock_imports.unlock();
    Assert(imp);
    
    if(imp->chunk_indices.size()==0)
        return;
    u32 cindex = imp->chunk_indices.last();
    
    lock_chunks.lock();
    Chunk* last = chunks.get(cindex);
    lock_chunks.unlock();

    FeedIterator iter{};
    iter.file_id = fileid;
    iter.file_token_index = 0;
    // We don't encode_file_token_index because integer overflow. If you know how not to create a bug then
    // you are free to use the encode function instead.
    iter.end_file_token_index = (imp->chunk_indices.size()-1) * 0x10000 + last->tokens.size();

    char buffer[256];
    u32 written = 0;
    while((written = feed(buffer,sizeof(buffer),iter))) {
        log::out.print(buffer,written);
    }
}
std::string Lexer::tostring(Token token) {
    auto iter = createFeedIterator(token);
    std::string out{};
    out.resize(15);
    char buffer[256];
    int written = 0;
    while((written = feed(buffer,sizeof buffer,iter))) {
        out.append(buffer,written);
    }
    return out;
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
    Assert(chunk_index < TOKEN_ORIGIN_CHUNK_MAX);
    Assert(tiny_token_index < TOKEN_ORIGIN_TOKEN_MAX);
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
u32 Lexer::encode_import_token_index(u32 file_chunk_index, u32 tiny_token_index){
    return encode_origin(file_chunk_index,tiny_token_index);
}
void Lexer::decode_import_token_index(u32 token_index_into_file, u32* file_chunk_index, u32* tiny_token_index) {
    return decode_origin(TokenOrigin{token_index_into_file}, file_chunk_index, tiny_token_index);
}
}