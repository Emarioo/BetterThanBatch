/*
    C header parser for transpiling C to BTB

    Features:
        Parse macros
        Parse comments
        Parse function declarations (including attribute, declspec)
        Parse structures
        Parse enums
        Parse typedefs
        Parse external global variables

    Limitations:
        Ignore function bodies
        Statements
        Non-external global variables
        Undef is ignored, it does not exist in BTB
    
    Output:
        A btb file with import declarations
        Macros, enums and everything appears in the same order.

*/

#pragma once

#include <string>
#include "Engone/Util/Array.h"

namespace clexer {
    struct Token {
        std::string data;
        int line;
        int column;
        bool has_newline;
    };
    
    struct Macro {
        DynamicArray<std::string> args;
        DynamicArray<Token> content;
    };
    struct Variable {
        std::string name;
        std::string type;
    };
    struct Function {
        std::string name;
        struct Arg {
            std::string name;
            std::string type;
        };
        DynamicArray<Arg> args;
        std::string return_type;
    };
    struct Structure {
        std::string name;
        struct Field {
            // Bit fields not allowed
            // unions not allowed
            std::string name;
            std::string type;
        };
        DynamicArray<Field> fields;
    };

    struct LexerContext {
        DynamicArray<Token> tokens;
        int head = 0;
        int cur_line = 1;
        int cur_column = 1;
        std::string out = "";

        std::unordered_map<std::string, Macro> macros;
        std::unordered_map<std::string, std::string> typedefs;
        DynamicArray<Variable> variables;
        DynamicArray<Function> functions;
        DynamicArray<Structure> structures;

        void write(const Token& token) {
            write(token.data, token.line, token.column, token.has_newline);
        }
        void write(const std::string& str, const Token& token) {
            write(str, token.line, token.column, token.has_newline);
        }
        void writeln() {
            out += "\n";
            cur_line++;
            cur_column=1;
        }
        void write(const std::string& str, int line, int column, bool has_newline) {
            while(cur_line < line) {
                out += "\n";
                cur_line++;
                cur_column=1;
            }
            if (cur_column == 1) {
                while(cur_column < column-1) {
                    out += " ";
                    cur_column++;
                }
            } else {
                if(cur_column < column) {
                    out += " ";
                    cur_column++;
                }
            }
            out += str;
            cur_column += str.size();
            if(has_newline) {
                out += "\n";
                cur_line++;
                cur_column=1;
            }
        }
        Token toknull = {""};
        void parse_top();
        std::string getstr(int index) {
            if(index >= tokens.size()) {
                return "";
            }
            return tokens[index].data;
        }
        Token& gettok(int index) {
            if(index >= tokens.size()) {
                return toknull;
            }
            return tokens[index];
        }
        bool is_tok_alnum(int index) {
            if(index >= tokens.size()) {
                return "";
            }
            char chr = tokens[index].data[0];
            return (((chr|32) >= 'a' && (chr|32) <= 'z') || (chr >= '0' && chr <= '9') || chr == '_');
        }
        void parse_struct_fields(int& index, Structure& structure);
        std::string parse_base_type(int& index);
        void skip_paren(int& index, int depth = 1);
    };
}

std::string TranspileCToBTB(const std::string& in_text);

std::string TranspileCFileToBTB(const std::string& filepath);