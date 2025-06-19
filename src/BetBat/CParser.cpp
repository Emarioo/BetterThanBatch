#include "BetBat/CParser.h"

#include "Engone/PlatformLayer.h"
#include "Engone/Logger.h"
#include "BetBat/Util/StringBuilder.h"

std::string TranspileCFileToBTB(const std::string& filepath, TranspileOptions* options) {
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
    return TranspileCToBTB(text, options, filepath);
}

std::string TranspileCToBTB(const std::string& text, TranspileOptions* options, const std::string& path) {
    using namespace clexer;
    using namespace engone;
    {
        CPreprocContext context{};
        context.options = options;
        std::string stoff = PreprocessText(&context, text, path);
        auto f = FileOpen("temp.h", FILE_CLEAR_AND_WRITE);
        Assert(f);
        FileWrite(f, stoff.c_str(), stoff.size());
        FileClose(f);
    }

    return "";

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

    #define isalnum(chr) (((chr|32) >= 'a' && (chr|32) <= 'z') || (chr >= '0' && chr <= '9') || chr == '_')

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

    // for(auto& pair : context.macros) {
    //     context.out += "#macro " + pair.first;
    //     if(pair.second.args.size() < 0) {
    //         context.out += "(";
    //         for(int i=0;i<pair.second.args.size(); i++) {
    //             auto& arg = pair.second.args[i];
    //             if(i != 0)
    //                 context.out += ", ";
    //             context.out += arg;
    //         }
    //         context.out += ")\n";
    //     } else {
    //         context.out += " ";
    //     }
    //     if(pair.second.content.size() > 3) {
    //         for(int i=0;i<pair.second.content.size(); i++) {
    //             auto& tok = pair.second.content[i];
    //             if(i != 0)
    //                 context.out += " ";
    //             context.out += tok.data;
    //             if(tok.has_newline)
    //                 context.out += "\n";
    //         }
    //         context.out += "#endmacro\n";
    //     } else {
    //         for(int i=0;i<pair.second.content.size(); i++) {
    //             auto& tok = pair.second.content[i];
    //             if(i != 0)
    //                 context.out += " ";
    //             context.out += tok.data;
    //         }
    //         context.out += "\n";
    //     }
    // }
    // for(auto& pair : context.typedefs) {
    //     context.out += "#macro " + pair.first + " " + pair.second + "\n";
    // }
    for(auto& var : context.variables) {
        context.out += "global " + var.name + ": " + var.type + "\n";
    }
    for(auto& fun : context.functions) {
        context.out += "fn @import(__c_import__) " + fun.name + "(";
        for(int i=0;i<fun.args.size();i++){
            if(i!=0) {
                context.out+=", ";
            }
            context.out += fun.args[i].name + ": " + fun.args[i].type;
        }
        context.out += ")";

        context.out += " -> " + fun.return_type + ";\n";
    }
    // for(auto& str : context.structures) {
    //     context.out += "struct " + str.name + " {\n";
    //     for(int i=0;i<str.fields.size();i++){
    //         context.out += "   " + str.fields[i].name + ": " + str.fields[i].type;
    //         context.out+=";\n";
    //     }
    //     context.out += "}\n";
    // }

    return context.out;
}

int parse_space(StringView text, int* head) {
    Assert(head);
    int start = *head;
    while(*head < text.len) {
        char c = text.ptr[*head];
        if(c != ' ' && c != '\t') {
        // if(c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        *head += 1;
    }
    return *head - start;
}
int parse_name(StringView text, int* head) {
    Assert(head);
    int start = *head;
    while(*head < text.len) {
        char c = text.ptr[*head];
        if (!( 
            ((c|32) >= 'a' && (c|32) <= 'z') ||
            (c == '_') ||
            (start != *head && c >= '0' && c <= '9')
            )) {
            break;
        }
        *head += 1;
    }
    return *head - start;
}

void report_error(const std::string& text, int pos, const std::string& path, const std::string& msg) {
    using namespace engone;
    int head = 0, line = 1, column = 1;
    while(head < pos) {
        if (text[head] == '\n') {
            line++;
            column=0;
        }
        column++;
        head++;
    }
    log::out << log::RED << path << ":"<<line<<":"<<column<<": "<< log::NO_COLOR << msg << "\n";
}

std::string calc_location(const std::string& text, int pos, const std::string& path) {
    int head = 0, line = 1, column = 1;
    while(head < pos) {
        if (text[head] == '\n') {
            line++;
            column=0;
        }
        column++;
        head++;
    }
    return path + ":" + std::to_string(line) + ":" + std::to_string(column);
}

int eval_expression(CPreprocContext* context, const std::string& text, int* head, const std::string& origin_path) {
    /* Some grammar
    expr        := or_expr
    or_expr     := and_expr ( "||" and_expr )*
    and_expr    := equality_expr ( "&&" equality_expr )*
    equality_expr := rel_expr ( ("==" | "!=") rel_expr )*
    rel_expr    := add_expr ( ("<" | ">" | "<=" | ">=") add_expr )*
    add_expr    := mul_expr ( ("+" | "-") mul_expr )*
    mul_expr    := unary_expr ( ("*" | "/" | "%") unary_expr )*
    unary_expr  := ("!" | "~" | "-" | "defined") unary_expr | primary
    primary     := integer | identifier | "(" expr ")"
    */
}

std::string PreprocessText(CPreprocContext* context, const std::string& text, const std::string& origin_path) {
    using namespace engone;
    // ifdef, macros, include, pragma once, pragma pack push/pop
    // include dirs, pre-defines
    // CPreprocContext context;

    std::string output_text = "";

    output_text += "// include " + origin_path + "\n";

    // find directives and expand macros
    // string
    // comments
    // backslash?
    int head = 0;
    while (head < text.size()) {

        // Preserve comments
        if(head+1 < text.size() && text[head] == '/' && text[head+1] == '/') {
            output_text += "//";
            head+=2;
            while (head < text.size()) {
                if(text[head] == '\n') {
                    head+=1;
                    output_text += "\n";
                    break;
                }
                output_text += text[head];
                head+=1;
            }
            continue;
        }
        if(head+1 < text.size() && text[head] == '/' && text[head+1] == '*') {
            output_text += "/*";
            head+=2;
            while (head < text.size()) {
                if(head+1 < text.size() && text[head] == '*' && text[head+1] == '/') {
                    head+=2;
                    output_text += "*/";
                    break;
                }
                output_text += text[head];
                head+=1;
            }
            continue;
        }
        // Preserve strings
        if (text[head] == '"') {
            output_text += "\"";
            head+=1;
            while (head < text.size()) {
                if(text[head] == '"' && (head-1 < 0 || text[head-1] != '\\')) {
                    head+=1;
                    output_text += "\"";
                    break;
                }
                output_text += text[head];
                head+=1;
            }
            continue;
        }
        if (text[head] == '\'') {
            output_text += "'";
            head+=1;
            while (head < text.size()) {
                if(text[head] == '\'' && (head-1 < 0 || text[head-1] != '\\')) {
                    head+=1;
                    output_text += "'";
                    break;
                }
                output_text += text[head];
                head+=1;
            }
            continue;
        }

        if (text[head] != '#') {
            // evaluate macros

            int mac_start = head;
            int parsed_chars = parse_name(text, &head);

            if (parsed_chars != 0) {
                std::string name = text.substr(mac_start, parsed_chars);

                auto pair = context->macros.find(name);
                if (pair == context->macros.end()) {
                    output_text += name;
                    continue;
                } else {
                    log::out << "Matched " << name<< "("<<calc_location(text, mac_start, origin_path)<<")\n";
                    parse_space(text, &head);

                    // TODO: Handle concatenation
                    // TODO: Handle text to string

                    if (text[head] == '(' && !pair->second.no_params) {
                        head++;
                        DynamicArray<std::string> args{};

                        while (head < text.size()) {
                            if(text[head] == ')') {
                                head++;
                                break;
                            } else if(text[head] == ',') {
                                head++;
                                continue;
                            }
                            // TODO: Skip space next to comma?

                            if(args.size() == 0)
                                args.add({});
                            
                            args.last() += text[head];
                            head++;
                        }

                        // for(auto& s : args)  log::out << "arg " << s <<"\n";

                        if (pair->second.parameters.size() != args.size()) {
                            // TODO: Handle VA ARGS
                            report_error(text, head, origin_path, "Args mismatch.");
                            Assert(pair->second.parameters.size() == args.size());
                        }

                        std::string tmp_text = pair->second.content;
                        for(int i=0;i<pair->second.parameters.size();i++){
                            
                            int tmp_head = 0;
                            while(true) {
                                int at = tmp_text.find(pair->second.parameters[i], tmp_head);
                                if(at == -1)
                                    break;

                                tmp_text = tmp_text.substr(0, at) + args[i] + tmp_text.substr(at + pair->second.parameters[i].size());
                                tmp_head = at + args[i].size();
                            }
                        }
                        output_text += tmp_text;
                        // Assert(false);
                    } else {
                        // nocheckin evaluate nested macros
                        output_text += pair->second.content;
                        continue;
                    }
                }
            }
            // evaluate nested macros

            // we may have macro with zero arguments no parenthesis

            // we may have macro with arguments

            output_text += text[head];
            head++;
            continue;
        }
        head+=1;
        parse_space(text, &head); // "# ifdef" this does occur in headers

        int directive_start = head;
        int parsed_chars = parse_name(text, &head);
        if(parsed_chars == 0) {
            // nocheckin Is this C syntax error
            output_text += text[head];
            head++;
            continue;
        }
        std::string directive = text.substr(directive_start, parsed_chars);
        if(directive == "define") {
            // parse macro definition
            parse_space(text, &head);

            int macro_name_start = head;
            int name_length = parse_name(text, &head);
            std::string macro_name = "";
            macro_name.resize(name_length);
            memcpy((char*)macro_name.data(), text.c_str() + macro_name_start, name_length);

            log::out << "Define "<<macro_name << " ("<<calc_location(text, macro_name_start, origin_path)<< ")\n";

            parse_space(text, &head);

            CMacro& macro = context->macros[macro_name] = {};
            macro.name = macro_name;

            // Parse arguments
            if(text[head] != '(') {
                macro.no_params = true;
            } else {
                head++;

                // nocheckin VA_ARGS

                while(head < text.size()) {
                    parse_space(text, &head);

                    if(head >= text.size()) {
                        // nocheckin Syntax error
                        break;
                    }

                    if (text[head] == ')') {
                        head+=1;
                        break;
                    }
                    
                    int param_start = head;
                    int parsed_chars = parse_name(text, &head);
                    if(parsed_chars == 0) {
                        // nocheckin Syntax error
                        break;
                    }
                    macro.parameters.add(text.substr(param_start, parsed_chars));

                    parse_space(text, &head);

                    if (text[head] == ',') {
                        head+=1;
                        break;
                    } else if (text[head] == ')') {
                        head+=1;
                        break;
                    } else {
                        // nocheckin Syntax error
                        break;
                    }
                }
                parse_space(text, &head);
            }

            // parse character content
            while (head < text.size()) {
                if(text[head] == '\n' && (head-1 < 0 || text[head-1] != '\\')) {
                    // head++;
                    break;
                }
                macro.content += text[head];
                head++;
            }
            continue;
        } else if (directive == "undef") {
            parse_space(text, &head);

            int parsed_chars = parse_name(text, &head);
            Assert(parsed_chars);
            std::string name = text.substr(head-parsed_chars, parsed_chars);
            context->macros.erase(name);
            continue;
        } else if (directive == "include") {
            parse_space(text, &head);

            std::string path = "";

            if(text[head] == '<') {
                head++;

                parse_space(text, &head);

                int start = head;
                int end = head;
                while(head < text.size()) {
                    if(text[head] == '>') {
                        head++;
                        break;
                    }
                    if (text[head] == ' ' || text[head] == '\t' || text[head] == '\n' || text[head] == '\r') {
                        head++;
                        continue;
                    }
                    end = head + 1;
                    head++;
                }
                path = text.substr(start, end-start);
            } else if (text[head] == '"') {
                head++;
                int start = head;
                int end = head;
                while(head < text.size()) {
                    if (text[head] == '"' && text[head-1] != '\\') {
                        end = head;
                        head++;
                        break;
                    }
                    head++;
                }

                path = text.substr(start, end - start);
            } else {
                // nocheckin syntax error
            }

            log::out << "Include " << path << "\n";
            std::string found_path="";
            for(int i=0;i<context->options->include_dirs.size();i++) {
                std::string real_path = context->options->include_dirs[i] + "/" + path;
                if (FileExist(real_path)) {
                    found_path = real_path;
                } else {
                    // log::out << " not found in " << context->options->include_dirs[i] << "\n";
                }
            }
            if (found_path.size() == 0) {
                // nocheckin File not found error
                log::out << "File not found " << found_path << "\n";
                continue;
            }
            // Look for path in include dirs.
            u64 filesize;
            auto file = FileOpen(found_path, FILE_READ_ONLY, &filesize);
            if(!file) {
                log::out << "File denied " << found_path << "\n";
                // nocheckin File read denied
                continue;
            }
            log::out << "Process "<< found_path << "\n";
            std::string new_text{};
            new_text.resize(filesize);
            FileRead(file, (char*)new_text.data(), filesize);
            FileClose(file);

            context->included_files.add(found_path);

            output_text += PreprocessText(context, new_text, found_path);
            continue;
        } else if (directive == "ifdef" || directive == "ifndef" || directive == "if") {
            parse_space(text, &head);

            bool skip_block = false; 

            if (directive == "if") {
                int value = eval_expression(context, text, &head, origin_path);
                skip_block = value == 0;
            } else {
                parsed_chars = parse_name(text, &head);
                if (parsed_chars == 0) {
                    report_error(text, head - parsed_chars, origin_path, "Syntax error?");
                    Assert(false);
                }
                std::string name = text.substr(head - parsed_chars, parsed_chars);

                auto pair = context->macros.find(name);
                skip_block = pair == context->macros.end();
                if(directive == "ifndef")
                    skip_block = !skip_block;
            }

            if(skip_block) {
                while(head < text.size()) {
                    // TODO: Parse literal string and comments so we ignore #endif inside them

                    if(text[head] == '#') {
                        head++;
                        parse_space(text, &head);
                        parsed_chars = parse_name(text, &head);
                        if (parsed_chars != 0) {
                            if (!strncmp(text.c_str() + head - parsed_chars, "endif", parsed_chars)) {
                                break;
                            }
                        }
                        continue;
                    }
                    head++;
                }
            }
            continue;
        } else if(directive == "endif") {
            // do nothing
            continue;
        }

        output_text += "#" + text.substr(directive_start, parsed_chars);

        //  else if(!strncmp(text.c_str(), "if", parsed_chars)) {
        //     // parse macro definition
        //     Assert(false);
        // } else if(!strncmp(text.c_str(), "ifdef", parsed_chars)) {
        //     // parse macro definition
        //     Assert(false);
        // } else if(!strncmp(text.c_str(), "undef", parsed_chars)) {
        //     // parse macro definition
        //     Assert(false);
        // } else if(!strncmp(text.c_str(), "pragma", parsed_chars)) {
        //     // parse macro definition
        //     Assert(false);
        // }
        // nocheckin Unknown directive
    }
    output_text += "// END include " + origin_path + "\n";
    return output_text;
}

namespace clexer {
    bool LexerContext::parse_comment() {
        if (gettok(head).data == "/" && gettok(head+1).data == "/") {
            head+=2;
            auto& token = gettok(head);
            
            write("//", token.line, token.column, false);
            while(true) {
                write(gettok(head));
                if (gettok(head).has_newline)
                    break;
                head++;
            }

            return true;
        } else if (gettok(head).data == "/" && gettok(head+1).data == "*") {
            head+=2;


            return true;
        }
        return false;
    }
    void LexerContext::parse_top() {
        using namespace engone;
        while (head < tokens.size()) {
            auto& token = tokens[head];

            // if(token[])

            // When importing C header we can skip comments but
            // if you manually want to convert C then this might be nice.
            if (token.data == "/" && gettok(head).data == "/" && token.column+1 == gettok(head).column && token.line == gettok(head).line) {
                // log::out << token.data << " "<<token.line<<" "<<token.column << " " << gettok(head).line << " " << gettok(head).column<<"\n";
                head++;
                write("//", token.line, token.column, false);
                while(true) {
                    write(gettok(head));
                    if (gettok(head).has_newline)
                        break;
                    head++;
                }
                continue;
            }
            if (token.data == "/" && gettok(head).data == "*" && token.column+1 == gettok(head).column && token.line == gettok(head).line) {
                // log::out << token.data << " "<<token.line<<" "<<token.column << " " << gettok(head).line << " " << gettok(head).column<<"\n";
                write("/*", token.line, token.column, gettok(head).has_newline);
                head++;
                while(true) {
                    Token& tok0 = gettok(head);
                    Token& tok1 = gettok(head+1);
                    if (tok0.data == "*" && tok1.data == "/" && !tok0.has_newline) {
                        write("*/", tok0.line, tok0.column, tok1.has_newline);
                        // write(tok0);
                        // write(tok1);
                        head+=2;
                        break;
                    } else { 
                        write(tok0);
                        head++;
                    }
                }
                continue;
            }
            head++;

            if(token.data == "typedef") {
                if(head == tokens.size())
                    continue;
                auto& token0 = tokens[head];
                auto& token1 = head+1 < tokens.size() ? tokens[head+1] : toknull;
                std::string first_type = parse_base_type(head);

                std::string name = tokens[head].data;

                typedefs[name] = first_type;
                write("#macro", token);
                write(name, token.line, token.column + 7, false);
                write(first_type, token.line, token.column + 8 + name.size(), false);
                // void unsigned int char short long signed float double struct
            } else if(token.data == "#") {
                auto& token = tokens[head];
                head++;
                if(token.data == "undef") {
                    // TODO: Proper error message
                    log::out << log::RED << "#undef is not supported\n";
                    return;
                } else if(token.data == "define") {
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
                    int head_bef = head;
                    if(!gettok(head_bef).has_newline) {
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
                                    auto last = gettok(head+4);
                                    std::string word = "@export";
                                    macro.content.add({word, tok.line, tok.column, last.has_newline});
                                    write(word, tok.line, tok.column, last.has_newline);
                                } else {
                                    // Header shouldn't import?
                                    // It should import actually.
                                    // It shouldn't export.
                                }
                                head += 5;
                                // skip
                            } else if(getstr(head-1) == "__attribute__" && getstr(head) == "(" && getstr(head+1) == "(" && getstr(head+2) == "visibility" && getstr(head+3) == "(" && getstr(head+4) == "\"default\"" && getstr(head+5) == ")" && getstr(head+6) == ")" && getstr(head+7) == ")") {
                                // NO export, what about import
                                had_export = true;
                                std::string word = "@export";
                                auto last = gettok(head+7);
                                macro.content.add({word, tok.line, tok.column, last.has_newline});
                                write(word, tok);
                                head += 8;
                            } else if(getstr(head-1) == "__declspec" && getstr(head) == "(" && (getstr(head+1) == "dllexport"||getstr(head+1) == "dllimport") && getstr(head+2) == ")") {
                                if(getstr(head+1) == "dllexport") {
                                    had_export = true;
                                    auto last = gettok(head+2);
                                    std::string word = "@export";
                                    macro.content.add({word, tok.line, tok.column, last.has_newline});
                                    write(word, tok.line, tok.column, last.has_newline);
                                } else {
                                    // Should be IMPORT NOT EXPORT
                                }
                                head += 3;
                                // skip
                            } else {
                                macro.content.add(tok);
                                write(tok);
                            }
                            // if (macro.content.size() == 0) {
                                if(tok.has_newline || head == tokens.size()) {
                                //     write("#endmacro", tok);
                                    break;
                                }
                            // }
                        }
                    }
                    if (head_bef == head || gettok(head_bef).line != gettok(head-1).line) {
                        write(" #endmacro", gettok(head-1));
                    }
                } else if(token.data == "ifdef" || token.data == "ifndef") {
                    auto& name = gettok(head);
                    write("#if", token);
                    if(token.data == "ifndef")
                        write("!", name.line, name.column, false);
                    write(name);
                    head++;
                } else if(token.data == "include") {
                    auto& name = gettok(head);
                    if (name.data[0] == '"') {
                        head++;
                        log::out << "include "<<name.data<<"\n";
                    } else if (name.data[0] == '<') {
                        head++;
                        std::string path;
                        while(head < tokens.size()){
                            auto& tok = gettok(head);
                            head++;
                            if (tok.data == ">") {
                                break;
                            }
                            path += tok.data;
                        }
                        log::out << "include "<<path<<"\n";
                    } else {
                        head++;
                    }
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

                    write("struct", token);
                    write(name, token.line, token.column + 7, false);
                    write("{", token.line, token.column + 7 + name.size() + 1, false);

                    parse_struct_fields(head, structure);

                    write("}", token);
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
                                        // if(argtype != "void")
                                        func.args.add({name, argtype});
                                        break;
                                    }
                                }
                                
                                auto& token0 = tokens[head];
                                if (token0.data == ";") {
                                    head++;
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

            auto token0 = tokens[index];
            if(token0.data == ";") {
                index++;
            }
            token0.has_newline = false;
            write(name, token);
            write(":", token);
            write(fieldtype, token);
            write(";", token);
            writeln();

            structure.fields.add({name, fieldtype});
        }
    }
    std::string LexerContext::parse_base_type(int& index) {
        std::string typestring = "";
        if(tokens[index].data == "const")
            index++;
        auto& token0 = tokens[index];
        auto& token1 = index+1 < tokens.size() ? tokens[index+1] : toknull;
        auto& token2 = index+2 < tokens.size() ? tokens[index+2] : toknull;
        if(token0.data == "void" || token0.data == "bool") {
            // C doesn't have bool type, we handle it
            // anyway because you might want to parse C++ header
            // that is mostly C with some C++ elements (like bool)
            typestring = token0.data;
            index+=1;
        } else if(token0.data == "float") {
            typestring = "f32";
            index+=1;
        } else if(token0.data == "double") {
            typestring = "f64";
            index+=1;
        } else if (token0.data == "char" || token0.data == "short" || token0.data == "int") {
            if(token0.data == "char")
                typestring += "i8";
            else if(token0.data == "short")
                typestring += "i16";
            else if(token0.data == "int")
                typestring += "i32";
            index+=1;
        } else if (token0.data == "long" && (token1.data == "long" || token1.data == "int")) {
            typestring += "i64";
            index+=2;
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
            index+=2;
        } else if(token0.data == "enum") {
            typestring += token1.data;
            index+=2;
        } else if((token0.data == "unsigned" || token0.data == "signed") && token1.data == "long" && (token2.data == "long" || token2.data == "int")) {
            if(token0.data == "signed")
                typestring += "i";
            else
                typestring += "u";
            typestring += "64";
            index+=3;
        } else {
            // named thing?
            if(isalnum(token0.data[0])) {
                typestring = token0.data;
                index++;
            }
            // TODO: handle function pointer
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