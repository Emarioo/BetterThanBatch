#include "BetBat/CParser.h"

#include "Engone/PlatformLayer.h"
#include "Engone/Logger.h"
#include "BetBat/Util/StringBuilder.h"
#include "BetBat/CompilerOptions.h"

#define CPARSER_VERBOSE(X) 
// #define CPARSER_VERBOSE(X) X

std::string annihilate_forsaken_space_in_program_files(const std::string& path);

std::string TranspileCFileToBTB(const std::string& filepath, TranspileOptions* options, CompileOptions* compile_options) {
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
    return TranspileCToBTB(text, options, annihilate_forsaken_space_in_program_files(filepath), compile_options);
}

std::string TranspileCToBTB(const std::string& text, TranspileOptions* options, const std::string& path, CompileOptions* compile_options) {
    using namespace clexer;
    using namespace engone;
    {
        CPreprocContext context{};

        /*
            Predefined macros
        */
        context.macros["__STDC__"] = {"1"};
        context.macros["__STDC_VERSION__"] = {"201112"};
        // TODO: Don't assume gnu? we may link with clang or msvc
        context.macros["__GNUC__"] = {"14"};
        context.macros["_DLL"] = {};
        
        // GCC behaviour defining declspec and cdecl and stuff
        auto& macro_declspec = context.macros["__declspec"] = {"__attribute__((X))"};
            macro_declspec.has_params = true;
            macro_declspec.parameters.add("X");
        context.macros["__cdecl"] = {"__attribute__((__cdecl__))"};
        context.macros["__stdcall"] = {"__attribute__((__stdcall__))"};
        context.macros["__fastcall"] = {"__attribute__((__fastcall__))"};

        switch(compile_options->target) {
            // https://github.com/cpredef/predef/blob/master/Architectures.md
            case TARGET_WINDOWS_x64: {
                context.macros["_WIN32"] = {"1"};
                context.macros["_WIN64"] = {"1"};
                context.macros["__x86_64__"] = {""};
            } break;
            case TARGET_LINUX_x64: {
                context.macros["__linux__"] = {};
                // TODO: Which macros on Linux?
            } break;
            case TARGET_AARCH64: {
                context.macros["__aarch64__"] = {};
            } break;
            case TARGET_ARM: {
                context.macros["__arm__"] = {};
            } break;
            default: {
                // TODO: Print the line where we imported the C header.
                log::out << log::YELLOW << "No predefined macros when importing C header on target '"<<compile_options->target<<"'.\n";
            }
        }

        context.options = options;
        std::string stoff = PreprocessText(&context, text, annihilate_forsaken_space_in_program_files(path));
        auto f = FileOpen("temp.h", FILE_CLEAR_AND_WRITE);
        Assert(f);
        FileWrite(f, stoff.c_str(), stoff.size());
        FileClose(f);
        // log::out << stoff << "\n";
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
int parse_name(StringView text, int* head, std::string* name) {
    Assert(head);
    int start = *head;
    while(*head < text.len) {
        char c = text.ptr[*head];
        if (!( 
            (((c|32) >= 'a' && (c|32) <= 'z')) ||
            (c == '_') ||
            (start != *head && c >= '0' && c <= '9')
            )) {
            break;
        }
        *head += 1;
    }
    if(name) {
        *name = std::string(text.ptr + start, *head - start);
    }
    return *head - start;
}
int parse_string(StringView text, int* head, std::string* name) {
    Assert(head);
    Assert(name);

    if(text.ptr[*head] != '"') {
        return 0;
    }
    *head += 1;

    int start = *head;
    while(*head < text.len) {
        char c = text.ptr[*head];
        if(c == '"') {
            *head += 1;
            break;
        }
        *head += 1;
    }
    *name = std::string(text.ptr + start, *head - start);
    return *head - start;
}
// DOES NOT RETURN PARSED INTEGER, check 'value' instead
int parse_int(StringView text, int* head, int* value) {
    Assert(head);
    Assert(value);

    int start = *head;
    // while(*head < text.len) {
    //     char c = text.ptr[*head];
    //     if (c >= '0' && c <= '9') {
    //         *head += 1;
    //         continue;
    //     }
    //     break;
    // }
    // // *value = atoi(text.ptr + start);
    char* end_ptr;
    *value = strtol(text.ptr + *head, &end_ptr, 0);
    *head = (u64)end_ptr - (u64)text.ptr;

    while(*head < text.len) {
        char c = text.ptr[*head];
        if((c|32) == 'l' || (c|32) == 'u') {
            *head += 1;
            continue;
        }
        break;
    }
    return *head - start;
}
// comment includes slash and newlines
// comment may be null
int parse_comment(StringView text, int* head, std::string* comment) {
    if(comment)
        *comment = {};
    
    int start = *head;

    if(*head + 1 >= text.size())
        return 0;

    if(text.ptr[*head] == '/' && text.ptr[*head+1] == '/') {
        *head += 2;
        while(*head < text.len) {
            char c = text.ptr[*head];
            if (c == '\n') {
                break;
            }
            *head += 1;
        }
        *comment = std::string(text.ptr + start, *head - start);
        return *head - start;
    } else if(text.ptr[*head] == '/' && *head + 1 < text.size() && text.ptr[*head+1] == '*') {
        *head += 2;
        while(*head + 1 < text.len) {
            char c = text.ptr[*head];
            if (c == '*' && text.ptr[*head + 1] == '/') {
                *head += 2;
                break;
            }
            *head += 1;
        }
        *comment = std::string(text.ptr + start, *head - start);
        return *head - start;
    }
    return 0;
}

std::string annihilate_forsaken_space_in_program_files(const std::string& path) {
    int at = path.find("C:/Program Files/");
    if (at == -1)
        return path;
    return path.substr(0, at) + "C:/PROGRA~1/" + path.substr(at + strlen("C:/Program Files/"));
}

void report_error(const std::string& text, int pos, const std::string& path, const std::string& msg) {
    using namespace engone;
    u64 filesize;
    auto f = FileOpen(path, FILE_READ_ONLY, &filesize);
    Assert(f);
    std::string data;
    data.resize(filesize);
    FileRead(f, (char*)data.data(), filesize);
    FileClose(f);
    
    int head = 0, line = 1, column = 1;
    while(head < pos) {
        if (data[head] == '\n') {
            line++;
            column=0;
        }
        column++;
        head++;
    }

    log::out << log::RED << annihilate_forsaken_space_in_program_files(path) << ":"<<line<<":"<<column<<": "<< log::NO_COLOR << msg << "\n";
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

void expand_macros(CPreprocContext* context, std::string& text, const std::string& origin_path, bool inside_expression, CMacro* parent = nullptr, DynamicArray<std::string>* parent_args = nullptr, DynamicArray<std::string>* parent_raw_args = nullptr) {
    int head = 0;
    while(head < text.size()) {
        if(text[head] == '#' && head+1 < text.size() && text[head+1] == '#') {
            int non_space_end = head;
            head+=2;
            while(non_space_end-1 > 0) {
                if(text[non_space_end-1] == ' ' || text[non_space_end-1] == '\t' || text[non_space_end-1] == '\n' || text[non_space_end-1] == '\r') {
                    non_space_end--;
                    continue;
                }
                break;
            }
            parse_space(text, &head);
            int prev_len = text.size();
            text = text.substr(0, non_space_end) + text.substr(head);
            head = non_space_end;
            continue;
        } else if(text[head] == '#') {
            int macro_start = head;
            head++;
            parse_space(text, &head);
            std::string macro_name;
            int parsed = parse_name(text, &head, &macro_name);
            int macro_end = head;
            
            if(parsed && parent) {
                int argi = 0;
                for(;argi < parent->parameters.size(); argi++) {
                    if (macro_name == parent->parameters.get(argi)) {
                        break;
                    }
                }
                if (argi < parent_raw_args->size()) {
                    // TODO: Escape newline, tabs, hex codes
                    int prev_len = text.size();
                    text = text.substr(0, macro_start) + "\"" + parent_raw_args->get(argi) + "\"" + text.substr(macro_end);
                    head = macro_start + 2 + parent_raw_args->get(argi).size();
                    continue;
                }
            }
            continue;
        }

        std::string macro_name;
        int macro_start = head;
        int parsed = parse_name(text, &head, &macro_name);
        int macro_end = head;
        CMacro* macro = nullptr;
        if (parsed) {
            if(macro_name == "defined" && inside_expression) {
                parse_space(text, &head);

                int parsed = parse_name(text, &head, nullptr);
                if(!parsed) {
                    Assert(text[head] == '(');
                    head++;
                    int depth = 1;
                    while(head < text.size()) {
                        if(text[head] == '(') {
                            depth++;
                        } else if (text[head] == ')') {
                            depth--;
                            if(depth == 0) {
                                head++;
                                break;
                            }
                        }
                        head++;
                    }
                    continue;
                }
            }
            if(parent) {
                int argi = 0;
                for(;argi < parent->parameters.size(); argi++) {
                    if (macro_name == parent->parameters.get(argi)) {
                        break;
                    }
                }
                if (argi < parent_args->size()) {
                    int prev_len = text.size();
                    text = text.substr(0, macro_start) + parent_args->get(argi) + text.substr(macro_end);
                    head = macro_start + parent_args->get(argi).size();
                    continue;
                }
            }
            auto pair = context->macros.find(macro_name);
            if(pair != context->macros.end()) {
                macro = &pair->second;
            }
            if(!macro || macro == parent) {
                continue;
            }
        } else {
            parsed = parse_string(text, &head, &macro_name);
            if(!parsed) {
                head++;
                // do nothing, nothing to expand
            } else {
                // string, nothing to expand
            }
            continue;
        }

        parse_space(text, &head);

        DynamicArray<std::string> arguments;
        if (text[head] == '(' && macro->has_params) {
            head++;
            parse_space(text, &head);
            int depth = 1;
            int arg_start = head;
            std::string buffer;
            while (head < text.size()) {
                if(text[head] == ')') {
                    depth--;
                    if(depth==0) {
                        head++;
                        break;
                    }
                } else if(text[head] == ',' && depth == 1) {
                    head++;
                    parse_space(text, &head);
                    arguments.add({});
                    continue;
                }
                if(text[head] == '(')
                    depth++;

                if(arguments.size() == 0)
                    arguments.add({});
                
                arguments.last() += text[head];
                head++;
            }
            macro_end = head;
        }

        if (macro->parameters.size() != arguments.size()) {
            // TODO: Handle VA ARGS
            report_error(text, context->current_pos, origin_path, "Args mismatch here somewhere. macro: " + macro_name);
            return;
        }
        DynamicArray<std::string> raw_arguments;
        raw_arguments.resize(arguments.size());
        for(int i=0;i<raw_arguments.size();i++) {
            raw_arguments[i] = arguments[i];
            expand_macros(context,  arguments[i], origin_path, inside_expression, parent, parent_args, parent_raw_args);
        }

        std::string macro_body = macro->content;
        expand_macros(context, macro_body, macro->origin_file, inside_expression, macro, &arguments, &raw_arguments);
        
        int prev_len = text.size();
        text = text.substr(0, macro_start) + macro_body + text.substr(macro_end);
        head = macro_start + macro_body.size();
    }
}
int eval_expression(CPreprocContext* context, std::string& text, int* head, const std::string& origin_path, int expression_start) {
    using namespace engone;
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


    struct Value {
        int literal;
    };
    struct Op {
        Op(char a=0, char b = 0) : kind(a | (b << 8)) {
            
        }
        int kind;
    };
    DynamicArray<Value> values;
    DynamicArray<Op> ops;

    const int OP_AND           = '&' | ('&'<<8);
    const int OP_OR            = '|' | ('|'<<8);
    const int OP_SHL           = '<' | ('<'<<8);
    const int OP_SHR           = '>' | ('>'<<8);
    const int OP_EQUAL         = '=' | ('='<<8);
    const int OP_NOT_EQUAL     = '!' | ('='<<8);
    const int OP_LESS_EQUAL    = '<' | ('='<<8);
    const int OP_GREATER_EQUAL = '>' | ('='<<8);

    auto precedence = [&](int kind) {
        switch(kind) {
            case OP_OR:
                return -2;
            case OP_AND:
                return -1;
            case '|':
                return 2;
            case '^':
                return 3;
            case '&':
                return 4;
            case OP_EQUAL:
            case OP_NOT_EQUAL:
                return 5;
            case OP_LESS_EQUAL:
            case OP_GREATER_EQUAL:
            case '<':
            case '>':
                return 6;
            case OP_SHL:
            case OP_SHR:
                return 7;
            case '+':
            case '-':
                return 10;
            case '*':
            case '/':
            case '%':
                return 20;
        }
        Assert(false);
        return -1;
    };
    int head_start = *head;
    bool finalize = false;
    bool expect_primary = true;
    while (true) {
        parse_space(text, head);
        char c = 0, c2 = 0, c3 = 0;
        if (*head < text.size())
            c = text[*head];
        if(*head+1 < text.size())
            c2 = text[*head + 1];
        if(*head+2 < text.size())
            c3 = text[*head + 2];

        if (c == '\\' && (c2 == '\n' || (c2 == '\r' && c3 == '\n'))) {
            if(c2 == '\r')
                *head += 1;
            *head += 2;
            continue;
        }

        std::string comment;
        parse_comment(text, head, &comment);
        if(comment.size())
            continue;

        if (expect_primary) {
            int unary = 0;
            if(c == '-' || c == '!' || c == '~') {
                *head += 1;
                unary = c;
                parse_space(text, head);
                c = text[*head];
                c2 = 0;
                if(*head+1 < text.size())
                    c2 = text[*head + 1];
            }

            if(isdigit(c)) {
                int value = 0;
                int parsed = parse_int(text, head, &value);
                values.add({value});
            } else if (c == '(') {
                *head += 1;
                int value = eval_expression(context, text, head, origin_path, expression_start);
                values.add({value});
                if(text[*head] != ')') {
                    report_error(text, expression_start + *head, origin_path, "Expected closing parenthesis ')'");
                    return 0;
                }
                *head += 1;
            } else if(isalpha(c) || c == '_') {
                std::string name;
                int macro_start = *head;
                int parsed = parse_name(text, head, &name);

                if(name == "defined") {
                    parse_space(text, head);
                    bool has_paren = false;
                    if(text[*head] == '(') {
                        *head += 1;
                        has_paren = true;
                        parse_space(text, head);
                    }
                    parsed = parse_name(text, head, &name);
                    if(parsed == 0) {
                        report_error(text, expression_start + *head, origin_path, "Expected identifier after defined");
                        return 0;
                    }

                    if(has_paren) {
                        parse_space(text, head);
                        if (text[*head] == ')') {
                            *head += 1;
                        } else {
                            report_error(text, expression_start + *head, origin_path, "Expected closing parenthesis ')'");
                            return 0;
                        }
                    }
                    auto pair = context->macros.find(name);
                    values.add({pair != context->macros.end()});
                } else {
                    auto pair = context->macros.find(name);
                    if (pair == context->macros.end()) {
                        values.add({0});
                    } else {
                        parse_space(text, head);
                        // skip arguments since macro isn't defined
                        if (text[*head] == '(') {
                            *head += 1;
                            int depth = 1;
                            while(*head < text.size()) {
                                if(text[*head] == '(') {
                                    depth++;
                                } else if(text[*head] == ')') {
                                    depth--;
                                    if (depth == 0) {
                                        *head += 1;
                                        break;
                                    }
                                } else {
                                    *head += 1;
                                }
                            }
                        }
                        std::string macro_text = text.substr(macro_start, *head - macro_start);
                        expand_macros(context, macro_text, origin_path, true);
                        text = text.substr(0, macro_start) + macro_text + text.substr(*head);
                        *head = macro_start;
                        continue;
                    }
                }
            } else {
                // log::out << text << "\n";
                report_error(text, expression_start + *head, origin_path, "Bad value in #if expression.");
                return 0;
            }
            if(unary != 0) {
                auto& val = values.last().literal;
                switch(unary) {
                    case '-': val = -val; break;
                    case '!': val = !val; break;
                    case '~': val = ~val; break;
                    default: Assert(false);
                }
            }
            expect_primary = false;
            continue;
        } else {
            *head += 1;
            if(c == '+') {
                ops.add({c});
            } else if(c == '-') {
                ops.add({c});
            } else if(c == '*') {
                ops.add({c});
            } else if(c == '/') {
                ops.add({c});
            } else if(c == '%') {
                ops.add({c});
            } else if(c == '&' && c2 == '&') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '|' && c2 == '|') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '=' && c2 == '=') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '!' && c2 == '=') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '<' && c2 == '=') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '>' && c2 == '=') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '<' && c2 == '<') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '>' && c2 == '>') {
                *head += 1;
                ops.add({c, c2});
            } else if(c == '>') {
                ops.add({c});
            } else if(c == '<') {
                ops.add({c});
            } else if(c == '&') {
                ops.add({c});
            } else if(c == '|') {
                ops.add({c});
            } else if(c == '^') {
                ops.add({c});
            } else {
                *head -= 1;
                finalize = true;
            }
            
            expect_primary = true;
        }

        while (values.size() >= 2 && ops.size() > 0) {
            int op = 0;
            if(ops.size() >= 2) {
                int op0 = ops[ops.size()-2].kind;
                int op1 = ops[ops.size()-1].kind;

                if(precedence(op0) >= precedence(op1)) {
                    op = op0;
                    ops.removeAt(ops.size()-2);
                } else if (finalize) {
                    op = op1;
                    ops.pop();
                } else {
                    break;
                }
            } else if(finalize) {
                op = ops.last().kind;
                ops.pop();
            } else {
                break;
            }
            
            int val1 = values.last().literal;
            values.pop();
            int val0 = values.last().literal;

            int value = 0;
            switch(op) {
                case OP_AND: value = val0 && val1; break;
                case OP_OR: value = val0 || val1; break;
                case OP_EQUAL: value = val0 == val1; break;
                case OP_NOT_EQUAL: value = val0 != val1; break;
                case OP_LESS_EQUAL: value = val0 <= val1; break;
                case OP_GREATER_EQUAL: value = val0 >= val1; break;
                case '<': value = val0 < val1; break;
                case '>': value = val0 > val1; break;
                case OP_SHL: value = val0 << val1; break;
                case OP_SHR: value = val0 >> val1; break;
                case '+': value = val0 + val1; break;
                case '-': value = val0 - val1; break;
                case '*': value = val0 * val1; break;
                case '/': value = val0 / val1; break;
                case '%': value = val0 % val1; break;
                case '&': value = val0 & val1; break;
                case '|': value = val0 | val1; break;
                case '^': value = val0 ^ val1; break;
                default: Assert(false);
            }
            values.last() = {value};
        }
        if(finalize)
            break;
    }
    Assert(values.size() == 1);
    Assert(ops.size() == 0);
    CPARSER_VERBOSE(
    log::out << "val " << values[0].literal << " = " << text.substr(head_start, *head - head_start)<<"\n";
    )
    return values[0].literal;
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
            if(!context->should_skip())
                output_text += "//";
            head+=2;
            while (head < text.size()) {
                if(text[head] == '\n') {
                    head+=1;
                    if(!context->should_skip())
                        output_text += "\n";
                    break;
                }
                if(!context->should_skip())
                    output_text += text[head];
                head+=1;
            }
            continue;
        }
        if(head+1 < text.size() && text[head] == '/' && text[head+1] == '*') {
            if(!context->should_skip())
                output_text += "/*";
            head+=2;
            while (head < text.size()) {
                if(head+1 < text.size() && text[head] == '*' && text[head+1] == '/') {
                    head+=2;
                    if(!context->should_skip())
                        output_text += "*/";
                    break;
                }
                if(!context->should_skip())
                    output_text += text[head];
                head+=1;
            }
            continue;
        }
        // Preserve strings
        if (text[head] == '"') {
            if(!context->should_skip())
                output_text += "\"";
            head+=1;
            while (head < text.size()) {
                if(text[head] == '"' && (head-1 < 0 || text[head-1] != '\\')) {
                    head+=1;
                    if(!context->should_skip())
                        output_text += "\"";
                    break;
                }
                if(!context->should_skip())
                    output_text += text[head];
                head+=1;
            }
            continue;
        }
        if (text[head] == '\'') {
            if(!context->should_skip())
                output_text += "'";
            head+=1;
            while (head < text.size()) {
                if(text[head] == '\'' && (head-1 < 0 || text[head-1] != '\\')) {
                    head+=1;
                    if(!context->should_skip())
                        output_text += "'";
                    break;
                }
                if(!context->should_skip())
                    output_text += text[head];
                head+=1;
            }
            continue;
        }

        if (text[head] != '#') {
            if(context->should_skip()) {
                head++;
                continue;
            }
            // evaluate macros

            int macro_start = head;
            std::string name;
            int parsed_chars = parse_name(text, &head, &name);

            if (parsed_chars != 0) {
                auto pair = context->macros.find(name);
                if (pair == context->macros.end()) {
                    output_text += name;
                    continue;
                } else {
                    CPARSER_VERBOSE(
                    log::out << log::YELLOW <<"Matched " << name<<log::NO_COLOR<< "("<<calc_location(text, macro_start, origin_path)<<")\n";
                    )
                    parse_space(text, &head);

                    // skip arguments since macro isn't defined
                    if (text[head] == '(') {
                        head += 1;
                        int depth = 1;
                        while(head < text.size()) {
                            if(text[head] == '(') {
                                depth++;
                            } else if(text[head] == ')') {
                                depth--;
                                if (depth == 0) {
                                    head += 1;
                                    break;
                                }
                            } else {
                                head += 1;
                            }
                        }
                    }
                    std::string macro_text = text.substr(macro_start, head - macro_start);
                    context->current_pos = macro_start;
                    expand_macros(context, macro_text, origin_path, false);
                    output_text += macro_text;
                    continue;
                }
            }

            output_text += text[head];
            head++;
            continue;
        }
        head+=1;
        parse_space(text, &head); // "# ifdef" this does occur in headers

        int directive_start = head;
        std::string directive;
        int parsed_chars = parse_name(text, &head, &directive);
        if(parsed_chars == 0 && context->should_skip()) {
            // we probably parsed a concat/quotation hashtag in a macro definition
            continue;
        }
        if(parsed_chars == 0) {
            // parsed_chars = parse_name(text, &head, &directive);
            report_error(text, head, origin_path, "Expected directive name after #");
            return "";
        }
        parse_space(text, &head);
        if (directive == "ifdef" || directive == "ifndef" || directive == "if" || directive == "elif" || directive == "elifdef" || directive == "elifndef") {
            if (directive.substr(0,2) == "if")
                context->if_blocks.add({});
            auto& last_block = context->if_blocks.last();
            if(directive.substr(0,2) != "if" && last_block.in_else_block) {
                report_error(text, head, origin_path, "Syntax error #elif not allowed after #else");
                return "";
            }

            if (last_block.has_enabled_block || (context->if_blocks.size() >= 2 && context->if_blocks[context->if_blocks.size()-2].skip)) {
                // if previous if, elif block was enabled then always skip remaining elifs
                // if parent if block was skipped then we should always skip too
                last_block.skip = true;
            } else if (directive == "if" || directive == "elif") {
                int expr_start = head;
                while(head < text.size()) {
                    if(text[head] == '\\' && ((head+1 < text.size() && text[head+1] == '\n') || (head+2 < text.size() && text[head+1] == '\r' &&  text[head+2] == '\n'))) {
                        if(text[head+1] == '\r')
                            head++;
                        head+=1;
                    } else if(text[head] == '\n') {
                        head+=1;
                        break;
                    }
                    head+=1;
                }
                std::string expr_text = text.substr(expr_start, head - expr_start);
                int head = 0;
                context->current_pos = expr_start;
                int value = eval_expression(context, expr_text, &head, origin_path, expr_start);
                last_block.skip = value == 0;
                if(!last_block.skip)
                    last_block.has_enabled_block = true;
            } else {
                std::string name;
                parsed_chars = parse_name(text, &head, &name);
                if (parsed_chars == 0) {
                    report_error(text, head - parsed_chars, origin_path, "Syntax error?");
                    return "";
                }

                auto pair = context->macros.find(name);
                last_block.skip = pair == context->macros.end();
                if(directive == "ifndef" || directive == "elifndef")
                    last_block.skip = !last_block.skip;
                if(!last_block.skip)
                    last_block.has_enabled_block = true;
            }
            continue;
        } else if (directive == "else") {
            context->if_blocks.last().skip = !context->if_blocks.last().skip;
            context->if_blocks.last().in_else_block = true;
            // if parent if block has skip, the child if block whether in else or not should ALSO always skip
            if(context->if_blocks.last().has_enabled_block || (context->if_blocks.size() >= 2 && context->if_blocks[context->if_blocks.size()-2].skip)) {
                context->if_blocks.last().skip = true;
            }
            continue;
        } else if(directive == "endif") {
            context->if_blocks.pop();
            continue;
        }
        if(context->should_skip()) {
            continue;
        }

        if(directive == "define") {
            // parse macro definition
            int macro_name_start = head;
            std::string macro_name;
            int name_length = parse_name(text, &head, &macro_name);

            CPARSER_VERBOSE(
            log::out <<log::LIME<< "Define "<<macro_name << log::NO_COLOR<<" ("<<calc_location(text, macro_name_start, origin_path)<< ")\n";
            )

            int parsed_space = parse_space(text, &head);

            CMacro& macro = context->macros[macro_name] = {};
            macro.origin_file = origin_path;


            // Parse arguments
            if(text[head] != '(' || parsed_space) {

            } else {
                macro.has_params = true;
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
                    std::string param_name;
                    int parsed_chars = parse_name(text, &head, &param_name);
                    if(parsed_chars == 0) {
                        // nocheckin Syntax error
                        break;
                    }
                    macro.parameters.add(param_name);

                    parse_space(text, &head);

                    if (text[head] == ',') {
                        head+=1;
                        continue;
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
            macro.pos_in_file = head;
            while (head < text.size()) {
                // TODO: parse string
                if(text[head] == '/' && head+1 < text.size() && (text[head+1] == '/' || text[head+1] == '*'))
                    break;
                if(text[head] == '\n' && (head-1 < 0 || text[head-1] != '\\')) {
                    // head++;
                    break;
                }
                macro.content += text[head];
                head++;
            }
            continue;
        } else if (directive == "undef") {
            std::string name;
            int parsed_chars = parse_name(text, &head, &name);
            Assert(parsed_chars);
            context->macros.erase(name);
            continue;
        } else if (directive == "include") {
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
            CPARSER_VERBOSE(
            log::out << log::AQUA<<"Include " << log::NO_COLOR<<path << "\n";
            )
            std::string found_path="";
            for(int i=0;i<context->options->include_dirs.size();i++) {
                std::string real_path = context->options->include_dirs[i] + "/" + path;
                if (FileExist(real_path)) {
                    found_path = annihilate_forsaken_space_in_program_files(real_path);
                } else {
                    // log::out << " not found in " << context->options->include_dirs[i] << "\n";
                }
            }
            if (found_path.size() == 0) {
                // nocheckin File not found error
                log::out << "File not found " << path << "\n";
                continue;
            }
            bool found = false;
            bool has_pragma_once = false;
            for(int i=0;i<context->included_files.size();i++) {
                if(found_path == context->included_files[i].path) {
                    found = true;
                    if(context->included_files[i].pragma_once)
                        has_pragma_once = true;
                    break;
                }
            }
            if (!has_pragma_once) {
                u64 filesize;
                auto file = FileOpen(found_path, FILE_READ_ONLY, &filesize);
                if(!file) {
                    log::out << "File denied " << found_path << "\n";
                    // nocheckin File read denied
                    continue;
                }
                // log::out << "Process "<< found_path << "\n";
                std::string new_text{};
                new_text.resize(filesize);
                FileRead(file, (char*)new_text.data(), filesize);
                FileClose(file);

                if(!found) {
                    context->included_files.add({});
                    context->included_files.last().path = found_path;
                    // context->included_files.last().pragma_once = we don't know yet
                }
                auto prev_pos = context->current_pos;
                output_text += PreprocessText(context, new_text, found_path);
                context->current_pos = prev_pos;
            }
            continue;
        } else if(directive == "pragma") {
            std::string name;
            parse_name(text, &head, &name);

            if(name.size() == 0) {
                report_error(text, head, origin_path, "Invalid syntax for pragma. Missing pragma name/type.\n");
                return "";
            }
            
            if (name == "pack") {
                output_text += "#" + text.substr(directive_start, parsed_chars) + " " + name;
                continue;
            } else if (name == "push_macro") {
                parse_space(text, &head);
                if(text[head] != '(') {
                    report_error(text, head, origin_path, "Missing parenthesis\n");
                    return "";
                }
                head++;

                parse_space(text, &head);
                
                std::string name;
                parse_string(text, &head, &name);
                
                parse_space(text, &head);

                if(text[head] != ')') {
                    report_error(text, head, origin_path, "Missing parenthesis\n");
                    return "";
                }
                head++;

                auto pair = context->stacked_macros.find(name);
                if(pair == context->stacked_macros.end()) {
                    context->stacked_macros[name] = {};
                }
                auto& list = context->stacked_macros[name];
                auto pair_m = context->macros.find(name);
                if(pair_m == context->macros.end()) {
                    list.add({});
                } else {
                    list.add(pair_m->second);
                    context->macros[name] = {};
                }
                continue;
            } else if (name == "pop_macro") {
                parse_space(text, &head);
                if(text[head] != '(') {
                    report_error(text, head, origin_path, "Missing parenthesis\n");
                    return "";
                }
                head++;
                parse_space(text, &head);

                std::string name;
                parse_string(text, &head, &name);

                parse_space(text, &head);

                if(text[head] != ')') {
                    report_error(text, head, origin_path, "Missing parenthesis\n");
                    return "";
                }
                head++;

                auto pair = context->stacked_macros.find(name);
                if(pair != context->stacked_macros.end()) {
                    context->macros[name] = pair->second.last();
                    pair->second.pop();
                }
                continue;
            } else if (name == "once") {
                bool found = false;
                for(int i=0;i<context->included_files.size();i++) {
                    if(origin_path == context->included_files[i].path) {
                        found = true;
                        context->included_files[i].pragma_once = true;
                        break;
                    }
                }
                Assert(found);
                continue;
            } else {
                report_error(text, head, origin_path, "Missing support pragma\n");
                return "";
            }
        } else if (directive == "error") {
            int err_start = head;
            while(head < text.size()) {
                if(text[head] == '\n') {
                    head++;
                    break;
                }
                head++;
            }
            log::out << log::GRAY << calc_location(text, directive_start, origin_path) << "\n";
            log::out << log::RED << "error: " << log::NO_COLOR<< text.substr(err_start, head - err_start) << "\n";
            continue;
        } else if (directive == "warning") {
            int err_start = head;
            while(head < text.size()) {
                if(text[head] == '\n') {
                    head++;
                    break;
                }
                head++;
            }
            log::out << log::GRAY << calc_location(text, directive_start, origin_path) << "\n";
            log::out << log::GOLD << "warning: " << log::NO_COLOR<< text.substr(err_start, head - err_start) << "\n";
            continue;
        }
        report_error(text, head, origin_path, "Unknown directive '"+directive+"'\n");
        return "";
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