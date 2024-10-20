
#include "BetBat/Lexer.h"
#include "BetBat/Util/Perf.h"

namespace lexer {
u32 Lexer::tokenize(const std::string& path, u32 existing_import_id){
    u64 size = 0;
    char* buffer = nullptr;
    
    VirtualFile* vfile = findVirtualFile(path);
    
    if(vfile) {
        size = vfile->builder.size();
        buffer = vfile->builder.data();
    } else {
        auto file = engone::FileOpen(path, engone::FILE_READ_ONLY, &size);
        if(!file) return 0;
        defer {
            if(file)
                engone::FileClose(file);
        };

        // TODO: Reuse buffers
        buffer = TRACK_ARRAY_ALLOC(char, size);
        if(!buffer) return 0;

        u64 readBytes = engone::FileRead(file, buffer, size);
        Assert(readBytes != -1); // TODO: Handle this better
        Assert(readBytes == size);

        engone::FileClose(file); // close file as soon as possible so that other code can read or write to it if they want
        file = {};
    }
        
    u32 file_id = tokenize(buffer,size,path, existing_import_id);

    if(!vfile && buffer) {
        TRACK_ARRAY_FREE(buffer, char, size);
    }

    return file_id;
}
u32 Lexer::tokenize(const std::string& text, const std::string& path_name, u32 existing_import_id, int line, int column){
    return tokenize(text.c_str(), text.length(), path_name, existing_import_id, line, column);
}
u32 Lexer::tokenize(const char* text, u64 length, const std::string& path_name, u32 existing_import_id, int inherit_line, int inherit_column){
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

    lexer_import->last_modified_time = 0;
    FileLastWriteSeconds(path_name, &lexer_import->last_modified_time);
    // log::out << path_name << " " << lexer_import->last_modified_time<<"\n";

    // TODO: Optimize by keeping traack of chunks ids and their pointers here instead of using
    //  appendToken which queries file and chunk id every time.


    // Token token = {};
    // int token_line = 0;
    // int token_column = 0;
    
    int str_start = 0;
    int str_end = 0;
    
    TokenInfo* prev_token = nullptr; // quite useful
    TokenInfo* new_tokens = nullptr;
    TokenSource* new_source_tokens = nullptr;
    int new_tokens_len = 0;
    Chunk* last_chunk = nullptr;
    
    {
        lock_chunks.lock();
        int cindex = addChunk(&last_chunk);
        // int cindex = chunks.add(nullptr,&last_chunk);
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
            int cindex = addChunk(&last_chunk);
            //  chunks.add(nullptr,&last_chunk);
            lock_chunks.unlock();
            
            last_chunk->chunk_index = cindex;
            last_chunk->import_id = file_id;
            lexer_import->chunk_indices.add(cindex);
            lexer_import->chunks.add(last_chunk);
            
            last_chunk->tokens.reserve(TOKEN_ORIGIN_TOKEN_MAX);
            last_chunk->sources.reserve(TOKEN_ORIGIN_TOKEN_MAX);
        } else {
            int prev_index=0;
            if(prev_token)
                prev_index = prev_token - last_chunk->tokens.data(); 
            last_chunk->tokens.reserve(TOKEN_ORIGIN_TOKEN_MAX);
            last_chunk->sources.reserve(TOKEN_ORIGIN_TOKEN_MAX);
            if(prev_token)
                prev_token = prev_index + last_chunk->tokens.data(); 
        }
        // we can't fit all N requested tokens so we clamp
        if(last_chunk->tokens.used + n > TOKEN_ORIGIN_TOKEN_MAX) {
            n = TOKEN_ORIGIN_TOKEN_MAX - last_chunk->tokens.used;   
        }
        
        new_tokens = last_chunk->tokens.data() + last_chunk->tokens.used;
        new_source_tokens = last_chunk->sources.data() + last_chunk->tokens.used;
        last_chunk->tokens.used += n;
        last_chunk->sources.used += n;
        new_tokens_len = n;
        
        *new_tokens = {};
        *new_source_tokens = {};
    };
    
    reserv_tokens(); // make sure we start with a token so that the lexer doesn't have to check if a token has been requested or not everytime it needs to modify the current/next token
    auto INCREMENT_TOKEN = [&](){
        prev_token = new_tokens;
        new_tokens++;
        new_source_tokens++;
        new_tokens_len--;
        // log::out << "Tok "<<new_tokens->type << "\n";
        if(new_tokens_len == 0)
            reserv_tokens();
        *new_tokens = {};
        *new_source_tokens = {};
    };

    auto UPDATE_SOURCE = [&](int line, int column){
        // log::out << "Src "<<line << " "<<column << "\n";
        // new_tokens->line = line;
        // new_tokens->column = column;
        new_source_tokens->line = line;
        new_source_tokens->column = column;
    };
    
    auto APPEND_DATA = [&](const void* ptr, int size) {
        auto info = new_tokens;
        // TODO: You can probably optimize this function
        // if(info->type & (TOKEN_LITERAL_STRING|TOKEN_ANNOTATION|TOKEN_IDENTIFIER))
            info->flags |= TOKEN_FLAG_NULL_TERMINATED; // always null terminate, we atoi to convert literal integers to numbers and it needs null termination
        if(info->flags & TOKEN_FLAG_HAS_DATA) {
            int len = last_chunk->get_len(info->data_offset);
            // If we had null termination first time we added data then we should have specified it now to.
            // probably bug otherwise.
            if(last_chunk->aux_used == info->data_offset + AUX_SIZE_OF_LEN_PREFIX + len + (info->flags&TOKEN_FLAG_NULL_TERMINATED?1:0)) {
                Assert(len+size <= AUX_LEN_MAX); // check integer overflow
                // Previous data exists at the end so we can just append the new data.
                last_chunk->alloc_aux_space(size);
                // chunk->auxiliary_data.resize(chunk->auxiliary_data.size() + size);
                last_chunk->set_len(info->data_offset, len + size);
                memcpy(last_chunk->aux_data + info->data_offset + AUX_SIZE_OF_LEN_PREFIX + len, ptr, size);
                if(info->flags&TOKEN_FLAG_NULL_TERMINATED) {
                    last_chunk->aux_data[info->data_offset + AUX_SIZE_OF_LEN_PREFIX + len + size] = '\0';
                }
            } else {
                // We have move memory because previous data wasn't at the end and there is probably not any space.
                // TODO: Just moving data to the end will cause fragmentation. Should we perhaps not allow this
                //   or do we do something about it?
                Assert(false);
            }
        } else {
            Assert(size <= AUX_LEN_MAX);
            info->flags |= TOKEN_FLAG_HAS_DATA;
            info->data_offset = last_chunk->aux_used;
            if(info->flags&TOKEN_FLAG_NULL_TERMINATED) {
                info->flags |= TOKEN_FLAG_NULL_TERMINATED;
                last_chunk->alloc_aux_space(size + 1 + AUX_SIZE_OF_LEN_PREFIX);
                last_chunk->set_len(info->data_offset, size);
                memcpy(last_chunk->aux_data + info->data_offset + AUX_SIZE_OF_LEN_PREFIX, ptr, size);
                last_chunk->aux_data[info->data_offset + AUX_SIZE_OF_LEN_PREFIX + size] = '\0';
            } else {
                last_chunk->alloc_aux_space(size + AUX_SIZE_OF_LEN_PREFIX);
                last_chunk->set_len(info->data_offset, size);
                memcpy(last_chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
            }
        }
    };
    
    u16 line = inherit_line == 0 ? 1 : inherit_line; // TODO: TestSuite needs functionality to set line and column through function arguments.
    u16 column = inherit_column == 0 ? 1 : inherit_column;
    
    bool inQuotes = false;
    bool isSingleQuotes = false;
    bool isRawString = false;
    bool inComment = false;
    bool inEnclosedComment = false;
    int nested_comment_depth = 0;
    
    const char* raw_str_beg = "strbeg";
    int raw_str_beg_len = strlen(raw_str_beg);
    const char* raw_str_end = "strend";
    int raw_str_end_len = strlen(raw_str_end);
    
    bool isNumber = false;
    bool isAlpha = false; // alpha and then alphanumeric
    bool isDecimal = false;

    bool inHexidecimal = false;
    bool inBinary = false;
    bool inOctal = false;
    // int commentNestDepth = 0; // C doesn't have this and it's not very good. /*/**/*/
    
    bool canBeDot = false; // used to deal with decimals (eg. 255.92) as one token
    
    // preprocessor stuff
    bool foundHashtag=false;

    bool foundNonSpaceOnLine = false;
    
    // Token quote_token{};

    #define IS_CHR_SPACE(C) (C == ' ' || C == '\t')

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
                        foundNonSpaceOnLine = false;
                    }
                    lexer_import->lines++;
                    update_flags(TOKEN_FLAG_NEWLINE);
                }

                if(chr=='*'&&nextChr=='/'){
                    index++;
                    nested_comment_depth--;
                    if(nested_comment_depth == 0) {
                        inComment=false;
                        inEnclosedComment=false;
                        // _TLOG(log::out << "// : End comment\n";)
                    }
                }
                if(chr=='/'&&nextChr=='*'){
                    nested_comment_depth++;
                    index++;
                }
            }else{
                // I tried to optimize comment parsing at one point but it wasn't any faster.
                // Also, comments is not the slow part of the compiler so please don't waste time optimizing them.
                if(chr=='\n'||index==length){
                    if(!foundNonSpaceOnLine)
                        lexer_import->blank_lines++;
                    lexer_import->lines++;
                    foundNonSpaceOnLine=false;
                    // if(outStream->length()!=0)
                    //     outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
                    update_flags(TOKEN_FLAG_NEWLINE);
                    inComment=false;
                    // _TLOG(log::out << "// : End comment\n";)
                }
            }
            continue;
        } else if(inQuotes) {
            if(isRawString) {
                int head = index - 1;
                if(chr == ' ' || chr == '\t' || chr == '\n' || chr == '\r') {
                    while(head < length) {
                        char chr = text[head];
                        head++;
                        if(chr == ' ' || chr == '\t' || chr == '\n' || chr == '\r') { 
                            continue;
                        }
                        head--;
                        break;
                    }
                }
                
                if(text[head] == '@') {
                    head++;
                    if(length - head >= raw_str_end_len) {
                        bool is_end = true;
                        for(int i=0;i<raw_str_end_len;i++) {
                            if(raw_str_end[i] != text[head + i]) {
                                is_end = false;
                                break;
                            }
                        }
                        if(is_end) {
                            // log::out << line <<  "\n";
                            // index--;
                            while(index < length) {
                                char chr = text[index];
                                index++;
                                if(chr == ' ' || chr == '\r') {
                                    column++;
                                    continue;
                                }
                                if (chr == '\t') {
                                    column += 4;
                                    continue;
                                }
                                if (chr == '\n') { 
                                    column = 1;
                                    line++;
                                    continue;
                                }
                                break;
                            }
                            // log::out << line <<  "\n";
                            
                            index += raw_str_end_len;
                            column += raw_str_end_len;
                            inQuotes=false;
                            new_tokens->flags |= TOKEN_FLAG_DOUBLE_QUOTED;
                            INCREMENT_TOKEN();
                            str_start = 0;
                            str_end = 0;
                            continue;
                        }
                    }
                }
                APPEND_DATA(&chr,1);
            } else if((!isSingleQuotes && chr == '"') || 
                (isSingleQuotes && chr == '\'')){
                // Stop reading string token
                inQuotes=false;
                
                // new_tokens->flags = 0;
                if(nextChr=='\n')
                    new_tokens->flags |= TOKEN_FLAG_NEWLINE;
                else if(IS_CHR_SPACE(nextChr))
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
        if(chr == '@') {
            if(length - index >= raw_str_beg_len) {
                bool is_beg = true;
                for(int i=0;i<raw_str_beg_len;i++) {
                    if(raw_str_beg[i] != text[index + i]) {
                        is_beg = false;
                        break;
                    }
                }
                if(is_beg) {
                    isQuotes = true;
                    index  += raw_str_beg_len;
                    column += raw_str_beg_len;
                    while(true) { // skip whitespace
                        char chr = text[index];
                        index ++;
                        if(chr == ' ' || chr == '\r') {
                            column++;
                            continue;
                        }
                        if(chr == '\t') {
                            column+=4;
                            continue;
                        }
                        if(chr == '\n') {
                            column=1;
                            line++;
                            break;
                        }
                        index--;
                        break;
                    }
                }
            }
        }
        if(!isDelim && !isComment){
            foundNonSpaceOnLine = true;
        }
        if(chr == '\n' || index == length) {
            lexer_import->lines++;
            if(foundNonSpaceOnLine){
                foundNonSpaceOnLine = false;
            } else
                lexer_import->blank_lines++;
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
            isDecimal=true;
        }
        if(!isDelim&&!isSpecial&&!isQuotes&&!isComment){
            if(str_start != str_end){
                _TLOG(log::out << "-";)
            }
            _TLOG(log::out << chr;)
            if(str_start == str_end){
                str_start = index-1;
                str_end = str_start;
                UPDATE_SOURCE(ln,col);
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
                    if(chr == '0' && nextChr == 'b') {
                        inBinary = true;
                    }
                    if(chr == '0' && nextChr == 'o') {
                        inOctal = true;
                    }
                } else {
                    canBeDot = false;
                    isAlpha = true;
                }
            }
            if(!(chr>='0'&&chr<='9') && chr!='.') // TODO: how does it work with hexidecimals?
                canBeDot=false;
            // if(!isNumber || chr!='_') {
            // if(!isNumber) {
                // outStream->addData(chr);
            str_end++;
                // token.length++;
            // }
        }
        
        bool nextLiteralSuffix = false;
        // {
        //     char tmp = nextChr | 32;
        //     if(isNumber && isDecimal && !inHexidecimal && !inBinary && !inOctal) {
        //         nextLiteralSuffix = !inHexidecimal && !inBinary && !inOctal && isNumber && ((tmp>='a'&&tmp<='z') || nextChr == '_');
        //     }
        // }
        if(str_start != str_end && (isDelim || isQuotes || isComment || isSpecial || nextLiteralSuffix || index==length)){
            new_tokens->flags = 0;
            // TODO: is checking line feed necessary? line feed flag of last token is set further up.
            if(chr=='\n')
                new_tokens->flags |= TOKEN_FLAG_NEWLINE;
            else if(IS_CHR_SPACE(chr))
                new_tokens->flags |= TOKEN_FLAG_SPACE;
            // if(isAlpha)
            //     token.flags |= TOKEN_ALPHANUM;
            // if(isNumber)
            //     token.flags |= TOKEN_NUMERIC;
            // _TLOG(log::out << " : Add " << token << "\n";)
            
            bool has_data = true;
            if(isDecimal)
                new_tokens->type = TOKEN_LITERAL_DECIMAL;
            else if(inHexidecimal)
                new_tokens->type = TOKEN_LITERAL_HEXIDECIMAL;
            else if(inBinary)
                new_tokens->type = TOKEN_LITERAL_BINARY;
            else if(inOctal)
                new_tokens->type = TOKEN_LITERAL_OCTAL;
            else if(isNumber) // should come after hexidecimal
                new_tokens->type = TOKEN_LITERAL_INTEGER;
            else {
                new_tokens->type = TOKEN_IDENTIFIER;
            
                StringView temp{text + str_start, str_end - str_start};
                // TODO: Optimize. For example, if first character isn't one of these 'tfsenu' then it's not a special token and we don't have to run all ifs.
                // log::out << "check " << temp << "\n";
                
                #define CASE(S, STR, TOK)  if(S-1 == temp.len && temp == (char*)((u8*)STR+1)) {\
                    new_tokens->type = TOK; has_data = false; }

                char f = *(text+str_start);
                temp.ptr++; // not need to check first character
                temp.len--;
                switch(f){
                    case 'n': {
                        CASE(4, "null",        TOKEN_NULL)
                        else CASE(9, "namespace",   TOKEN_NAMESPACE)
                        else CASE(6, "nameof",      TOKEN_NAMEOF)
                    } break;
                    case 't': {
                        CASE(4, "true",        TOKEN_TRUE)
                        else CASE(6, "typeid",      TOKEN_TYPEID)
                        else CASE(3, "try",      TOKEN_TRY)
                    } break;
                    case 'f': {
                        CASE(5, "false",       TOKEN_FALSE)
                        else CASE(3, "for",         TOKEN_FOR)
                        else CASE(2, "fn",          TOKEN_FUNCTION)
                        else CASE(7, "finally",          TOKEN_FINALLY)
                    } break;
                    case 'i': {
                        CASE(2, "if",          TOKEN_IF)
                    } break;
                    case 'e': {
                        CASE(4, "else",        TOKEN_ELSE)
                        else CASE(4, "enum",        TOKEN_ENUM)
                    } break;
                    case 'w': {
                        CASE(5, "while",       TOKEN_WHILE)
                    } break;
                    case 's': {
                        CASE(6, "switch",      TOKEN_SWITCH)
                        else CASE(6, "struct",      TOKEN_STRUCT)
                        else CASE(6, "sizeof",      TOKEN_SIZEOF)
                    } break;
                    case 'd': {
                        CASE(5, "defer",       TOKEN_DEFER)
                    } break;
                    case 'r': {
                        CASE(6, "return",      TOKEN_RETURN)
                    } break;
                    case 'b': {
                        CASE(5, "break",       TOKEN_BREAK)
                    } break;
                    case 'c': {
                        CASE(8, "continue",    TOKEN_CONTINUE)
                        CASE(5, "catch",    TOKEN_CATCH)
                    } break;
                    case 'u': {
                        CASE(5, "using",       TOKEN_USING)
                        else CASE(5, "union",       TOKEN_UNION)
                    } break;
                    case 'o': {
                        CASE(8, "operator",    TOKEN_OPERATOR)
                    } break;
                    case 'a': {
                        CASE(3, "asm",         TOKEN_ASM)
                    } break;
                    case '_': {
                        CASE(5, "_test",         TOKEN_TEST)
                    } break;
                    // Non-optimize version
                    // #define CASE(STR, TOK)  if(temp == STR) { new_tokens->type = TOK; has_data = false; }
                    // CASE("null",        TOKEN_NULL)
                    // else CASE("true",        TOKEN_TRUE)
                    // else CASE("false",       TOKEN_FALSE)
                    // else CASE("if",          TOKEN_IF)
                    // else CASE("else",        TOKEN_ELSE)
                    // else CASE("while",       TOKEN_WHILE)
                    // else CASE("for",         TOKEN_FOR)
                    // else CASE("switch",      TOKEN_SWITCH)
                    // else CASE("defer",       TOKEN_DEFER)
                    // else CASE("return",      TOKEN_RETURN)
                    // else CASE("break",       TOKEN_BREAK)
                    // else CASE("continue",    TOKEN_CONTINUE)
                    // else CASE("using",       TOKEN_USING)
                    // else CASE("struct",      TOKEN_STRUCT)
                    // else CASE("fn",          TOKEN_FUNCTION)
                    // else CASE("operator",    TOKEN_OPERATOR)
                    // else CASE("enum",        TOKEN_ENUM)
                    // else CASE("namespace",   TOKEN_NAMESPACE)
                    // else CASE("union",       TOKEN_UNION)
                    // else CASE("sizeof",      TOKEN_SIZEOF)
                    // else CASE("nameof",      TOKEN_NAMEOF)
                    // else CASE("typeid",      TOKEN_TYPEID)
                    // else CASE("asm",         TOKEN_ASM)
                    #undef CASE
                }
            }
            // if(nextLiteralSuffix) {
            //     index++;
            // }
            if(has_data) {
                APPEND_DATA(text+str_start, str_end - str_start);
            }
            INCREMENT_TOKEN();
            
            canBeDot=false;
            isNumber=false;
            isAlpha=false;
            isDecimal=false;
            inHexidecimal = false;
            inBinary = false;
            inOctal = false;

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
                nested_comment_depth=1;
            }
            UPDATE_SOURCE(ln,col);
            lexer_import->comment_lines++;
            index++; // skip the next slash
            // _TLOG(log::out << "// : Begin comment\n";)
            continue;
        } else if(isQuotes){
            inQuotes = true;
            isSingleQuotes = chr == '\'';
            isRawString = chr == '@';
            // str_start = index-1+1;
            // str_end = index-1+1; // we haven't parsed a character in the quotes yet
            // token.str = (char*)outStream->tokenData.used;
            // token.length = 0;
            UPDATE_SOURCE(ln,col);
            
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
                int anot_line = ln;
                int anot_column = col;
                int anot_end = index;
                
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
                    char tmp = chr | 32;
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
                    anot_end++;
                    if (index==length){
                        break;
                    }
                }
                
                new_tokens->type = TOKEN_ANNOTATION;
                new_tokens->flags = anot.flags;
                UPDATE_SOURCE(anot_line, anot_column);
                
                APPEND_DATA(text+anot_start, anot_end - anot_start);
                
                INCREMENT_TOKEN();
                continue;
            }
            
            // this scope only adds special token
            _TLOG(log::out << chr;)
            str_start = index-1;
            str_end = str_start+1;
            bool has_data = true;

            if(chr == ':' && nextChr == ':') {
                index++;
                column++;
                new_tokens->type = TOKEN_NAMESPACE_DELIM;
                has_data = false;
            } else if(chr=='.'&&nextChr=='.'&&nextChr2=='.'){
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
                (chr=='.'&&nextChr=='.')||
                (chr=='|'&&nextChr=='|')
                ){
                index++;
                column++;
                str_end++;
                _TLOG(log::out << nextChr;)
            }
            if(index<length)
                nextChr = text[index]; // code below needs the updated nextChr
            else
                nextChr = 0;
            
            UPDATE_SOURCE(ln,col);

            if(nextChr =='\n')
                new_tokens->flags = TOKEN_FLAG_NEWLINE;
            else if(IS_CHR_SPACE(nextChr))
                new_tokens->flags = TOKEN_FLAG_SPACE;
            // _TLOG(log::out << " : Add " << token <<"\n";)
            _TLOG(log::out << " : special\n";)
            
            if(!has_data) {
                INCREMENT_TOKEN(); 
            } else if(str_end - str_start == 1) {
                new_tokens->type = (TokenType)chr;
                INCREMENT_TOKEN();
            } else {
                new_tokens->type = TOKEN_IDENTIFIER;
                APPEND_DATA(text+str_start, str_end - str_start);
                INCREMENT_TOKEN();
            }
                
            str_start = 0;
            str_end = 0;
        }
    }
    if(!foundHashtag){
        // preprocessor not necessary, save time by not running it.
        // If only #import were used (no #macro, no #if) then
        // we can handle that here and save time while using imports.
        // saving time when no imports are used doesn't help much.
        // _TLOG(log::out<<log::LIME<<"Couldn't find #. Disabling preprocessor.\n";)
    }
    
    // _LOG(LOG_IMPORTS,
    //     for(auto& str : outStream->importList){
    //         engone::log::out << log::LIME<<" #import '"<<str.name << "' as "<<str.as<<"'\n";
    //     }   
    // )

    // README: Seemingly strange tokens can appear if you miss a quote somewhere.
    //  This is not a bug. It happens since quoted tokens are allowed across lines.

    // log::out << "Tokens/Sources: "<<last_chunk->tokens.size() << "/" <<last_chunk->sources.size() << "\n";
    
    Assert(last_chunk->tokens.used >= new_tokens_len); // I have a feeling there is a bug here.
    last_chunk->tokens.used -= new_tokens_len; // we didn't use all tokens we requested so we "give them back"
    last_chunk->sources.used -= new_tokens_len;
    
    
    // TokenInfo* last_token=nullptr;
    // if(last_chunk->tokens.size() > 0)
    //     last_token = &last_chunk->tokens.size();
    TokenSource* last_token=nullptr;
    if(last_chunk->sources.size() > 0)
        last_token = &last_chunk->sources.last();
    
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
    
    Assert(last_chunk->tokens.size() == last_chunk->sources.size());

Tokenize_END:
    // log::out << "Last: "<<outStream->get(outStream->length()-1)<<"\n";
    return file_id;
}

void Lexer::appendToken(Import* imp, Token tok, StringView* string) {
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
        int cindex = addChunk(&chunk);
        // cindex = chunks.add(nullptr,&chunk);
        lock_chunks.unlock();
        
        chunk->chunk_index = cindex;
        chunk->import_id = imp->file_id;
        imp->chunk_indices.add(cindex);
        imp->chunks.add(chunk);
    }
    
    chunk->tokens.add({});
    chunk->sources.add({});

    auto info = &chunk->tokens.last();
    auto& source = chunk->sources.last();
    
    info->type = tok.type;
    info->c_type = tok.c_type;
    info->flags = tok.flags;
    info->data_offset = 0;

    {
        u32 cindex, tindex;
        decode_origin(tok.origin, &cindex, &tindex);
        Chunk* fc = getChunk_unsafe(cindex);
        source = fc->sources[tindex];
    }
    if(tok.flags & TOKEN_FLAG_HAS_DATA) {
        Assert(string);
        const void* ptr=string->ptr;
        u32 size = string->len;
        
        info->data_offset = chunk->aux_used;
        if(tok.flags & TOKEN_FLAG_NULL_TERMINATED) {
            chunk->alloc_aux_space(size + AUX_SIZE_OF_LEN_PREFIX + 1);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
            chunk->aux_data[info->data_offset + AUX_SIZE_OF_LEN_PREFIX + size] = '\0';
        } else {
            chunk->alloc_aux_space(size + AUX_SIZE_OF_LEN_PREFIX);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
        }
    }
    Assert(cindex != 0);
    // Token tok{};
    // tok.type = info->type;
    // tok.flags = info->flags;
    // tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
    // return tok;
}
Token Lexer::appendToken(Import* imp, Token token, bool compute_source, StringView* string, int inherited_column, int backup_line, int backup_column) {
    // ZoneScoped;
    // lock_imports.lock();
    // Import* imp = imports.get(fileId-1);
    // lock_imports.unlock();
    Assert(imp);
    
    TokenInfo* prev_token = nullptr;
    TokenSource* prev_source = nullptr;
    Token prev_tok = {};

    Chunk* chunk = nullptr;
    u32 cindex = 0;
    if(imp->chunk_indices.size() > 0) {
        cindex = imp->chunk_indices.last();
        
        lock_chunks.lock();
        chunk = _chunks.get(cindex);
        // chunk = chunks.get(cindex);
        lock_chunks.unlock();
        
        if(compute_source) {
            if(chunk->tokens.size()) {
                prev_token = &chunk->tokens.last();
                prev_source = &chunk->sources.last();
                prev_tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
            }
        }
        if(chunk->tokens.size()+1 >= TOKEN_ORIGIN_TOKEN_MAX) {
            chunk = nullptr; // chunk is full!
            cindex = 0;
        }
    }
    if(!chunk) {
        lock_chunks.lock();
        cindex = addChunk(&chunk);
        // cindex = chunks.add(nullptr,&chunk);
        lock_chunks.unlock();
        
        chunk->chunk_index = cindex;
        chunk->import_id = imp->file_id;
        imp->chunk_indices.add(cindex);
        imp->chunks.add(chunk);
    }

    int line = 1;
    int column = 1;
    if(prev_token) {
        column = prev_source->column;
        line = prev_source->line;
        if(prev_token->flags & TOKEN_FLAG_NEWLINE) {
            line++;
            if(inherited_column != 0)
                column = inherited_column;
        } else {
            if(prev_token->flags & TOKEN_FLAG_SPACE) {
                column++;
            }
            // Optimize?
            std::string name_of_prev = tostring(prev_tok);
            column += name_of_prev.size();
        }
    }
    
    chunk->tokens.add({});
    chunk->sources.add({});
    auto info = &chunk->tokens.last();
    auto& source = chunk->sources.last();
    auto from_info = getTokenInfo_unsafe(token);
    
    // *info = *from_info;
    // info->flags = (from_info->flags & ~TOKEN_FLAG_ANY_SUFFIX);
    // info->flags |= token.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
    info->type = token.type;
    info->flags = token.flags;
    info->data_offset = 0;

    if(prev_token || compute_source) {
        source.line = line;
        source.column = column;
    } else {
        // if(string) {
            // can't set source, token was created by preprocessor
            if(backup_line != 0) {
                source.line = backup_line;
                source.column = backup_column;
            }
        // }
         else {
            u32 cindex, tindex;
            decode_origin(token.origin, &cindex, &tindex);
            Chunk* fc = getChunk_unsafe(cindex);
            source = fc->sources[tindex];
        }
    }
    
    if(string) {
        Assert(token.flags & TOKEN_FLAG_HAS_DATA);
        Assert(string);
        const void* ptr=string->ptr;
        u32 size = string->len;
        
        info->data_offset = chunk->aux_used;
        if(token.flags & TOKEN_FLAG_NULL_TERMINATED) {
            chunk->alloc_aux_space(size + 1 + AUX_SIZE_OF_LEN_PREFIX);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
            chunk->aux_data[info->data_offset + AUX_SIZE_OF_LEN_PREFIX + size] = '\0';
        } else {
            chunk->alloc_aux_space(size + AUX_SIZE_OF_LEN_PREFIX);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
        }
    } else if(from_info->flags & TOKEN_FLAG_HAS_DATA) {
        const void* ptr=nullptr;
        u32 size = getDataFromToken(token, &ptr);
        
        info->data_offset = chunk->aux_used;
        if(from_info->flags & TOKEN_FLAG_NULL_TERMINATED) {
            chunk->alloc_aux_space(size + 1 + AUX_SIZE_OF_LEN_PREFIX);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
            chunk->aux_data[info->data_offset + AUX_SIZE_OF_LEN_PREFIX + size] = '\0';
        } else {
            chunk->alloc_aux_space(size + AUX_SIZE_OF_LEN_PREFIX);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
        }
    }

    Token tok{};
    tok.type = info->type;
    tok.flags = info->flags;
    tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
    return tok;
}
Token Lexer::appendToken_auto_source(Import* imp, TokenType type, u32 flags){
    // ZoneScoped;
    Assert(flags < 0x10000);
    // Assert(flags < 0x10000 && line < 0x10000 && column < 0x10000);

    // Make sure we don't add an identifier that is quoted.
    // shouldn't happen, bug in the code if it happens.
    Assert(type != TOKEN_IDENTIFIER || !(flags & (TOKEN_FLAG_DOUBLE_QUOTED|TOKEN_FLAG_SINGLE_QUOTED)));

    // lock_imports.lock();
    // Import* imp = imports.get(fileId-1);
    // lock_imports.unlock();
    Assert(imp);
    
    TokenInfo* prev_token = nullptr;
    TokenSource* prev_source = nullptr;
    Token prev_tok = {};
    Chunk* chunk = nullptr;
    u32 cindex = 0;
    if(imp->chunk_indices.size() > 0) {
        cindex = imp->chunk_indices.last();
        
        lock_chunks.lock();
        chunk = _chunks.get(cindex);
        lock_chunks.unlock();
        
        prev_token = &chunk->tokens.last();
        prev_source = &chunk->sources.last();
        prev_tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
        if(chunk->tokens.size()+1 >= TOKEN_ORIGIN_TOKEN_MAX) {
            chunk = nullptr; // chunk is full!
            cindex = 0;
        }
    }
    if(!chunk) {
        lock_chunks.lock();
        cindex = addChunk(&chunk);
        // cindex = chunks.add(nullptr,&chunk);
        lock_chunks.unlock();
        
        chunk->chunk_index = cindex;
        chunk->import_id = imp->file_id;
        imp->chunk_indices.add(cindex);
        imp->chunks.add(chunk);
    }
    
    int line = 1;
    int column = 1;
    // This function appends a new token from the preprocessor.
    // The token has no source information because it does not belong
    // to the original file. Therefore, we inherit the source information
    // from previous token. If we don't have a previous token then
    // we can't inherit source information and so the token will
    // be put at line 1. Which makes no sense when the user
    // looks at line 1. So we assert. - Emarioo, 2024-04-18
    Assert(prev_token);
    if(prev_token) {
        column = prev_source->column;
        line = prev_source->line;
        if(prev_token->flags & TOKEN_FLAG_NEWLINE) {
            line++;
            column = 1;
        } else {
            if(prev_token->flags & TOKEN_FLAG_SPACE) {
                column++;
            }
            // Optimize?
            std::string name_of_prev = tostring(prev_tok);
            column += name_of_prev.size();
        }
    }

    chunk->tokens.add({});
    chunk->sources.add({});
    auto& info = chunk->tokens.last();
    info.type = type;
    info.flags = flags;
    info.data_offset = 0;
    auto& src = chunk->sources.last();
    src.line = line;
    src.column = column;

    Token tok{};
    tok.type = type;
    tok.flags = flags;
    tok.origin = encode_origin(cindex, chunk->tokens.size()-1);
    return tok;
}
TokenInfo* Lexer::getTokenInfo_unsafe(TokenOrigin origin){
    // ZoneScoped;
    u32 cindex;
    u32 tindex;
    decode_origin(origin, &cindex, &tindex);
    
    lock_chunks.lock();
    Chunk* chunk = _chunks.get(cindex);
    lock_chunks.unlock();
    
    Assert(chunk);
    if(!chunk) return nullptr;
    return chunk->tokens.getPtr(tindex);
}
TokenInfo* Lexer::getTokenInfo_unsafe(SourceLocation location){
    // ZoneScoped;
    u32 cindex;
    u32 tindex;
    decode_origin(location.tok.origin, &cindex, &tindex);
    
    lock_chunks.lock();
    Chunk* chunk = _chunks.get(cindex);
    lock_chunks.unlock();
    
    Assert(chunk);
    if(!chunk) return nullptr;
    return chunk->tokens.getPtr(tindex);
}
TokenSource* Lexer::getTokenSource_unsafe(SourceLocation location){
    // ZoneScoped;
    u32 cindex;
    u32 tindex;
    decode_origin(location.tok.origin, &cindex, &tindex);
    
    lock_chunks.lock();
    Chunk* chunk = _chunks.get(cindex);
    lock_chunks.unlock();
    
    Assert(chunk);
    if(!chunk) return nullptr;
    return chunk->sources.getPtr(tindex);
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
    Chunk* chunk = _chunks.get(cindex);
    lock_chunks.unlock();
    Assert(chunk);
    
    if(!chunk) return 0;
    auto info = chunk->tokens.getPtr(tindex);

    if(info->flags & TOKEN_FLAG_HAS_DATA) {
        int len = chunk->get_len(info->data_offset);
        // If we had null termination first time we added data then we should have specified it now to.
        // probably bug otherwise.
        Assert((bool)with_null_termination == (bool)(info->flags&TOKEN_FLAG_NULL_TERMINATED));
        if(chunk->aux_used == info->data_offset + AUX_SIZE_OF_LEN_PREFIX + len + (info->flags&TOKEN_FLAG_NULL_TERMINATED?1:0)) {
            Assert(len+size <= AUX_LEN_MAX); // check integer overflow
            // Previous data exists at the end so we can just append the new data.
            chunk->alloc_aux_space(size);
            // chunk->auxiliary_data.resize(chunk->auxiliary_data.size() + size);
            chunk->set_len(info->data_offset, len + size);
            memcpy(chunk->aux_data + info->data_offset + AUX_SIZE_OF_LEN_PREFIX + len, ptr, size);
            if(with_null_termination) {
                chunk->aux_data[info->data_offset + AUX_SIZE_OF_LEN_PREFIX + len + size] = '\0';
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
            chunk->alloc_aux_space(size + 1 + AUX_SIZE_OF_LEN_PREFIX);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
            chunk->aux_data[info->data_offset + AUX_SIZE_OF_LEN_PREFIX + size] = '\0';
        } else {
            chunk->alloc_aux_space(size + AUX_SIZE_OF_LEN_PREFIX);
            chunk->set_len(info->data_offset, size);
            memcpy(chunk->aux_data + info->data_offset+AUX_SIZE_OF_LEN_PREFIX, ptr, size);
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
    Import* _imp = nullptr;
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
    Assert(false); // CAN'T ANYMORE BECAUSE WE USE DYNAMIC ARRAY INSTEAD OF BUCKET ARRAY!
    lock_imports.lock();
    Import* imp = imports.get(import_id-1);
    lock_imports.unlock();
    Assert(imp);
    
    lock_chunks.lock();
    for(int i=0;i<imp->chunk_indices.size();i++) {
        u32 cindex = imp->chunk_indices[i];
        Chunk* chunk = _chunks.get(cindex);
        chunk->~Chunk();
        _chunks.removeAt(cindex);
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
void Lexer::popTokenFromImport(Import* imp) {
    while(imp->chunks.size() > 0) {
        if(imp->chunks.last()->tokens.size() == 0) {
            u32 cindex = imp->chunk_indices.last();
            Chunk* chunk = _chunks.get(cindex);
            chunk->~Chunk();
            _chunks.removeAt(cindex);

            imp->chunks.pop();
            imp->chunk_indices.pop();
            continue;
        }
        auto chunk = imp->chunks.last();
        auto& info = chunk->tokens.last();
        // info.data_offset
        int len = chunk->get_len(info.data_offset);
        
        int size = AUX_SIZE_OF_LEN_PREFIX + len + (info.flags&TOKEN_FLAG_NULL_TERMINATED?1:0);
        if(chunk->aux_used == info.data_offset + size) {
            chunk->aux_used -= size;
        } else {
            Assert(false); // the math is wrong
        }

        imp->chunks.last()->tokens.pop();
        imp->chunks.last()->sources.pop();
        break;
    }
}
void Lexer::popMultipleTokensFromImport(Import* imp, int index_of_last_token_to_pop) {
    while(imp->chunks.size() > 0) {
        if(imp->chunks.last()->tokens.size() == 0) {
            u32 cindex = imp->chunk_indices.last();
            Chunk* chunk = _chunks.get(cindex);
            chunk->~Chunk();
            _chunks.removeAt(cindex);

            imp->chunks.pop();
            imp->chunk_indices.pop();
            continue;
        }
        int index = imp->chunks.last()->tokens.size()-1 + (imp->chunks.size()-1) * TOKEN_ORIGIN_TOKEN_MAX;
        if(index < index_of_last_token_to_pop) {
            break;
        }
        
        auto chunk = imp->chunks.last();
        auto& info = chunk->tokens.last();
        // info.data_offset
        u8 len = chunk->get_len(info.data_offset);
        int size = len + AUX_SIZE_OF_LEN_PREFIX + (info.flags&TOKEN_FLAG_NULL_TERMINATED?1:0);
        if(chunk->aux_used == info.data_offset + size) {
            chunk->aux_used -= size;
        } else {
            Assert(false); // the math is wrong
        }

        imp->chunks.last()->tokens.pop();
        imp->chunks.last()->sources.pop();
    }
}
TokenInfo* Lexer::getTokenInfoFromImport(u32 fileid, u32 token_index_into_import, TokenSource** src) {
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
    Chunk* chunk = _chunks.get(cindex);
    lock_chunks.unlock();
    Assert(chunk);
    
    auto info = chunk->tokens.getPtr(tindex);
    if(!info)
        return &eof;
    if(src)
        *src = chunk->sources.getPtr(tindex);
    return info;
}
Token Lexer::getTokenFromImport(u32 fileid, u32 token_index_into_file, StringView* string) {
    // ZoneScoped;
    Token out{};
    
    // NOTE: We assume that the content inside the imports won't be modified.
    //   Otherwise, we would need to lock the content too: file->lock_chunk_indices.lock().
    lock_imports.lock();
    Import* imp = imports.get(fileid-1);
    lock_imports.unlock();
    Assert(imp);

    u32 fcindex,tindex;
    decode_import_token_index(token_index_into_file, &fcindex,&tindex);

    if(imp->chunk_indices.size() <= fcindex) {
        // out.type = TOKEN_EOF;
        return imp->geteof();
    }

    u32 cindex = imp->chunk_indices.get(fcindex);
    // NOTE: We assume that the chunk won't be modified. Only read.
    lock_chunks.lock();
    Chunk* chunk = _chunks.get(cindex);
    lock_chunks.unlock();
    Assert(chunk);
    
    auto info = chunk->tokens.getPtr(tindex);
    if(!info) {
        // out.type = TOKEN_EOF;
        return imp->geteof();
    }

    if(string && (info->flags & lexer::TOKEN_FLAG_HAS_DATA)) {
        *string = chunk->get_string(info->data_offset);
        #ifdef LEXER_DEBUG_DETAILS
        out.s = chunk->get_data(info->data_offset);
        #endif
    }
    
    out.c_type = out.type;
    out.flags = info->flags;
    out.type = info->type;
    out.origin = encode_origin(cindex,tindex);
    return out;
}
Lexer::FeedIterator Lexer::createFeedIterator(Import* imp, u32 start, u32 end) {
    FeedIterator iter{};
    iter.file_id = imp->file_id;
    iter.file_token_index = start;
    iter.end_file_token_index = end;

    return iter;
}
Lexer::FeedIterator Lexer::createFeedIterator(Token start_token, Token end_token) {
    FeedIterator iter{};

    u32 cindex,tindex;
    decode_origin(start_token.origin,&cindex,&tindex);

    Chunk* first_chunk = _chunks.get(cindex);
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

        Chunk* last_chunk = _chunks.get(cindex);
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

const char* token_type_names[] {
    // "tok_eof",      // TOKEN_EOF,
    // "tok_id",       // TOKEN_IDENTIFIER,
    // "tok_anot",     // TOKEN_ANNOTATION,
    // "tok_lit_str",  // TOKEN_LITERAL_STRING,
    // "tok_lit_int",    // TOKEN_LITERAL_INTEGER,
    // "tok_lit_dec",    // TOKEN_LITERAL_DECIMAL,
    // "tok_lit_hex",    // TOKEN_LITERAL_HEXIDECIMAL,
    // "tok_lit_bin",    // TOKEN_LITERAL_BINARY,
    // "tok_lit_oct",    // TOKEN_LITERAL_OCTAL,

    "null", // TOKEN_NULL = TOKEN_KEYWORD_BEGIN,
    "true", // TOKEN_TRUE,
    "false", // TOKEN_FALSE,

    "sizeof", // TOKEN_SIZEOF,
    "nameof", // TOKEN_NAMEOF,
    "typeid", // TOKEN_TYPEID,

    "if", // TOKEN_IF,
    "else", // TOKEN_ELSE,
    "while", // TOKEN_WHILE,
    "for", // TOKEN_FOR,
    "switch", // TOKEN_SWITCH,
    "defer", // TOKEN_DEFER,
    "return", // TOKEN_RETURN,
    "break", // TOKEN_BREAK,
    "continue", // TOKEN_CONTINUE,

    "using", // TOKEN_USING,
    "struct", // TOKEN_STRUCT,
    "fn", // TOKEN_FUNCTION,
    "operator", // TOKEN_OPERATOR,
    "enum", // TOKEN_ENUM,
    "namespace", // TOKEN_NAMESPACE,
    "union", // TOKEN_UNION,
    "asm", // TOKEN_ASM,
    "_test", // TOKEN_TEST,
    "try", // TOKEN_TRY,
    "catch", // TOKEN_CATCH,
    "finally", // TOKEN_FINALLY,

    "::", // TOKEN_NAMESPACE_DELIM,

};
TokenType StringToTokenType(const char* str, int len) {
    

    return TOKEN_NONE;
}
bool Lexer::feed(FeedIterator& iterator, bool skipSuffix, bool apply_indent, bool no_quotes) {
    using namespace engone;
    Assert(iterator.file_id != 0);
    iterator.clear();

    int tokens_to_parse = iterator.end_file_token_index - iterator.file_token_index;

    // macros in case the implementation changes
    #define APPEND(C) iterator.append(C);
    #define APPENDS(SRC,N) iterator.append(SRC,N);
    
    if(iterator.file_token_index == iterator.end_file_token_index)
        return false;
    
    int prev_column = 1;
    while(true){
        if(iterator.file_token_index == iterator.end_file_token_index)
            return true;
        // TOOD: Refactor, getting token twice is dumb
        Token tok = getTokenFromImport(iterator.file_id, iterator.file_token_index);
        TokenSource* src;
        TokenInfo* token = getTokenInfoFromImport(iterator.file_id, iterator.file_token_index, &src);
        
        if(apply_indent && prev_column == 1) {
            // TODO: Macros mess up the indentation. The content of the macros are
            //   recomputed with new correct line/column information BUT any
            //   tokens after the macros will be incorrect. Those tokens will
            //   correctly map to the original file but will look strange when printed.
            //   This is currently solved by only allowing indentation on column 1.
            int indent = src->column - prev_column;
            for(int i=0;i<indent;i++) {
                APPEND(' ')
                prev_column++;
            }
        }
        
        int prev_written = iterator.len();
        
        if(tok.type < TOKEN_TYPE_BEGIN) { // ascii
            // TODO: What if character is a control character? (value 0-31)
            APPEND(tok.type)
        } else if(tok.type == TOKEN_LITERAL_STRING) {
            Assert(tok.flags & (TOKEN_FLAG_DOUBLE_QUOTED|TOKEN_FLAG_SINGLE_QUOTED));
            if(!no_quotes) {
                if(tok.flags & TOKEN_FLAG_SINGLE_QUOTED)
                    APPEND('\'')
                else
                    APPEND('"')
            }
            
            const char* str;
            u8 len = getStringFromToken(tok,&str);
            
            for(int i=0;i<len;i++) {
                char c = str[i];

                if(c == '\\') { // escape character?
                    APPEND('\\')
                    APPEND('\\')
                } else if(c == '\0') { // escape character?
                    APPEND('\\')
                    APPEND('0')
                } else if(c == '\t') {
                    APPEND('\\')
                    APPEND('t')
                } else if(c == '\n') {
                    APPEND('\\')
                    APPEND('n')
                } else if(c == '\r') {
                    APPEND('\\')
                    APPEND('r')
                } else if(c == '\x1b') { // escape character?
                    APPEND('\\')
                    APPEND('e')
                } else if(c < 32){
                    APPEND('?')
                } else {
                    APPEND(c)
                }
            }
            if(!no_quotes) {
                if(tok.flags & TOKEN_FLAG_SINGLE_QUOTED)
                    APPEND('\'')
                else
                    APPEND('"')
            }
        } else if(tok.type == TOKEN_ANNOTATION) {
            APPEND('@')
            const char* str;
            u8 len = getStringFromToken(tok,&str);
            
            APPENDS(str,len)
        } else if(tok.type == TOKEN_IDENTIFIER || tok.type == TOKEN_LITERAL_INTEGER || tok.type == TOKEN_LITERAL_HEXIDECIMAL || tok.type == TOKEN_LITERAL_BINARY ||tok.type == TOKEN_LITERAL_OCTAL || tok.type == TOKEN_LITERAL_DECIMAL) {
            const char* str;
            u8 len = getStringFromToken(tok,&str);
            APPENDS(str,len)
        } else if(tok.type >= TOKEN_KEYWORD_BEGIN) {
            auto str = TOK_KEYWORD_NAME(tok.type);
            u8 len = strlen(str);
            APPENDS(str,len)
        } else {
            APPENDS("<EOF>",5)
        }
        // TODO: Don't do suffix if we processed the last token or if suffix should be skipped (skipSuffix)
        
        // if(!(skipSuffix && tokens_to_parse == 1)) {
        if(!(skipSuffix && iterator.file_token_index+1 == iterator.end_file_token_index)) {
            if((tok.flags & TOKEN_FLAG_NEWLINE)) {
            // if((tok.flags & TOKEN_FLAG_NEWLINE) && (!skipSuffix || iterator.file_token_index+1 != iterator.end_file_token_index)) {
                APPEND('\n')
                prev_column = 1;
            } else {
                if((tok.flags & TOKEN_FLAG_SPACE)) {
                    APPEND(' ')
                }
                prev_column += iterator.len() - prev_written;
            }
        }
        iterator.file_token_index++;
    }
    #undef ENSURE
    #undef APPEND
    #undef APPENDS
    return true;
}
Token Import::geteof(){
    Token out{};
    out.type = TOKEN_EOF;
    out.flags = 0;
    if(chunk_indices.size()>0) {
        out.origin = Lexer::encode_origin(chunk_indices.last(), chunks.last()->tokens.size());
    }
    return out;
}
u32 Lexer::getStringFromToken(Token tok, const char** ptr) {
    return getDataFromToken(tok, (const void**)ptr);
}
    const char* ptr;
std::string Lexer::getStdStringFromToken(Token tok) {
    u32 len = getStringFromToken(tok, &ptr);
    return std::string((const char*)ptr, len);
}
u32 Lexer::getDataFromToken(Token tok, const void** ptr){
    // ZoneScoped;
    auto info = getTokenInfo_unsafe(tok);
    if(0 == (info->flags & TOKEN_FLAG_HAS_DATA)) {
        static const char* empty = "";
        if(ptr)
            *ptr = empty;
        return 0;
    }
    // Assert(info->flags & TOKEN_FLAG_HAS_DATA);
    u32 cindex,tindex;
    decode_origin(tok.origin,&cindex,&tindex);
    lock_chunks.lock();
    Chunk* chunk = _chunks.get(cindex);
    lock_chunks.unlock();

    *ptr = chunk->get_data(info->data_offset);
    int len = chunk->get_len(info->data_offset);
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
    Chunk* last = _chunks.get(cindex);
    lock_chunks.unlock();

    FeedIterator iter{};
    iter.file_id = fileid;
    iter.file_token_index = 0;
    // We don't encode_file_token_index because integer overflow. If you know how not to create a bug then
    // you are free to use the encode function instead.
    iter.end_file_token_index = (imp->chunk_indices.size()-1) * TOKEN_ORIGIN_TOKEN_MAX + last->tokens.size();

    while(feed(iter, false, true)) {
        log::out.print(iter.data(),iter.len());
    }
}
std::string Lexer::tostring(Token token, bool no_quotes) {
    auto iter = createFeedIterator(token);
    std::string out{};
    // TODO: Optimize
    while(feed(iter, true, false, no_quotes)) {
        out.append(iter.data(),iter.len());
    }
    return out;
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

double ConvertDecimal(const StringView& view, bool* double_suffix) {
    Assert(view.ptr[view.len] == '\0');
    double num = atof(view.ptr);
    if (double_suffix){
        if (view.ptr[view.len-1] == 'd')
            *double_suffix = true;
        else
        *double_suffix = false;
    }
    return num;
}
int ConvertInteger(const StringView& view) {
    Assert(view.ptr[view.len] == '\0');
    int num = atoi(view.ptr);
    return num;
}
u64 ConvertHexadecimal(const StringView& view) {
    Assert(view.ptr[0]!='-'); // not handled here

    int start = 0;
    if(view.len >= 2 && (view.ptr[1] == 'x')) // i changed this line, was that okay?
    // if(view.len >= 2 && view.ptr[0] == '0' || view.ptr[1] == 'x')
        start = 2;
    
    u64 hex = 0;
    for(int i=start;i<view.len;i++){
        char chr = view.ptr[i];
        if(chr == '_')
            continue;
        if(chr>='0' && chr <='9'){
            hex = 16*hex + chr-'0';
            continue;
        }
        chr = chr&(~32); // what is this for? chr = chr&0x20 ?
        if(chr>='A' && chr<='F'){
            hex = 16*hex + chr-'A' + 10;
            continue;
        }
    }
    return hex;
}
std::string Lexer::getline(SourceLocation location) {
    Assert(location.tok.type != TOKEN_NONE);

    auto imp = getImport_unsafe(location);

    u32 cindex, tindex;
    decode_origin(location.tok.origin, &cindex, &tindex);

    int index = 0;
    for(int i=0;i<imp->chunks.size();i++){
        if(imp->chunk_indices[i] == cindex) {
            index = TOKEN_ORIGIN_TOKEN_MAX * i + tindex;
            break;
        }
    }

    auto getsource = [&](u32 tokindex) {
        return &imp->chunks[tokindex >> TOKEN_ORIGIN_TOKEN_BITS]->sources[tokindex & (TOKEN_ORIGIN_TOKEN_MAX-1)];
    };
    auto getinfo = [&](u32 tokindex) {
        return &imp->chunks[tokindex >> TOKEN_ORIGIN_TOKEN_BITS]->tokens[tokindex & (TOKEN_ORIGIN_TOKEN_MAX-1)];
    };

    auto base_tok = getsource(index);
    int line = base_tok->line;
    
    int start = index;
    while(start - 1 >= 0) {
        auto tok = getsource(start - 1);
        if(tok->line != line)
            break;
        start--;
    }
    
    auto last_chunk = imp->chunks.last();
    int max = (imp->chunks.size()-1) * TOKEN_ORIGIN_TOKEN_MAX + last_chunk->tokens.size();
    Assert(last_chunk->tokens.size() == last_chunk->sources.size());
    int end = index;
    while(end + 1 < max) {
        auto tok = getsource(end + 1);
        if(tok->line != line)
            break;
        end++;
    }
    end++; // exclusive
    
    // This is a stupid iterator but I am also lazy.
    std::string yeet="";
    auto iter = createFeedIterator(imp, start, end);
    while(feed(iter, true)) {
        yeet.append(iter.data(), iter.len());
    }
    return yeet;
}

bool Lexer::createVirtualFile(const std::string& virtual_path, StringBuilder* builder) {
    auto vfile = TRACK_ALLOC(VirtualFile);
    new(vfile) VirtualFile();
    vfile->virtual_path = virtual_path;
    vfile->builder.steal(builder);
    virtual_files.add(vfile);
    return true;
}
Lexer::VirtualFile* Lexer::findVirtualFile(const std::string& virtual_path) {
    for(auto f : virtual_files) {
        if(f->virtual_path == virtual_path) {
            return f;
        }
    }
    return nullptr;
}
bool Lexer::isIntegerLiteral(Token token, i64* value, int* significant_digits, int* signedness_suffix) {
    auto& type = token.type;

    if(!(type == TOKEN_LITERAL_OCTAL || type == TOKEN_LITERAL_BINARY || type == TOKEN_LITERAL_INTEGER || type == TOKEN_LITERAL_HEXIDECIMAL || (type == TOKEN_LITERAL_STRING && (token.flags & TOKEN_FLAG_SINGLE_QUOTED)))) {
        return false;
    }

    if(!value) return true;

    const char* data = nullptr;
    int len = getStringFromToken(token, &data);

    if(type == TOKEN_LITERAL_INTEGER) {
        i64 num = 0;
        for(int i=0;i<len;i++) {
            char chr = data[i];
            if(chr == '_') continue;
            if(chr == 'u') {
                if(signedness_suffix)
                    *signedness_suffix = UNSIGNED_SUFFIX;
                continue;
            }
            if(chr == 's') {
                if(signedness_suffix)
                    *signedness_suffix = SIGNED_SUFFIX;
                continue;
            }
            if(!(chr >= '0' && chr <= '9')) {
                Assert(false);
                return false;
            }
            num *= 10;
            num += chr - '0';
            if(significant_digits)
                (*significant_digits)++;
        }
        *value = num;
    } else if(type == TOKEN_LITERAL_BINARY) {
        i64 num = 0;
        for(int i=2;i<len;i++) { // i=2 -> skip '0b'
            char chr = data[i];
            if(chr == '_') continue;
            if(chr == 'u') {
                if(signedness_suffix)
                    *signedness_suffix = UNSIGNED_SUFFIX;
                continue;
            }
            if(chr == 's') {
                if(signedness_suffix)
                    *signedness_suffix = SIGNED_SUFFIX;
                continue;
            }
            if(!(chr >= '0' && chr <= '1')) {
                // Assert(false);
                return false;
            }
            num *= 2;
            num += chr - '0';
            if(significant_digits)
                (*significant_digits)++;
        }
        *value = num;
    } else if(type == TOKEN_LITERAL_OCTAL) {
        i64 num = 0;
        for(int i=2;i<len;i++) { // i=2 -> skip '0o'
            char chr = data[i];
            if(chr == '_') continue;
            if(chr == 'u') {
                if(signedness_suffix)
                    *signedness_suffix = UNSIGNED_SUFFIX;
                continue;
            }
            if(chr == 's') {
                if(signedness_suffix)
                    *signedness_suffix = SIGNED_SUFFIX;
                continue;
            }
            if(!(chr >= '0' && chr <= '7')) {
                // Assert(false);
                return false;
            }
            num *= 8;
            num += chr - '0';
            if(significant_digits)
                (*significant_digits)++;
        }
        *value = num;
    } else if(type == TOKEN_LITERAL_HEXIDECIMAL) {
        i64 num = 0;
        for(int i=2;i<len;i++) { // i=2 -> skip '0x'
            char chr = data[i];
            if(chr == '_') continue;
            if(chr == 'u') {
                if(signedness_suffix)
                    *signedness_suffix = UNSIGNED_SUFFIX;
                continue;
            }
            if(chr == 's') {
                if(signedness_suffix)
                    *signedness_suffix = SIGNED_SUFFIX;
                continue;
            }
            if(!((chr >= '0' && chr <= '9') || ((chr|32) >= 'a' && (chr|32)<='f'))) {
                // Assert(false);
                return false;
            }
            num *= 16;
            if(chr >= '0' && chr <= '9')
                num += chr - '0';
            else
                num += (chr|32) - 'a' + 10;
            if(significant_digits)
                (*significant_digits)++;
        }
        *value = num;
    } else if (type == TOKEN_LITERAL_STRING) {
        Assert(len == 1);
        *value = *data;
        if(significant_digits)
            (*significant_digits)++;
    } else Assert(false);

    return true;
}
bool Lexer::get_source_information(SourceLocation loc, std::string* path, int* line, int* column, Import** out_imp) {
    std::string cwd = engone::GetWorkingDirectory() + "/";
    ReplaceChar((char*)cwd.data(), cwd.length(), '\\','/');
    int index = 0;
    
    u32 cindex,tindex;
    decode_origin(loc.tok.origin, &cindex, &tindex);

    auto chunk = getChunk_unsafe(cindex);
    lexer::TokenInfo* info=nullptr;
    lexer::TokenSource* src=nullptr;
    if(loc.tok.type == lexer::TOKEN_EOF) {
        info = &chunk->tokens[tindex-1];
        src = &chunk->sources[tindex-1];
    } else {
        info = &chunk->tokens[tindex];
        src = &chunk->sources[tindex];
    }
    auto imp = getImport_unsafe(chunk->import_id);
    if(out_imp)
        *out_imp = imp;

    while(imp->path.size() > index && cwd.size() > index) {
        if(imp->path[index] != cwd[index])
            break;
        index++;
    }
    if(path) {
        if(index != 0)
            *path = imp->path.substr(index);
        else
            *path = imp->path;
    }
    if(line)
        *line = src->line;
    if(column)
        *column = src->column;
    return true;
}
Import* Lexer::getImportFromTokenPointer(TokenInfo* info, int* out_token_index) {
    lock_chunks.lock();
    auto iter = _chunks.iterator();
    Chunk* chunk = nullptr;
    // TODO: Optimize with a hash map?
    while(_chunks.iterate(iter)) {
        if (info > iter.ptr->tokens.data() && info < iter.ptr->tokens.data() + iter.ptr->tokens.size()) {
            chunk = iter.ptr;
            break;
        }
    }
    if(out_token_index)
        *out_token_index = ((u64)info - (u64)iter.ptr->tokens.data()) / sizeof (*info);
    auto imp = imports.get(chunk->import_id-1);
    lock_chunks.unlock();
    return imp;
}
std::string Lexer::get_source_information_string(SourceLocation loc) {
    std::string path;
    int line,column;
    get_source_information(loc, &path, &line, &column);
    return path + ":"+std::to_string(line)+":"+std::to_string(column);
}
}

