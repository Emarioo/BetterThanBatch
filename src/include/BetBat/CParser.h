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
#include "Engone/Util/BucketArray.h"
#include "BetBat/CompilerOptions.h"

struct CMacro {
    std::string content;
    DynamicArray<std::string> parameters;
    std::string origin_file;
    int pos_in_file;
    bool has_params;
};
struct TranspileOptions {
    DynamicArray<std::string> include_dirs;
    DynamicArray<std::string> c_defines;
};
struct IncludedFile {
    std::string path;
    bool pragma_once;
};
struct IfBlock {
    bool skip; // whether to skip current text in the current if,elif,else selective block
    bool in_else_block; // whether we have seen else, used to print error if we see a second one
    bool has_enabled_block; // whether an if, elif block was enabled, if so the other elif sections should be skipped
};
struct CPreprocContext {
    // const std::string& text;
    // std::string output;
    TranspileOptions* options;
    std::unordered_map<std::string, CMacro> macros;
    DynamicArray<IncludedFile> included_files;
    std::unordered_map<std::string, DynamicArray<CMacro>> stacked_macros;

    DynamicArray<IfBlock> if_blocks;
    int current_pos = 0;

    bool should_skip() const {
        return if_blocks.size() != 0 && if_blocks.last().skip;
    }
};

namespace clexer {
    

    enum TokenKind {
        END_OF_FILE = 256,
        IDENTIFIER,
        NUMBER,
        STRING,
        CHAR,
        TYPEDEF,
        STRUCT,
        ENUM,
        UNION,
        CONST,
        VOLATILE,
        ATTRIBUTE,
        EXTERN,
    };
    struct Token {
        TokenKind kind;
        
        int line;
        int column;
        
        std::string data;
    };
    
    struct Macro {
        DynamicArray<std::string> args;
        DynamicArray<Token> content;
    };


    struct LexerContext {
        DynamicArray<Token> tokens;
        int head = 0;
        int cur_line = 1;
        int cur_column = 1;
        std::string out = "";

        // std::unordered_map<std::string, Macro> macros;
        // std::unordered_map<std::string, std::string> typedefs;
        // DynamicArray<Variable> variables;
        // DynamicArray<Function> functions;
        // DynamicArray<Structure> structures;

       
        
        // void parse_struct_fields(int& index, Structure& structure);
        // std::string parse_base_type(int& index);
        // void skip_paren(int& index, int depth = 1);

        void lex_tokens(const std::string& text);
    };
    enum CNodeKind {
        KIND_TYPEDEF,
        KIND_STRUCT,
        KIND_FUNCTION,
        KIND_VARIABLE,
        KIND_TYPE,
    };
    struct CStruct;
    struct CNode {
        CNode(CNodeKind k) : kind(k) {}
        int nodeid;
        CNodeKind kind;
    };
    struct CType : CNode {
        CType() : CNode(KIND_TYPE) {}
        std::string name;
        CStruct* obj;
    };
    struct CFunction : CNode {
        CFunction() : CNode(KIND_FUNCTION) {}
        std::string name;
        struct Parameter {
            std::string name;
            CType* type;
        };
        DynamicArray<Parameter> parameters;
        CType* return_type;
    };
    struct CStruct : CNode {
        CStruct() : CNode(KIND_STRUCT) {}
        std::string name;
        struct Field {
            // Bit fields not allowed
            // unions not allowed
            std::string name;
            CType* type;
        };
        DynamicArray<Field> fields;
        int packing = 8;
    };
    struct CVariable : CNode {
        CVariable() : CNode(KIND_VARIABLE) {}
        std::string name;
        CType* type;
    };
    struct CTypedef : CNode {
        CTypedef() : CNode(KIND_TYPEDEF) {}
        std::string name;
        CType* type;
        bool weak_void_type;
    };
    struct CRoot {
        DynamicArray<CNode*> nodes;
    };

    struct ParserContext {
        CPreprocContext preproc;
        LexerContext lexer;

        CRoot root{};
        BucketArray<CFunction> functions{100};
        BucketArray<CStruct> structs{100};
        BucketArray<CType> types{100};
        BucketArray<CVariable> variables{100};
        BucketArray<CTypedef> typedefs{100};

        std::string output;

        DynamicArray<int> pack_stack;
        int next_nodeid=0;

        CType* create_type() {
            CType* node;
            types.add(nullptr, &node);
            node->nodeid = next_nodeid++;
            return node;
        }
        CTypedef* create_typedef() {
            CTypedef* node;
            typedefs.add(nullptr, &node);
            node->nodeid = next_nodeid++;
            return node;
        }
        CFunction* create_function() {
            CFunction* node;
            functions.add(nullptr, &node);
            node->nodeid = next_nodeid++;
            return node;
        }
        CStruct* create_struct() {
            CStruct* node;
            structs.add(nullptr, &node);
            node->nodeid = next_nodeid++;
            return node;
        }
        CVariable* create_variable() {
            CVariable* node;
            variables.add(nullptr, &node);
            node->nodeid = next_nodeid++;
            return node;
        }

        Token TOKEN_EOF = {END_OF_FILE};
        
        std::string getstr(int index) {
            if(index >= lexer.tokens.size()) {
                return "";
            }
            return lexer.tokens[index].data;
        }
        Token& gettok(int index) {
            if(index >= lexer.tokens.size()) {
                return TOKEN_EOF;
            }
            return lexer.tokens[index];
        }
        bool is_tok_alnum(int index) {
            if(index >= lexer.tokens.size()) {
                return false;
            }
            char chr = lexer.tokens[index].data[0];
            return (((chr|32) >= 'a' && (chr|32) <= 'z') || (chr >= '0' && chr <= '9') || chr == '_');
        }

        bool match(int& head, int kind) {
            using namespace engone;
            if(gettok(head).kind == kind) {
                head++;
                return true;
            } else {
                return false;
            }
        }

        CRoot* parse_top();
        bool parse_type(int& head, CType** type);
        bool parse_struct_fields(int& head, CStruct* obj);
        bool parse_function_parameters(int& head, CFunction* obj);
        void skip_construct(int& head);

        void walk();
        std::string type_to_string(CType* type);

        // void write(const Token& token) {
        //     write(token.data, token.line, token.column, token.has_newline);
        // }
        // void write(const std::string& str, const Token& token) {
        //     write(str, token.line, token.column, token.has_newline);
        // }
        // void writeln() {
        //     out += "\n";
        //     cur_line++;
        //     cur_column=1;
        // }
        // void write(const std::string& str, int line, int column, bool has_newline) {
        //     while(cur_line < line) {
        //         out += "\n";
        //         cur_line++;
        //         cur_column=1;
        //     }
        //     if (cur_column != 1) {
        //         out += " ";
        //     //     while(cur_column < column-1) {
        //     //         out += " ";
        //     //         cur_column++;
        //     //     }
        //     // } else {
        //     }
        //     // while(cur_column < column) {
        //     //     out += " ";
        //     //     cur_column++;
        //     // }
        //     out += str;
        //     cur_column += str.size();
        //     if(has_newline) {
        //         out += "\n";
        //         cur_line++;
        //         cur_column=1;
        //     }
        // }
    };
}

std::string TranspileCToBTB(const std::string& in_text, TranspileOptions* options, const std::string& path, CompileOptions* compile_options);

std::string TranspileCFileToBTB(const std::string& filepath, TranspileOptions* options, CompileOptions* compile_options);

std::string PreprocessText(CPreprocContext* context, const std::string& text, const std::string& origin_path);