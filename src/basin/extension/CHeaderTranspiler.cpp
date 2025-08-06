#include "basin/extension/CHeaderTranspiler.h"

#include "Engone/PlatformLayer.h"
#include "Engone/Logger.h"
#include "basin/CompilerOptions.h"
#include "tracy/Tracy.hpp"
#include "string.h"

#define CPARSER_VERBOSE(X) 
// #define CPARSER_VERBOSE(X) X

// TODO: We should get this from compile options and target.
std::string annihilate_forsaken_space_in_program_files(const std::string& path);

engone::Logger& operator<<(engone::Logger& logger, const clexer::Token& tok) {
    using namespace clexer;
    if(tok.kind < END_OF_FILE)
        return logger << (char)tok.kind;
    else 
        return logger << tok.data;
}

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

    ZoneScopedC(tracy::Color::Bisque1);

    ParserContext context{};
    context.origin_path = path;
    context.compile_options = compile_options;
    CPreprocContext& preproc = context.preproc;
    // for (int i=0;i<options->include_dirs.size();i++) {
    //     log::out << options->include_dirs[i] << "\n";
    // }
    /*
        Predefined macros
    */
   preproc.macros["__STDC__"] = {"1"};
   preproc.macros["__STDC_VERSION__"] = {"201112"};
    // TODO: Don't assume gnu? we may link with clang or msvc
    preproc.macros["__GNUC__"] = {"14"};
    preproc.macros["_DLL"] = {};
    
    // GCC behaviour defining declspec and cdecl and stuff
    auto& macro_declspec = preproc.macros["__declspec"] = {"__attribute__((X))"};
        macro_declspec.has_params = true;
        macro_declspec.parameters.add("X");
    preproc.macros["__cdecl"] = {"__attribute__((__cdecl__))"};
    preproc.macros["__stdcall"] = {"__attribute__((__stdcall__))"};
    preproc.macros["__fastcall"] = {"__attribute__((__fastcall__))"};

    switch(compile_options->target) {
        // https://github.com/cpredef/predef/blob/master/Architectures.md
        case TARGET_WINDOWS_x64: {
            preproc.macros["_WIN32"] = {"1"};
            preproc.macros["_WIN64"] = {"1"};
            preproc.macros["__x86_64__"] = {};
        } break;
        case TARGET_LINUX_x64: {
            preproc.macros["__linux__"] = {};
            preproc.macros["__x86_64__"] = {"1"};
        } break;
        case TARGET_AARCH64: {
            preproc.macros["__aarch64__"] = {};
        } break;
        case TARGET_ARM: {
            preproc.macros["__arm__"] = {};
        } break;
        default: {
            // TODO: Print the line where we imported the C header.
            log::out << log::YELLOW << "No predefined macros when importing C header on target '"<<compile_options->target<<"'.\n";
        }
    }

    for(int i=0;i<options->c_defines.size();i++) {
        preproc.macros[options->c_defines[i]] = {"1"};
    }
    context.REGISTER_SIZE = compile_options->arch.REGISTER_SIZE;

    preproc.options = options;
    std::string stoff = PreprocessText(&preproc, text, annihilate_forsaken_space_in_program_files(path));
    
    // TODO: Debug feature, writing to temp.h
    int slash = path.rfind("/");
    std::string temp_path = path.substr(slash+1);
    auto f = FileOpen("bin/int/" + temp_path, FILE_CLEAR_AND_WRITE);
    Assert(f);
    FileWrite(f, stoff.c_str(), stoff.size());
    FileClose(f);
    
    context.lexer.lex_tokens(stoff);
    // for(int i=0;i<tokens.size();i++) {
    //     auto& tok = tokens[i];
    //     context.write(tok);
    // }
    // context.cur_column = 1;
    // context.head = 0;
    // context.cur_line = 1;
    context.parse_top();

    context.walk();

    // TODO: Any memory we need to free or does destructors do that already?

    return context.output;
}

#pragma region preproc
int parse_space(const std::string& text, int* head) {
    Assert(head);
    int start = *head;
    while(*head < text.size()) {
        char c = text[*head];
        if(c != ' ' && c != '\t') {
        // if(c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        *head += 1;
    }
    return *head - start;
}

int parse_name(const std::string& text, int* head, std::string* name) {
    Assert(head);
    int start = *head;
    while(*head < text.size()) {
        char c = text[*head];
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
        *name = text.substr(start, *head - start);
    }
    return *head - start;
}
int parse_string(const std::string& text, int* head, std::string* name) {
    Assert(head);
    Assert(name);

    if(text[*head] != '"') {
        return 0;
    }
    *head += 1;

    int start = *head;
    while(*head < text.size()) {
        char c = text[*head];
        if(c == '"') {
            *head += 1;
            break;
        }
        *head += 1;
    }
    *name = text.substr(start, *head - start - 1);
    return *head - start;
}
// DOES NOT RETURN PARSED INTEGER, check 'value' instead
int parse_int(const std::string& text, int* head, int* value) {
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
    *value = strtol(text.data() + *head, &end_ptr, 0);
    *head = (u64)end_ptr - (u64)text.data();

    while(*head < text.size()) {
        char c = text[*head];
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
int parse_comment(const std::string& text, int* head, std::string* comment) {
    if(comment)
        *comment = {};
    
    int start = *head;

    if(*head + 1 >= text.size())
        return 0;

    if(text[*head] == '/' && text[*head+1] == '/') {
        *head += 2;
        while(*head < text.size()) {
            char c = text[*head];
            if (c == '\n') {
                break;
            }
            *head += 1;
        }
        *comment = std::string(text.data() + start, *head - start);
        return *head - start;
    } else if(text[*head] == '/' && *head + 1 < text.size() && text[*head+1] == '*') {
        *head += 2;
        while(*head + 1 < text.size()) {
            char c = text[*head];
            if (c == '*' && text[*head + 1] == '/') {
                *head += 2;
                break;
            }
            *head += 1;
        }
        *comment = std::string(text.data() + start, *head - start);
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
    expr        := ternary_expr
    ternary_expr := or_expr ? or_expr : or_expr
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

    #define OP_AND            ('&' | ('&'<<8))
    #define OP_OR             ('|' | ('|'<<8))
    #define OP_SHL            ('<' | ('<'<<8))
    #define OP_SHR            ('>' | ('>'<<8))
    #define OP_EQUAL          ('=' | ('='<<8))
    #define OP_NOT_EQUAL      ('!' | ('='<<8))
    #define OP_LESS_EQUAL     ('<' | ('='<<8))
    #define OP_GREATER_EQUAL  ('>' | ('='<<8))

    auto precedence = [&](int kind) {
        switch(kind) {
            case '?':
            case ':':
                return -3;
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
                                }
                                *head += 1;
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
            } else if(c == '?') {
                ops.add({c});
            } else if(c == ':') {
                ops.add({c});
            } else {
                *head -= 1;
                finalize = true;
            }
            
            expect_primary = true;
        }

       // log::out << "ops " << ops.size() << " " << values.size() << "\n";

        while (values.size() >= 2 && ops.size() > 0) {
            int op = 0;
            if(ops.size() >= 2) {
                int op0 = ops[ops.size()-2].kind;
                int op1 = ops[ops.size()-1].kind;
                // log::out << (char)op0 << " " << (char)op1 << "\n";

                int opm3 = 0;
                if(ops.size() >= 3)
                    opm3 = ops[ops.size()-3].kind;

                if(op0 == '?' && op1 == ':') {
                    if (!finalize) {
                        break;
                    }
                    // perform ternary operation
                    // how about nested ternary operation.
                    op = '?';
                    ops.pop();
                    ops.pop();
                } else if(precedence(op0) >= precedence(op1) && op0 != '?') {
                    if (opm3 == '?' && op0 == ':') {
                        op = '?';
                        ops.removeAt(ops.size()-2);
                        ops.removeAt(ops.size()-2);
                    } else {
                        op = op0;
                        ops.removeAt(ops.size()-2);
                    }
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
                case '?': {
                    values.pop();
                    int val_cond = values.last().literal;
                    // log::out << "cond " << val_cond << " " << val0 << " " << val1 << "\n";
                    value = val_cond ? val0 : val1;
                    break;
                }
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
    ZoneScopedC(tracy::Color::Bisque4);
    // ifdef, macros, include, pragma once, pragma pack push/pop
    // include dirs, pre-defines
    // CPreprocContext context;

    std::string output_text = "";

    output_text += "// include " + origin_path + "\n";

    bool preserve_comments = false;

    context->options->readBytes += text.size();

    context->options->lines++;

    // find directives and expand macros
    // string
    // comments
    // backslash?
    int head = 0;
    while (head < text.size()) {
        if(text[head] == '\r') {
            head++;
            continue;
        }

        // Preserve comments
        if(head+1 < text.size() && text[head] == '/' && text[head+1] == '/') {
            if(!context->should_skip() && preserve_comments)
                output_text += "//";
            head+=2;
            while (head < text.size()) {
                if(text[head] == '\n') {
                    context->options->comment_lines++;
                    context->options->lines++;
                    head+=1;
                    if(!context->should_skip() && preserve_comments)
                        output_text += "\n";
                    break;
                }
                if(!context->should_skip() && preserve_comments)
                    output_text += text[head];
                head+=1;
            }
            continue;
        }
        if(head+1 < text.size() && text[head] == '/' && text[head+1] == '*') {
            if(!context->should_skip() && preserve_comments)
                output_text += "/*";
            head+=2;
            while (head < text.size()) {
                if(text[head] == '\n') {
                    context->options->comment_lines++;
                    context->options->lines++;
                }

                if(head+1 < text.size() && text[head] == '*' && text[head+1] == '/') {
                    head+=2;
                    if(!context->should_skip() && preserve_comments)
                        output_text += "*/";
                    break;
                }
                if(!context->should_skip() && preserve_comments)
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
                if(text[head] == '\n') {
                    context->options->lines++;
                }
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
                            if(text[head] == '\n') {
                                context->options->lines++;
                            }
                            if(text[head] == '(') {
                                depth++;
                            } else if(text[head] == ')') {
                                depth--;
                                if (depth == 0) {
                                    head += 1;
                                    break;
                                }
                            }
                            head+=1;
                        }
                    }
                    std::string macro_text = text.substr(macro_start, head - macro_start);
                    context->current_pos = macro_start;
                    expand_macros(context, macro_text, origin_path, false);
                    output_text += macro_text;
                    continue;
                }
            }


            if(output_text.size() > 1 && output_text[output_text.size()-2] == '\n' && output_text.back() == '\n' && text[head] == '\n') {
                // skip consecutive newlines
                context->options->blank_lines++;
            } else {
                if(text[head] == '\n') {
                    context->options->lines++;
                }
                output_text += text[head];
            }
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
                        context->options->lines++;
                        if(text[head+1] == '\r')
                            head++;
                        head+=1;
                    } else if(text[head] == '\n') {
                        context->options->lines++;
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
            log::out <<log::LIME<< "Define "<<macro_name << "  " << context->macros.size() << log::NO_COLOR<<" ("<<calc_location(text, macro_name_start, origin_path)<< ")\n";
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
                if(text[head] == '\n') {
                    context->options->lines++;
                }
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
                log::out << log::YELLOW << "WARNING: " << log::NO_COLOR << "File not found " << path << "\n";
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
                    context->options->lines++;
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
                    context->options->lines++;
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
#pragma endregion preproc

namespace clexer {

    
    void LexerContext::lex_tokens(const std::string& text) {
        using namespace engone;
        // #define isalnum(chr) (((chr|32) >= 'a' && (chr|32) <= 'z') || (chr >= '0' && chr <= '9') || chr == '_')

        int head = 0;
        int line = 1;
        int column = 1;
        while(head < text.size()) {
            char chr = text[head];
            char chr2 = 0;
            if(head < text.size())
                chr2 = text[head+1];
            head++;

            if(chr == '/' && chr2 == '/') {
                head++;
                while(head < text.size() && text[head] != '\n') {
                    head++;
                }
                continue;
            }
            if(chr == '/' && chr2 == '*') {
                head++;
                while(head+1 < text.size()) {
                    if(text[head] == '*' && text[head+1] == '/') {
                        head++;
                        break;
                    }
                    head++;
                }
                head++;
                continue;
            }
            
            if(chr == '\n') {
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

            if(isalpha(chr) || chr == '_') {
                int start = head-1;
                while(isalnum(text[head]) || text[head] == '_') {
                    head++;
                }
                int end = head;
                Token tok{};
                std::string word = text.substr(start, end-start);
                // log::out << "WORD "<<word<<"\n";
                if(word == "const") tok.kind = CONST;
                else if(word == "typedef") tok.kind = TYPEDEF;
                else if(word == "struct") tok.kind = STRUCT;
                else if(word == "enum") tok.kind = ENUM;
                else if(word == "union") tok.kind = UNION;
                else if(word == "__attribute__") {
                    tok.kind = ATTRIBUTE;
                    while(head < text.size() && isspace(text[head])) {
                        head++;
                    }
                    if(head+1 < text.size() && text[head] == '(' && text[head+1] == '(') {
                        head+=2;
                        int word_start = head;
                        int paren_depth =  0;
                        while(head < text.size()) {
                            char chr = text[head];
                            if(text[head] == '(') {
                                paren_depth++;
                            } else if(text[head] == ')') {
                                paren_depth--;
                                if(paren_depth == -2) {
                                    head++;
                                    break;
                                }
                            }
                            head++;
                        }
                        end = head;
                        tok.data = text.substr(word_start, head-2 - word_start);
                    }
                }
                else if(word == "extern") tok.kind = EXTERN;
                else if(word == "__extension__" || word == "restrict" || word == "volatile") {
                    // Skip
                    head = end;
                    column += end - start;
                    continue;
                }
                else {
                    tok.kind = IDENTIFIER;
                    tok.data = word;
                }
                tok.line = line;
                tok.column = column;
                tokens.add(tok);
                head = end;
                column += end - start;
                continue;
            }
            if((chr == '0' && chr2 == 'x')) {
                int start = head-1;
                head++; // skip x
                while(isdigit(text[head]) || ((text[head]|32) >= 'a' && (text[head]|32) <= 'f')) {
                    head++;
                }
                int end = head;
                Token tok{};
                tok.kind = NUMBER;
                tok.data = text.substr(start, end-start);
                tok.line = line;
                tok.column = column;
                tokens.add(tok);
                head = end;
                column += end - start;
                continue;
            } else if(isdigit(chr)) {
                int start = head-1;
                // Handle float
                while(isdigit(text[head])) {
                    head++;
                }
                int end = head;
                Token tok{};
                tok.kind = NUMBER;
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
                while(true) {
                    char chr = text[head];
                    head++;
                    // handle escaped quotes
                    if(chr == '"')
                        break;
                }
                int end = head;
                Token tok{};
                tok.kind = STRING;
                tok.data = text.substr(start, end-start);
                tok.line = line;
                tok.column = column;
                tokens.add(tok);
                head = end;
                column += end - start;
                continue;
            }

            Token tok{};
            tok.kind = (TokenKind)chr;
            tok.line = line;
            tok.column = column;
            column += 1;
            tokens.add(tok);
            continue;
        }
    }

    CRoot* ParserContext::parse_top() {
        using namespace engone;

        // TODO: 32-bit will have 4 bytemight be a different
        pack_stack.add(8); // default is 8-byte packing

        bool result = false;
        int head = 0;
        while (true) {
            result = false;
            int head_start = head;
            auto& token = gettok(head);

            if (token.kind == END_OF_FILE) {
                break;
            }
            #define CHECK_FAIL if(!result) goto parse_fail;

            if(token.kind == '#') {
                if (gettok(head+1).data == "pragma" && gettok(head+2).data == "pack") {
                    Token num;
                    int pack_value;
                    head += 3;
                    result = match(head, '(');
                    if(!result) goto cant_handle_pragma_pack;
                    
                    num = gettok(head);
                    result = match(head, IDENTIFIER);
                    if(!result) goto cant_handle_pragma_pack;

                    if(num.data == "push") {
                    
                        if(!result) goto cant_handle_pragma_pack;
                        result = match(head, ',');
                        
                        num = gettok(head);
                        result = match(head, NUMBER);
                        if(!result) goto cant_handle_pragma_pack;
                        
                        char* endptr;
                        pack_value = strtol(num.data.c_str(), &endptr, 0);
                        // pack_value = atoi(num.data.c_str());

                        if(pack_value != 8 && pack_value != 1) {
                            log::out << "ERROR: Can't handle struct packing "<<pack_value<<". BTB only supports 8-byte and 1-byte packing." << "\n";
                            goto parse_fail;
                        }
                        result = match(head, ')');
                        if(!result) goto cant_handle_pragma_pack;
                        
                        pack_stack.add(pack_value);
                    } else if (num.data == "pop"){
                        result = match(head, ')');
                        if(!result) goto cant_handle_pragma_pack;

                        if(pack_stack.size() == 0) {
                            log::out << "WARNING: C parser pack stack is empty, line " << token.line << "\n";
                        }
                        pack_stack.pop();
                    } else {
                        result = false;
                    }

                    if(!result) {
                        cant_handle_pragma_pack:
                        log::out << "ERROR: Can't handle syntax for #pragma pack, line " << token.line << "\n";
                        CHECK_FAIL
                    }
                }
            }
            if(token.kind == TYPEDEF) {
                head++;
                CType* type;
                result = parse_type(head, &type); // function pointers are special
                if(!result) {
                    log::out << "ERROR: bad type?, line " << token.line << "\n";
                }
                CHECK_FAIL

                std::string type_name;
                if(!type->obj_func)  {
                    CTypedef* node = create_typedef();
                    node->type = type;
                    if(node->type->obj && node->type->obj->name.size()) {
                        node->typeNames.add({node->type->obj->name});
                    }
                    while(true) {
                        int ptr_level = 0;
                        while(true) {
                            auto tok = &gettok(head);
                            if(tok->kind != '*') {
                                break;
                            }
                            head++;
                            ptr_level++;
                        }
                        auto& token_id= gettok(head);
                        result = match(head, IDENTIFIER);
                        if(!result) {
                            log::out << "ERROR: bad type2?, line " << token.line << "\n";
                        }
                        CHECK_FAIL
                        type_name = token_id.data;
                        
                        if(type->obj || type->obj_enum || type->obj_func || type->name != type_name) {
                            node->typeNames.add({});
                            node->typeNames.last().name = type_name;
                            node->typeNames.last().ptr_level = ptr_level;
                        } else {
                            type->name = "void";
                            node->typeNames.add({});
                            node->typeNames.last().name = type_name;
                            node->typeNames.last().ptr_level = 0;
                            node->weak_void_type = true; // TODO: Weak type means we have: typedef struct SomeType SomeType; In GLFW this is how opaque objects are declared.
                            // However, in C you can define the struct later in which case we shouldn't add: #macro SomeType void
                        }
                        set_identifier(type_name, type);
                        
                        auto tok = &gettok(head);
                        if(tok->kind != ',') {
                            break;
                        }
                        head++; // skip ,
                    }
                    Assert(!node->weak_void_type || node->typeNames.size() == 1);
                    root.nodes.add(node);
                    
                    // TODO: Not sure if we handle this case when we need to find identifier for type size.
                    //    typedef struct Hello { int x; } Hello;

                    // if(type->obj && type->obj->name.size()) {
                    //     set_identifier(name, obj);
                    // }
                } else {
                    type_name = type->obj_func->name;

                    // TODO: Refactor
                    if(type->obj || type->obj_enum || type->obj_func || type->name != type_name) {
                        CTypedef* node = create_typedef();
                        node->typeNames.add({});
                        node->typeNames.last().name = type_name;
                        node->typeNames.last().ptr_level = 0;
                        node->type = type;
                        root.nodes.add(node);
                    } else {
                        type->name = "void";
                        CTypedef* node = create_typedef();
                        node->typeNames.add({});
                        node->typeNames.last().name = type_name;
                        node->typeNames.last().ptr_level = 0;
                        node->type = type;
                        node->weak_void_type = true;
                        root.nodes.add(node);
                    }
                    set_identifier(type_name, type);
                }
                
                result = match(head, ';');
                CHECK_FAIL
            } else if(token.kind == STRUCT) {
                head++;

                std::string name = gettok(head).data;

                result = match(head, IDENTIFIER);
                CHECK_FAIL

                if(gettok(head).kind == ';') {
                    // skip declaration
                    head++;
                    continue;
                }

                result = match(head, '{');
                CHECK_FAIL
                
                CStruct* obj = create_struct();
                obj->packing = pack_stack.last();
                obj->name = name;
                
                result = parse_struct_fields(head, obj);
                CHECK_FAIL
                
                obj->calculate_size();

                set_identifier(name, obj);
                
                result = match(head, '}');
                CHECK_FAIL
                result = match(head, ';');
                CHECK_FAIL

                mark_strong(name);

                root.nodes.add(obj);
            } else if(token.kind == UNION) {
                // TODO: BTB doesn't support unions so we use the first field in the union.
                head++;

                std::string name = gettok(head).data;

                result = match(head, IDENTIFIER);
                CHECK_FAIL

                if(gettok(head).kind == ';') {
                    // skip declaration
                    head++;
                    continue;
                }

                result = match(head, '{');
                CHECK_FAIL
                
                CStruct* obj = create_struct();
                obj->packing = pack_stack.last();
                obj->name = name;
                
                result = parse_struct_fields(head, obj);
                CHECK_FAIL
                
                obj->union_ify();
                obj->calculate_size();
                
                set_identifier(name, obj);

                result = match(head, '}');
                CHECK_FAIL
                result = match(head, ';');
                CHECK_FAIL

                mark_strong(name);

                root.nodes.add(obj);
            } else {
                // assume function or variable

                bool has_dllimport = false;
                if(token.kind == ATTRIBUTE) {
                    head++;
                    if(token.data == "dllimport") {
                        has_dllimport = true;
                    } else {
                        // Some attribute we don't recorgnize.
                        // Skip function/variable to be safe.
                        goto parse_fail;
                    }
                }
                
                bool has_extern = false;
                if(gettok(head).kind == EXTERN) {
                    head++;
                    has_extern = true;
                }

                if(gettok(head).kind == IDENTIFIER && gettok(head).data == "static") {
                    // static functions are not supported.
                    goto parse_fail;
                }

                CType* type;
                result = parse_type(head, &type);
                CHECK_FAIL

                int convention = 0;
                auto& tok2 = gettok(head);
                if(tok2.kind == ATTRIBUTE) {
                    head++;
                    if(tok2.data == "__stdcall__" || tok2.data == "__cdecl__") {
                        // OI, is __cdecl__ okay because BTB can't generate cdecl?
                        convention = 1;
                        // nocheckin TODO: do something with calling convention
                    } else {
                        // Some attribute we don't recorgnize.
                        // Skip function/variable to be safe.
                        goto parse_fail;
                    }
                }
                
                auto& token_id = gettok(head);
                result = match(head, IDENTIFIER);
                CHECK_FAIL

                if(gettok(head).kind == ';') {
                    head++;
                    // variable
                    if(has_extern) {
                        CVariable* var = create_variable();
                        var->name = token_id.data;
                        var->type = type;
                        root.nodes.add(var);
                    } else {
                        // variables not marked extern are defined variables.
                        // not declared as coming from static or dynamic library.
                        // We do not want to create global variables.
                    }
                } else if(gettok(head).kind == '(') {
                    head++;
                    
                    CFunction* func = create_function();
                    result = parse_function_parameters(head, func);
                    CHECK_FAIL
                    
                    result = match(head, ')');
                    CHECK_FAIL
                    
                    result = match(head, ';');
                    CHECK_FAIL
                    
                    func->name = token_id.data;
                    if(type->name != "void")
                        func->return_type = type;
                    root.nodes.add(func);

                }
            }

            if (!result) {
            parse_fail:
                // log::out << "Skipping " << gettok(head_start) <<" at "<< gettok(head_start).line << "\n";
                head = head_start;
                skip_construct(head);
            }
        }
        return &root;
    }
    
    void ParserContext::skip_construct(int& head) {
        int paren_depth = 0;
        int bracket_depth = 0;
        int curly_depth = 0;
        int start = head;

        while(true) {
            auto& tok = gettok(head);
            head++;

            if(tok.kind == END_OF_FILE)
                break;

            if(tok.kind == ';' && paren_depth == 0 && bracket_depth == 0 && curly_depth == 0) {
                break;
            }
            if(tok.kind == '(') {
                paren_depth++;
            } else if(tok.kind == ')') {
                paren_depth--;
            } else if(tok.kind == '{') {
                curly_depth++;
            } else if(tok.kind == '}') {
                curly_depth--;
                if(curly_depth == 0) {
                    // end of function definition body
                    break;
                }
            } else if(tok.kind == '[') {
                bracket_depth++;
            } else if(tok.kind == ']') {
                bracket_depth--;
            }
        }
        int end = head;
    }
    bool ParserContext::parse_literal(int& head, i64* number) {
        using namespace engone;

        /* Some grammar
        expr        := ternary_expr
        ternary_expr := or_expr ? or_expr : or_expr
        or_expr     := and_expr ( "||" and_expr )*
        and_expr    := equality_expr ( "&&" equality_expr )*
        equality_expr := rel_expr ( ("==" | "!=") rel_expr )*
        rel_expr    := add_expr ( ("<" | ">" | "<=" | ">=") add_expr )*
        add_expr    := mul_expr ( ("+" | "-") mul_expr )*
        mul_expr    := unary_expr ( ("*" | "/" | "%") unary_expr )*
        unary_expr  := ("!" | "~" | "-" | "defined") unary_expr | primary
        primary     := integer | identifier | "(" expr ")"
        */
        bool result;

        struct Value {
            i64 literal;
        };
        struct Op {
            Op(TokenKind a=(TokenKind)0, TokenKind b = (TokenKind)0) : kind((int)a | ((int)b << 8)) {
                
            }
            int kind;
        };
        DynamicArray<Value> values;
        DynamicArray<Op> ops;

        //  defined in 
        // #define OP_AND            ('&' | ('&'<<8))
        // #define OP_OR             ('|' | ('|'<<8))
        // #define OP_SHL            ('<' | ('<'<<8))
        // #define OP_SHR            ('>' | ('>'<<8))
        // #define OP_EQUAL          ('=' | ('='<<8))
        // #define OP_NOT_EQUAL      ('!' | ('='<<8))
        // #define OP_LESS_EQUAL     ('<' | ('='<<8))
        // #define OP_GREATER_EQUAL  ('>' | ('='<<8))

        auto precedence = [&](int kind) {
            switch(kind) {
                case '?':
                case ':':
                    return -3;
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
        // int head_start = *head;
        bool finalize = false;
        bool expect_primary = true;
        while (true) {
            auto tok = &gettok(head);
            auto tok2 = &gettok(head+1);
            // auto tok3 = &gettok(head+2);

            // don't think we need to handle comment?


            // __fd_mask __fds_bits[1024 / (8 * (int) sizeof (__fd_mask))];

            if (expect_primary) {
                TokenKind unary = END_OF_FILE;
                if(tok->kind == '-' || tok->kind == '!' || tok->kind == '~') {
                    head++;
                    unary = tok->kind;
                    
                    tok = &gettok(head);
                    tok2 = &gettok(head+1);
                }

                if(tok->kind == NUMBER) {
                    head++;
                    char* endptr;
                    i64 value = strtoll(tok->data.c_str(), &endptr, 0);
                    values.add({(int)value});
                } else if (tok->kind == '(') {
                    head++;
                    // nocheckin casting
                    int head_start = head;
                    CType* type;
                    result = parse_type(head, &type);
                    if(result) {
                        // type cast, we ignore them for now.
                        // we assume the cast is an integer.
                        // We need to properly cast between short,int,long if code
                        // relies on integer overflow.
                        if(gettok(head).kind != ')') {
                            return false;
                        }
                        head++;
                        continue; // parse primary again, cast is not a primary.
                    } else {
                        head = head_start;
                        i64 value;
                        result = parse_literal(head, &value);
                        if(!result) return false;
                        values.add({value});
                    }
                    if(gettok(head).kind != ')') {
                        return false;
                        // report_error(text, expression_start + *head, origin_path, "Expected closing parenthesis ')'");
                        // return 0;
                    }
                    head++;
                } else if(tok->kind == IDENTIFIER) {
                    if(tok->data == "sizeof") {
                        head++;

                        if(gettok(head).kind != '(') {
                            return false;
                        }
                        head++;

                        CType* type;
                        result = parse_type(head, &type);
                        if(!result) return false;

                        if(gettok(head).kind != ')') {
                            return false;
                        }
                        head++;

                        values.add({type->size});
                    } else {
                        // we don't handle constants
                        // Code usually uses macros for contant literals so it should be fine in most cases.
                        return false;
                    }
                } else {
                    return false;
                    // report_error(text, expression_start + *head, origin_path, "Bad value in #if expression.");
                }
                if(unary != END_OF_FILE) {
                    auto& val = values.last().literal;
                    switch((int)unary) {
                        case '-': val = -val; break;
                        case '!': val = !val; break;
                        case '~': val = ~val; break;
                        default: Assert(false);
                    }
                }
                expect_primary = false;
                continue;
            } else {
                head++;
                if(tok->kind == '+') {
                    ops.add({tok->kind});
                } else if(tok->kind == '-') {
                    ops.add({tok->kind});
                } else if(tok->kind == '*') {
                    ops.add({tok->kind});
                } else if(tok->kind == '/') {
                    ops.add({tok->kind});
                } else if(tok->kind == '%') {
                    ops.add({tok->kind});
                } else if(tok->kind == '&' && tok2->kind == '&') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '|' && tok2->kind == '|') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '=' && tok2->kind == '=') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '!' && tok2->kind == '=') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '<' && tok2->kind == '=') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '>' && tok2->kind == '=') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '<' && tok2->kind == '<') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '>' && tok2->kind == '>') {
                    head++;
                    ops.add({tok->kind, tok2->kind});
                } else if(tok->kind == '>') {
                    ops.add({tok->kind});
                } else if(tok->kind == '<') {
                    ops.add({tok->kind});
                } else if(tok->kind == '&') {
                    ops.add({tok->kind});
                } else if(tok->kind == '|') {
                    ops.add({tok->kind});
                } else if(tok->kind == '^') {
                    ops.add({tok->kind});
                } else if(tok->kind == '?') {
                    ops.add({tok->kind});
                } else if(tok->kind == ':') {
                    ops.add({tok->kind});
                } else {
                    head--;
                    finalize = true;
                }
                
                expect_primary = true;
            }

            // log::out << "ops " << ops.size() << " " << values.size() << "\n";

            while (values.size() >= 2 && ops.size() > 0) {
                int op = 0;
                if(ops.size() >= 2) {
                    int op0 = ops[ops.size()-2].kind;
                    int op1 = ops[ops.size()-1].kind;
                    // log::out << (char)op0 << " " << (char)op1 << "\n";

                    int opm3 = 0;
                    if(ops.size() >= 3)
                        opm3 = ops[ops.size()-3].kind;

                    if(op0 == '?' && op1 == ':') {
                        if (!finalize) {
                            break;
                        }
                        // perform ternary operation
                        // how about nested ternary operation.
                        op = '?';
                        ops.pop();
                        ops.pop();
                    } else if(precedence(op0) >= precedence(op1) && op0 != '?') {
                        if (opm3 == '?' && op0 == ':') {
                            op = '?';
                            ops.removeAt(ops.size()-2);
                            ops.removeAt(ops.size()-2);
                        } else {
                            op = op0;
                            ops.removeAt(ops.size()-2);
                        }
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
                
                i64 val1 = values.last().literal;
                values.pop();
                i64 val0 = values.last().literal;
                i64 value = 0;
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
                    case '?': {
                        values.pop();
                        i64 val_cond = values.last().literal;
                        // log::out << "cond " << val_cond << " " << val0 << " " << val1 << "\n";
                        value = val_cond ? val0 : val1;
                        break;
                    }
                    default: Assert(false);
                }
                values.last() = {value};
            }
            if(finalize)
                break;
        }
        Assert(values.size() == 1);
        Assert(ops.size() == 0);
        // CPARSER_VERBOSE(
        // log::out << "parse val " << values[0].literal << " = " << text.substr(head_start, *head - head_start)<<"\n";
        // )
        Assert(number);
        *number = values[0].literal;
        return true;
    }

    bool ParserContext::parse_struct_fields(int& head, CStruct* obj) {
        using namespace engone;
        bool result = false;
        // TODO: Handle attribute
        while(true) {
            auto& token = gettok(head);
            if(token.kind == END_OF_FILE)
                return false;
            if (token.kind == '}') {
                break;
            }

            CType* type;
            result = parse_type(head, &type);
            if(!result) return false;

            auto& id = gettok(head);
            result = match(head, IDENTIFIER);
            if(!result) return false;

            auto tok = &gettok(head);
            if(tok->kind == '[') {
                head++;
                i64 value;
                result = parse_literal(head, &value);
                if(!result) return false;
                
                type->name += "[" + std::to_string(value) + "]";

                result = match(head, ']');
                if(!result) return false;
            }

            obj->fields.add({});
            obj->fields.last().name = id.data;
            obj->fields.last().type = type;

            result = match(head, ';');
            if(!result) return false;
        }
        return true;
    }
    bool ParserContext::parse_enum_fields(int& head, CEnum* obj) {
        using namespace engone;
        bool result = false;
        // TODO: Handle attribute
        while(true) {
            auto& token = gettok(head);
            if(token.kind == END_OF_FILE)
                return false;
            if (token.kind == '}') {
                break;
            }

            auto& id = gettok(head);
            result = match(head, IDENTIFIER);
            if(!result) return false;

            i64 value = 0;
            if (obj->fields.size())
                value = obj->fields.last().value + 1;
            
            auto tok = &gettok(head);
            if(tok->kind == '=') {
                head++;
                result = parse_literal(head, &value);
                if(!result) return false;
            }
            
            obj->fields.add({});
            obj->fields.last().name = id.data;
            obj->fields.last().value = value;
            
            auto& toke = gettok(head);
            if(toke.kind == '}') {
                break;
            }
            result = match(head, ',');
            if(!result) return false;
        }
        return true;
    }
    bool ParserContext::parse_function_parameters(int& head, CFunction* obj) {
        using namespace engone;
        bool result = false;
        auto& tok0 = gettok(head);
        auto& tok1 = gettok(head+1);

        if(tok0.data == "void" && tok1.kind == ')') {
            // int myliteral(void)
            // means no parameters
            head++;
            return true;
        }

        // TODO: Handle attribute
        int paren_depth = 0;
        while(true) {
            auto& token = gettok(head);
            if(token.kind == END_OF_FILE)
                return false;
            if (token.kind == ')') {
                break;
            }

            CType* type;
            result = parse_type(head, &type);
            if(!result) return false;

            auto& id = gettok(head);
            if(id.kind == IDENTIFIER) {
                head++;
                obj->parameters.add({});
                obj->parameters.last().name = id.data;
            } else {
                obj->parameters.add({});
                obj->parameters.last().name = "arg"+std::to_string(obj->parameters.size()-1);
            }
            obj->parameters.last().type = type;

            auto tok = &gettok(head);
            if(tok->kind == '[') {
                head++;

                tok = &gettok(head);
                if(tok->kind == NUMBER) {
                    head++;
                    type->name += "[" + tok->data + "]";
                } else {
                    // fun(int array[]) becomes fun(array: int*)
                    type->name += "*";
                }

                result = match(head, ']');
                if(!result) return false;
            }

            auto& tok2 = gettok(head);
            if(tok2.kind == ',') {
                head++;
            } else if(tok2.kind == ')') {

            } else {
                return false;
            }
        }
        return true;
    }
    bool ParserContext::parse_type(int& head, CType** out_type) {
        using namespace engone;
        std::string typestring = "";
        int typesize = 0;
        int typealign = 0;
        bool result;
        if (gettok(head).kind == CONST)
            head++;

        auto find_and_set_sizes = [this,&typesize,&typealign](const std::string name) {
            auto pair = identifiers.find(name);
            if (pair == identifiers.end()) {
                log::out << "Cannot find " << name << "\n";
                typesize = 0;
                typealign = 0;
            } else {
                switch(pair->second->kind) {
                    case KIND_ENUM: {
                        typesize = 4;
                        typealign = 4;
                        break;
                    } case KIND_TYPE: {
                        auto t = (CType*)pair->second;
                        // log::out << "A " << t->name << " " << t->size << " " << t->alignment << "\n";
                        typesize = ((CType*)pair->second)->size;
                        typealign = ((CType*)pair->second)->alignment;
                        break;
                    } case KIND_STRUCT: {
                        typesize = ((CStruct*)pair->second)->size;
                        typealign = ((CStruct*)pair->second)->alignment;
                        break;
                    } default: Assert(false);
                }
            }
        };

        auto token = &gettok(head);
        if(token->data == "void" || token->data == "bool" || token->data == "char") {
            // C doesn't have bool type, we handle it
            // anyway because you might want to parse C++ header
            // that is mostly C with some C++ elements (like bool)
            typestring = token->data;
            head++;
            typesize = 1;
            typealign = 1;
        } else if(token->data == "float") {
            typestring = "f32";
            head++;
            typesize = 4;
            typealign = 4;
        } else if(token->data == "double") {
            typestring = "f64";
            head++;
            typesize = 8;
            typealign = 8;
        } else if(token->kind == STRUCT) {
            head++;
            
            auto& tok = gettok(head);
            if(tok.kind == IDENTIFIER) {
                head++;
            }
            
            if (gettok(head).kind == '{') {
                CStruct* obj = create_struct();
                obj->packing = pack_stack.last();
                if(tok.kind == IDENTIFIER) {
                    obj->name = tok.data;
                    mark_strong(obj->name);
                }

                result = match(head, '{');
                if(!result) return false;

                parse_struct_fields(head, obj);

                result = match(head, '}');
                if(!result) return false;

                obj->calculate_size();

                CType* type = create_type();
                *out_type = type;
                type->name = typestring;
                type->obj = obj;
                type->size = obj->size;
                type->alignment = obj->alignment;
                // TODO: trailing pointers
                return true;
            } else if(tok.kind == IDENTIFIER) {
                typestring = tok.data;
                mark_weak(typestring);
                find_and_set_sizes(typestring);
            } else {
                return false;
            }
        } else if(token->kind == ENUM) {
            head++;
            
            auto& tok = gettok(head);
            if(tok.kind == IDENTIFIER) {
                head++;
            }
            
            if (gettok(head).kind == '{') {
                CEnum* obj = create_enum();
                if(tok.kind == IDENTIFIER) {
                    obj->name = tok.data;
                }

                result = match(head, '{');
                if(!result) return false;

                parse_enum_fields(head, obj);

                result = match(head, '}');
                if(!result) return false;

                CType* type = create_type();
                *out_type = type;
                type->name = typestring;
                type->obj_enum = obj;
                type->size = 4;
                type->alignment = 4;

                // TODO: trailing pointers
                return true;
            } else if(tok.kind == IDENTIFIER) {
                typestring = tok.data;
                typesize = 4;
                typealign = 4;
            } else {
                return false;
            }
        } else if(token->kind == UNION) {
            head++;
            
            auto& tok = gettok(head);
            if(tok.kind == IDENTIFIER) {
                head++;
            }
            
            if (gettok(head).kind == '{') {
                CStruct* obj = create_struct();
                obj->packing = pack_stack.last();
                if(tok.kind == IDENTIFIER) {
                    obj->name = tok.data;
                    mark_strong(obj->name);
                }

                result = match(head, '{');
                if(!result) return false;

                parse_struct_fields(head, obj);

                result = match(head, '}');
                if(!result) return false;

                obj->union_ify();
                obj->calculate_size();

                CType* type = create_type();
                *out_type = type;
                type->name = typestring;
                type->obj = obj;
                type->size = obj->size;
                type->alignment = obj->alignment;
                // TODO: trailing pointers
                return true;
            } else if(tok.kind == IDENTIFIER) {
                typestring = tok.data;
                mark_weak(typestring);
                find_and_set_sizes(typestring);
            } else {
                return false;
            }
        } else {
            /* This code handles
                [signed|unsigned] char
                [signed|unsigned] short
                [signed|unsigned] int
                [signed|unsigned] short int
                [signed|unsigned] long [long] [int]

                Noteworthy quirk: long in GCC in NixOS (Linux) is 8 bytes, on Windows it's 4 bytes.
            */
           // these can be in any order..
            int has_int = 0;
            int has_char = 0;
            int has_short = 0;
            int long_count = 0;
            int has_unsigned = 0;
            int has_signed = 0;
            int head_before = head;
            while(true) {
                auto tok = &gettok(head);
                head++;
                if(tok->data == "unsigned") {
                    if(has_unsigned) {
                        // duplicate specifier
                        return false;
                    }
                    has_unsigned = 1;
                } else if(tok->data == "signed") {
                    if(has_signed) {
                        // duplicate specifier
                        return false;
                    }
                    has_signed = 1;
                } else if(tok->data == "char") {
                    if(has_char) {
                        // duplicate specifier
                        return false;
                    }
                    has_char = 1;
                } else if(tok->data == "short") {
                    if(has_short) {
                        // duplicate specifier
                        return false;
                    }
                    has_short = 1;
                } else if(tok->data == "int") {
                    if(has_int) {
                        // duplicate specifier
                        return false;
                    }
                    has_int = 1;
                } else if(tok->data == "long") {
                    if(long_count >= 2) {
                        // too many longs
                        return false;
                    }
                    long_count++;
                } else {
                    head--;
                    break;
                }
            }

            if ((has_char + (has_short || has_int) > 1) || (has_char + has_short && long_count)) {
                // invalid combination
                return false;
            }
        
            if(has_char) {
                if(has_unsigned) {
                    typestring = "u8";
                } else if(has_signed) {
                    typestring = "i8";
                } else {
                    typestring = "char";
                }
                typesize = 1;
                typealign = 1;
            } else if(has_short) {
                if(has_unsigned) {
                    typestring = "u16";
                } else {
                    typestring = "i16";
                }
                typesize = 2;
                typealign = 2;
            } else if (has_signed || has_unsigned || has_int || long_count) {
                if(has_unsigned) {
                    typestring = "u";
                } else {
                    typestring = "i";
                }
                if(long_count == 2 || (long_count == 1 && compile_options->target != TARGET_WINDOWS_x64)) {
                    typestring += "64";
                    typesize = 8;
                    typealign = 8;
                } else {
                    typestring += "32";
                    typesize = 4;
                    typealign = 4;
                }
            } else if(gettok(head).kind == IDENTIFIER) {
                head++;
                // Some named type
                typestring = token->data;
                find_and_set_sizes(typestring);
            } else {
                // parsed nothing
                return false;
            }
        }

        if (gettok(head).kind == CONST)
            head++;

        CType* base_type = create_type();
        base_type->name = typestring;
        base_type->size = typesize;
        base_type->alignment = typealign;
        while(true) {
            auto& tok3 = gettok(head);
            if(tok3.kind != '*') {
                break;
            }
            head++;
            base_type->name += "*";

            if (gettok(head).kind == CONST)
                head++;
            
            base_type->size = REGISTER_SIZE;
            base_type->alignment = REGISTER_SIZE;
        }

        auto& tok0 = gettok(head);
        if(tok0.kind == '(') {
            head++;
            auto tok = &gettok(head);
            if(tok->kind == ATTRIBUTE) {
                if(tok->data != "__stdcall__" && tok->data != "__cdecl__")
                    return false;
                head++;
            }
            result = match(head, '*');
            if(!result) return false;

            // likely a function pointer, syntax error otherwise i think?
            CFunction* func = create_function();
            CType* func_type = create_type();
            func_type->size = REGISTER_SIZE;
            func_type->alignment = REGISTER_SIZE;
            func_type->obj_func = func;
            if(base_type->name != "void")
                func->return_type = base_type;
            
            auto& tok2 = gettok(head);
            if(tok2.kind == ')') {
                // no identifier
            } else if(tok2.kind == IDENTIFIER) {
                head++;
                func_type->name = tok2.data;
                func->name = tok2.data;
            } else {
                return false;
            }
            result = match(head, ')');
            if(!result) return false;
            
            result = match(head, '(');
            if(!result) return false;
            
            result = parse_function_parameters(head, func);
            if(!result) return false;

            result = match(head, ')');
            if(!result) return false;

            *out_type = func_type;
        } else {
            *out_type = base_type;
        }

        return true;
    };
    void CStruct::union_ify() {
        using namespace engone;
        is_union = true;
        int index_of_largest_field = -1;
        int size_of_largest_field = 0;
        for(int i=0;i<fields.size();i++) {
            if (index_of_largest_field == -1 || fields[i].type->size > size_of_largest_field) {
                index_of_largest_field = i;
                size_of_largest_field = fields[i].type->size;
            }
        }
        
        if(index_of_largest_field != 0)
            fields[0] = fields[index_of_largest_field];
        while(fields.size() >= 2) {
            fields.pop();
        }
    }
    void CStruct::calculate_size() {
        int final_alignment = 1;

        int offset = 0;
        for(int i=0;i<fields.size();i++) {
            auto& field = fields[i];
            
            int align = field.type->alignment;
            // We don't assert because some types may not been parsed.
            // Align will be 0 for such types and we don't want to prevent compilation.
            // The C header to BTB transpiler is about best effort, it converts declarations
            // it can and sometimes it can't to better.
            // Assert(align > 0);
            if(align > packing)
                align = packing;
            if(align > final_alignment)
                final_alignment = align;
            
            if(offset % align)
                offset += align - (offset % align); // add padding to get proper alignment
            offset += field.type->size;
        }
        // Assert(final_alignment >= 1);
        size = ((offset + final_alignment-1) / final_alignment) * final_alignment;
        alignment = final_alignment;
    }
    std::string ParserContext::type_to_string(CType* type) {
        using namespace engone;
        if(type->obj_func) {
            CFunction* func = type->obj_func;
            std::string out = "fn @oscall (";
            // log::out << func->name << "  " << func->parameters.size() << "\n";
            for(int i=0;i<func->parameters.size();i++) {
                auto& param = func->parameters[i];
                if(i!=0)
                    out += ", ";
                if(param.name.size()) {
                    out += param.name + ": " + type_to_string(param.type);
                } else {
                    out += type_to_string(param.type);
                }
            }
            out += ")";
            if(func->return_type) {
                out += " -> " + type_to_string(func->return_type);
            }
            return out;
        } else if(type->obj) {
            std::string name = "unnamed_" + std::to_string(unnamed_count++);
            unnamed_types.add({});
            unnamed_types.last().unique_name = name;
            unnamed_types.last().type = type;
            return name + type->name;
        } else {
            return type->name;
        }
    }
    void ParserContext::walk() {
        output += "// Auto-generated BTB declarations from " + origin_path + "\n\n";

        // define opaque struct types
        bool has_weak = false;
        for(auto& pair : weak_structs) {
            if(!pair.second.defined) {
                if(!has_weak) {
                    output += "// Opaque struct types\n";
                    has_weak = true;
                }
                output += "struct " + pair.first + " {}\n";
            }
        }
        if(has_weak)
            output += "\n";

        for(int ni=0;ni<root.nodes.size();ni++) {
            CNode* base = root.nodes[ni];
            switch(base->kind) {
                case KIND_TYPEDEF: {
                    auto* node = (CTypedef*)base;
                    std::string type_name;
                    if(node->type && node->type->obj) {
                        std::string base_name = "base_typedef" + std::to_string(ni);
                        int base_index = -1;
                        for(int i=0;i<node->typeNames.size();i++) {
                            if(node->typeNames[i].ptr_level == 0) {
                                base_index = i;
                                base_name = node->typeNames[i].name;
                                break;
                            }
                        }
                        CStruct* struc = node->type->obj;
                        output += "struct ";
                        if(struc->packing != 8) {
                            output += "@no_padding ";
                        }
                        output += base_name + " {\n";
                        for(int fi=0;fi<struc->fields.size();fi++) {
                            auto& field = struc->fields[fi];
                            output += "    " + field.name + ": " + type_to_string(field.type) + ";\n";
                        }
                        output += "}\n";
                        for(int i=0;i<node->typeNames.size();i++) {
                            if(i == base_index)
                                continue;
                            if(node->typeNames[i].name == base_name)
                                continue;
                            output += "#macro " + node->typeNames[i].name + " " + base_name;
                            for (int j=0;j<node->typeNames[i].ptr_level;j++) {
                                output += "*";
                            }
                            output += "\n";
                        }
                    } else if(node->type && node->type->obj_enum) {
                        CEnum* enu = node->type->obj_enum;
                        output += "enum ";
                        type_name = node->typeNames[0].name;
                        output += type_name + " {\n";
                        for(int fi=0;fi<enu->fields.size();fi++) {
                            auto& field = enu->fields[fi];
                            output += "    " + field.name + " = " + std::to_string(field.value);
                            if(fi != enu->fields.size()-1)
                                output += ", \n";
                            else
                                output += "\n";
                        }
                        output += "}\n";
                    } else {
                        auto pair = weak_structs.find(node->typeNames[0].name);
                        if(pair == weak_structs.end() || pair->second.defined) {
                            type_name = type_to_string(node->type);
                            output += "#macro " + node->typeNames[0].name + " " + type_name + "\n";
                        }
                    }
                }
                break; case KIND_STRUCT: {
                    auto node = (CStruct*)base;
                    if(node->is_union)
                        output += "// should be a C union but BTB doesn't support them\n";
                    output += "struct ";
                    if(node->packing != 8) {
                        output += "@no_padding ";
                    }
                    output += node->name + " {\n";
                    for(int fi=0;fi<node->fields.size();fi++) {
                        auto& field = node->fields[fi];
                        output += "    " + field.name + ": " + type_to_string(field.type) + ";\n";
                    }
                    output += "}\n";
                }
                break; case KIND_ENUM: {
                    CEnum* enu = (CEnum*)base;
                    output += "enum " + enu->name + " {\n";
                    for(int fi=0;fi<enu->fields.size();fi++) {
                        auto& field = enu->fields[fi];
                        output += "    " + field.name + " = " + std::to_string(field.value);
                        if(fi != enu->fields.size()-1)
                            output += ", \n";
                        else
                            output += "\n";
                    }
                    output += "}\n";
                }
                break; case KIND_FUNCTION: {
                    auto node = (CFunction*)base;

                    auto pair = function_map.find(node->name);
                    if (pair != function_map.end()) {
                        // function already exists, skip we don't want to add duplicates.
                        // In C headers it is valid semantics to declare the same function multiple times.
                        // Not the case in BTB so we must deduplicate.
                        
                        // To reduce computation we assume the function types match.
                        // We may decide to check this in the future anyway: node->type == pair->second->type
                        break;
                    }
                        
                    function_map[node->name] = node;
                    output += "fn @import(__c_import__) " + node->name + "(";
                    for(int fi=0;fi<node->parameters.size();fi++) {
                        auto& parameter = node->parameters[fi];
                        if(fi != 0)
                            output += ", ";
                        output += parameter.name + ": " + type_to_string(parameter.type);
                    }
                    output += ")";
                    if(node->return_type)
                        output += " -> " + type_to_string(node->return_type);
                    output += ";\n";
                }
                break; case KIND_VARIABLE: {
                    auto node = (CVariable*)base;
                    output += "global @import(__c_import__) " + node->name + ": " + type_to_string(node->type) + ";\n";
                }
                break; default: Assert(false);
            }
            while(unnamed_types.size()) {
                auto obj = unnamed_types.last();
                unnamed_types.pop();
                
                if(obj.type->obj) {
                    auto node = (CStruct*)obj.type->obj;
                    output += "struct ";
                    if(node->packing != 8) {
                        output += "@no_padding ";
                    }
                    output += obj.unique_name + " {\n";
                    for(int fi=0;fi<node->fields.size();fi++) {
                        auto& field = node->fields[fi];
                        output += "    " + field.name + ": " + type_to_string(field.type) + ";\n";
                    }
                    output += "}\n";
                } else if(obj.type->obj_enum) {
                    Assert(false);
                }
            }
        }
        output += "\n";
        output += "// Macros\n";
        for(const auto& pair : preproc.macros) {
            const auto& macro = pair.second;
            // if(pair.first == "GL_DEBUG_CALLBACK_USER_PARAM") {
            //     int x=23;
            // }
            output += "#macro " + pair.first;
            if(macro.has_params) {
                output += "(";
                for(int i=0;i<macro.parameters.size();i++) {
                    const auto& param = macro.parameters[i];
                    if(i!=0)
                        output += ", ";
                    output += param;
                }
                output += ")";
            }
            output += " ";
            bool is_empty = true;
            for(int i=0;i<macro.content.size();i++) {
                if(!isspace(macro.content[i])) {
                    is_empty = false;
                    break;
                }
            }
            if(is_empty)
                output += "#endmacro";
            else {
                // Handles backslash
                bool has_backslash = false;
                int head = 0;
                while(head < macro.content.size()) {
                    int at = macro.content.substr(head).find("\\");
                    if(at == -1) {
                        output += macro.content.substr(head);
                        break;
                    } else {
                        if(head == 0)
                            output += "\n";
                        has_backslash = true;
                        at += head;
                        output += macro.content.substr(head, at - head);
                        head = at + 1;
                        // output += "\n"; // content should have newline after backslash
                    }
                }
                if(has_backslash) {
                    output += " #endmacro";
                }
            }
            output += "\n";
        }
    }

}
