#include "BetBat/CParser.h"

#include "Engone/PlatformLayer.h"
#include "Engone/Logger.h"

std::string TranspileCFileToBTB(const std::string& filepath) {
    using namespace engone;
    u64 filesize = 0;
    auto file = FileOpen(filepath, FILE_READ_ONLY, &filesize);
    if (!file) {
        log::out << log::RED << "Could not open " << filepath << "\n";
        return "";
    }
    std::string text = "";
    text.resize(filesize);
    bool res = FileRead(file, (char*)text.data(), filesize);
    if (!res) {
        log::out << log::RED << "Could not read "<<filesize << " from " << filepath<<"\n";
        return "";
    }
    FileClose(file);
    return TranspileCToBTB(text);
}

std::string TranspileCToBTB(const std::string& text) {
    using namespace clexer;
    using namespace engone;

    /*
        TODO:
            typedefs
            defines
            function defs
            variable defs
            ifdef
            attributes/declspec

            struct
            include

        Process
            Tokens (easy to work with)
            Parse and preprocess at the same time?
                accumulate language constructs and convert
                to BTB equivalent
    */

    // ====================
    //      LEX TOKENS
    // ====================

    #define isalnum(chr) (((chr|32) >= 'a' && (chr|32) <= 'z') || (chr >= '0' & chr <= '9') || chr == '_')

    LexerContext context{};

    DynamicArray<Token>& tokens = context.tokens;
    int head = 0;
    int line = 1;
    int column = 1;
    while(head < text.size()) {
        char chr = text[head];
        // char chr2 = text[head+1];
        head++;
        
        if(chr == '\n') {
            if(tokens.size() > 0)
                tokens.last().has_newline = true;
            line++;
            column = 1;
            continue;
        }
        if(chr == ' ') {
            column++;
            continue;
        }
        if(chr == '\t') {
            column += 4;
            continue;
        }
        if(chr == '\r') {
            continue;
        }

        if(isalnum(chr)) {
            int start = head-1;
            int end = head;
            while(true) {
                if(!isalnum(text[end]))
                    break;
                end++;
            }
            Token tok{};
            tok.data = text.substr(start, end-start);
            tok.line = line;
            tok.column = column;
            tokens.add(tok);
            head = end;
            column += end - start;
            continue;
        }
        if(chr == '"') {
            int start = head-1;
            int end = head;
            while(true) {
                char chr = text[end];
                end++;
                // handle escaped quotes
                if(chr == '"')
                    break;
            }
            Token tok{};
            tok.data = text.substr(start, end-start);
            tok.line = line;
            tok.column = column;
            tokens.add(tok);
            head = end;
            column += end - start;
            continue;
        }

        Token tok{};
        tok.data = chr;
        tok.line = line;
        tok.column = column;
        column += 1;
        tokens.add(tok);
        continue;
    }

    // for(int i=0;i<tokens.size();i++) {
    //     auto& tok = tokens[i];
    //     context.write(tok);
    // }
    // context.cur_column = 1;
    // context.head = 0;
    // context.cur_line = 1;
    
    // ====================
    //      PARSE TOKENS
    // ====================

    context.parse_top();

    for(auto& pair : context.macros) {
        context.out += "#macro " + pair.first;
        if(pair.second.args.size() < 0) {
            context.out += "(";
            for(int i=0;i<pair.second.args.size(); i++) {
                auto& arg = pair.second.args[i];
                if(i != 0)
                    context.out += ", ";
                context.out += arg;
            }
            context.out += ")\n";
        } else {
            context.out += " ";
        }
        if(pair.second.content.size() > 3) {
            for(int i=0;i<pair.second.content.size(); i++) {
                auto& tok = pair.second.content[i];
                if(i != 0)
                    context.out += " ";
                context.out += tok.data;
                if(tok.has_newline)
                    context.out += "\n";
            }
            context.out += "#endmacro\n";
        } else {
            for(int i=0;i<pair.second.content.size(); i++) {
                auto& tok = pair.second.content[i];
                if(i != 0)
                    context.out += " ";
                context.out += tok.data;
            }
            context.out += "\n";
        }
    }
    for(auto& pair : context.typedefs) {
        context.out += "#macro " + pair.first + " " + pair.second + "\n";
    }
    for(auto& var : context.variables) {
        context.out += "global " + var.name + ": " + var.type + "\n";
    }
    for(auto& fun : context.functions) {
        context.out += "fn " + fun.name + "(";
        for(int i=0;i<fun.args.size();i++){
            if(i!=0) {
                context.out+=", ";
            }
            context.out += fun.args[i].name + ": " + fun.args[i].type;
        }
        context.out += ")";

        context.out += " -> " + fun.return_type + "\n";
    }
    for(auto& str : context.structures) {
        context.out += "struct " + str.name + " {\n";
        for(int i=0;i<str.fields.size();i++){
            context.out += "   " + str.fields[i].name + ": " + str.fields[i].type;
            context.out+=";\n";
        }
        context.out += "}\n";
    }

    return context.out;
}

namespace clexer {
    void LexerContext::parse_top() {
        using namespace engone;
        while (head < tokens.size()) {
            auto& token = tokens[head];
            head++;

            if(token.data == "typedef") {
                if(head == tokens.size())
                    continue;
                auto& token0 = tokens[head];
                auto& token1 = head+1 < tokens.size() ? tokens[head+1] : toknull;
                std::string first_type = parse_base_type(head);

                std::string name = tokens[head].data;

                typedefs[name] = first_type;
                // void unsigned int char short long signed float double struct
            } else if(token.data == "#") {
                auto& token = tokens[head];
                head++;
                if(token.data == "define") {
                    if(head == tokens.size())
                        continue;
                    write("#macro", token);
                    std::string macro_name = tokens[head].data;
                    Token nametok = tokens[head];
                    head++;
                    write(nametok);

                    auto& macro = (macros[macro_name] = {});

                    // parse define arguments
                    if(tokens[head].data == "(") {
                        write(gettok(head));
                        head++;
                        while(head < tokens.size()) {
                            auto& tok = tokens[head];
                            head++;
                            if(tok.data == ")") {
                                write(tok);
                                break;
                            } else if(tok.data == ",") {
                                write(tok);
                                continue;
                            }
                            macro.args.add(tok.data);
                            write(tok);
                        }
                    }
                    bool had_import=false;
                    bool had_export=false;
                    // parse define content
                    while(head < tokens.size()) {
                        auto& tok = tokens[head];
                        head++;
                        if(tok.data == "\\" && tok.has_newline) {
                            writeln();
                            if(macro.content.size() > 0) 
                                macro.content.last().has_newline = true;
                            continue;
                        }
                        if(tok.data == "extern") {
                            // skip
                            if(!had_export) {
                                std::string word = "@import(LIB_" + macro_name + ")";
                                macro.content.add({word, tok.line, tok.column, tok.has_newline});
                                write(word, tok);
                            }
                        } else if(getstr(head-1) == "__attribute__" && getstr(head) == "(" && getstr(head+1) == "(" && (getstr(head+2) == "dllexport"||getstr(head+2) == "dllimport") && getstr(head+3) == ")" && getstr(head+4) == ")") {
                            if(getstr(head+2) == "dllexport") {
                                had_export = true;
                                auto& last = gettok(head+4);
                                std::string word = "@export";
                                macro.content.add({word, tok.line, tok.column, last.has_newline});
                                write(word, tok.line, tok.column, last.has_newline);
                            }
                            head += 5;
                            // skip
                        } else if(getstr(head-1) == "__attribute__" && getstr(head) == "(" && getstr(head+1) == "(" && getstr(head+2) == "visibility"||getstr(head+3) == "(" && getstr(head+4) == "\"default\"" && getstr(head+5) == ")" && getstr(head+6) == ")" && getstr(head+7) == ")") {
                            had_export = true;
                            std::string word = "@export";
                            auto& last = gettok(head+7);
                            macro.content.add({word, tok.line, tok.column, last.has_newline});
                            write(word, tok);
                            head += 8;
                        } else if(getstr(head-1) == "__declspec" && getstr(head) == "(" && (getstr(head+1) == "dllexport"||getstr(head+1) == "dllimport") && getstr(head+2) == ")") {
                            if(getstr(head+1) == "dllexport") {
                                had_export = true;
                                auto& last = gettok(head+2);
                                std::string word = "@export";
                                macro.content.add({word, tok.line, tok.column, last.has_newline});
                                write(word, tok.line, tok.column, last.has_newline);
                            }
                            head += 3;
                            // skip
                        } else {
                            macro.content.add(tok);
                            write(tok);
                        }

                        if(tok.has_newline || head == tokens.size()) {
                            write("#endmacro", tok);
                            break;
                        }
                    }
                } else if(token.data == "ifdef" || token.data == "ifndef") {
                    auto& name = gettok(head);
                    write("#if", token);
                    if(token.data == "ifndef")
                        write("!", name.line, name.column, false);
                    write(name);
                    head++;
                } else if(token.data == "if" || token.data == "elif") {
                    write(gettok(head-2)); // #
                    write(token);

                    while(head < tokens.size()) {
                        auto& token = tokens[head];
                        head++;
                        if(token.data == "defined") {
                            // skip 
                           if(getstr(head) == "(" && getstr(head+2) == ")") {
                                write(gettok(head+1));
                                head += 3;
                                if(gettok(head+2-3).has_newline) {
                                    writeln();
                                    break;
                                }
                           }
                        // if(token.data == "!") {
                        //     out += "!";
                        // } else if(token.data == "(") {
                        //     out += "(";
                        // } else if(token.data == ")") {
                        //     out += ")";
                        // } else if(token.data == "&") {
                        //     out += "&";
                        // } else if(token.data == "|") {
                        //     out += "|";
                        } else {
                            out += token.data;
                            if(token.has_newline)
                                break;
                                // out += "\n";
                        }
                    }
                    // numbers we handle later
                } else if(token.data == "else") {
                    write("#else", token);
                }  else if(token.data == "endif") {
                    write("#endif", token);
                } else {
                    head--;
                }
            } else {
                head--;
                
                if(getstr(head) == "struct" && is_tok_alnum(head+1) && getstr(head+2) == "{") {
                    std::string name = getstr(head+1);
                    head+=3;
                    Structure structure{};
                    structure.name = name;

                    parse_struct_fields(head, structure);
                    // TODO: Handle error
                    structures.add(structure);
                } else {
                    // is token a type?
                    int prev_head = head;
                    std::string first_type = parse_base_type(head);
                    if(prev_head != head) {
                        auto& name = tokens[head];
                        auto& token = head+1 < tokens.size() ? tokens[head+1] : toknull;
                        if(name.data.size() > 0 && isalnum(name.data[0])) {
                            if(token.data == "(") {
                                head += 2;
                                // function

                                Function func{};
                                func.return_type = first_type;
                                func.name = name.data;
                                while(head < tokens.size()) {
                                    auto& token = tokens[head];

                                    int prev_head = head;
                                    std::string argtype = parse_base_type(head);

                                    if (prev_head == head) {
                                        // assume typedef
                                        argtype = token.data;
                                        head++;
                                    }

                                    std::string name = "arg"+std::to_string(func.args.size());

                                    auto& nametok = tokens[head];

                                    if(isalnum(nametok.data[0])) {
                                        name = nametok.data;
                                        head++;
                                    }
                                    
                                    auto& token0 = tokens[head];
                                    head++;

                                    if(token0.data == ",") {
                                        func.args.add({name, argtype});
                                        continue;
                                    }
                                    if(token0.data == ")") {
                                        if(argtype != "void")
                                            func.args.add({name, argtype});
                                        break;
                                    }
                                }

                                functions.add(func);
                            } else if(token.data == ";"){
                                head += 2;
                                // variable
                                Variable var{};
                                var.name = name.data;
                                var.type = first_type;
                                variables.add(var);
                            }
                        }
                    } else {
                        head++;
                    }
                    // not a base type
                }
            }
        }
    }
    
    void LexerContext::parse_struct_fields(int& index, Structure& structure) {
        using namespace engone;
        // we have parsed {

        while(index < tokens.size()) {
            auto& token = tokens[index];
            if (token.data == "}") {
                index++;
                break;
            }

            int prev_head = index;
            std::string fieldtype = parse_base_type(index);
            if(prev_head == index) {
                log::out << "could not parse field type\n";
            }

            auto& tokname = tokens[index];
            index++;

            std::string name = tokname.data;

            auto& token0 = tokens[index];
            if(token0.data == ";") {
                index++;
            }
            structure.fields.add({name, fieldtype});
        }
    }
    std::string LexerContext::parse_base_type(int& index) {
        std::string typestring = "";
        int extra = 0;
        if(tokens[index].data == "const")
            extra++;
        auto& token0 = tokens[extra+index];
        auto& token1 = extra+index+1 < tokens.size() ? tokens[extra+index+1] : toknull;
        auto& token2 = extra+index+2 < tokens.size() ? tokens[extra+index+2] : toknull;
        if(token0.data == "void" || token0.data == "bool") {
            // C doesn't have bool type, we handle it
            // anyway because you might want to parse C++ header
            // that is mostly C with some C++ elements (like bool)
            typestring = token0.data;
            index+=extra+1;
        } else if(token0.data == "float") {
            typestring = "f32";
            index+=extra+1;
        } else if(token0.data == "double") {
            typestring = "f64";
            index+=extra+1;
        } else if (token0.data == "char" || token0.data == "short" || token0.data == "int") {
            if(token0.data == "char")
                typestring += "i8";
            else if(token0.data == "short")
                typestring += "i16";
            else if(token0.data == "int")
                typestring += "i32";
            index+=extra+1;
        } else if (token0.data == "long" && (token1.data == "long" || token1.data == "int")) {
            typestring += "i64";
            index+=extra+2;
        } else if((token0.data == "unsigned" || token0.data == "signed") && (token1.data == "char" || token1.data == "short" || token1.data == "int")) {
            if(token0.data == "signed")
                typestring += "i";
            else
                typestring += "u";
            if(token1.data == "char")
                typestring += "8";
            else if(token1.data == "short")
                typestring += "16";
            else if(token1.data == "int")
                typestring += "32";
            index += 2;
        } else if(token0.data == "struct") {
            typestring += token1.data;
            index+=extra+2;
        } else if((token0.data == "unsigned" || token0.data == "signed") && token1.data == "long" && (token2.data == "long" || token2.data == "int")) {
            if(token0.data == "signed")
                typestring += "i";
            else
                typestring += "u";
            typestring += "64";
            index+=extra+3;
        } else {
            // handle function pointer
        }
        return typestring;
    };
    void LexerContext::skip_paren(int& index, int in_depth) {
        int depth = in_depth;
        while(index < tokens.size()) {
            auto& tok = tokens[index];
            index++;
            if(tok.data == "(") {
                depth++;
            }
            if(tok.data == ")") {
                depth--;
                if(depth == 0)
                    break;
            }
        }
    };
}