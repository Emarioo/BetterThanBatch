/*
    Transpiler from C headers to BTB

    The transpiler converts what it can and skips functions and types it
    can't handle. This means that C standard headers on one from one compiler (GCC, Clang, MSVC, MinGW)
    may correctly parse everything while headers from a different compiler cannot be parsed at all.

    This is why the C header transpiler feature in BTB Compiler is EXPERIMENTAL. It is not
    recommended for developing stable software.

    - Emarioo, 2025-07-24
*/
/*
    Features:
        Parse macros
        Parse comments
        Parse function declarations (including attribute, declspec)
        Parse structures
        Parse enums
        Parse typedefs
        Parse external global variables

    Limitations:
        Varadic arguments (macros and functions)
        Function pointers inside function pointers (simple function pointers work fine such as those from glad.h)
        Constants (like this: const int MAX_SIZE = 24;)
        Ignore function bodies
        Statements
        Non-external global variables
    
    Output:
        A btb file with import declarations
        Macros, enums and everything appears in the same order.

*/

#pragma once

#include <string>
#include "Engone/Util/Array.h"
#include "Engone/Util/BucketArray.h"
#include "basin/CompilerOptions.h"

//##############################
//     PUBLIC FUNCTIONS 
//###############################

struct TranspileOptions;
std::string TranspileCToBTB(const std::string& in_text, TranspileOptions* options, const std::string& path, CompileOptions* compile_options);
std::string TranspileCFileToBTB(const std::string& filepath, TranspileOptions* options, CompileOptions* compile_options);

//################################
//      INTERNAL FUNCTIONS
//################################

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

    int readBytes;
    int lines;
    int comment_lines;
    int blank_lines;
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
        ATTRIBUTE,
        EXTERN,
        // restrict and volatile are skipped
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
        KIND_ENUM,
        KIND_FUNCTION,
        KIND_VARIABLE,
        KIND_TYPE,
    };
    struct CStruct;
    struct CEnum;
    struct CFunction;
    struct CNode {
        CNode(CNodeKind k) : kind(k) {}
        int nodeid;
        CNodeKind kind;
    };
    struct CType : CNode {
        CType() : CNode(KIND_TYPE) {}
        std::string name;
        CStruct* obj;
        CEnum* obj_enum;
        CFunction* obj_func;
        int size;
        int alignment;
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
        int packing = 8; // packing from #pragma pack(push, X)
        int size = 0;
        int alignment = 0; // alignment of the field with highest alignment
        bool is_union = false;

        void union_ify();
        void calculate_size();
    };
    struct CEnum : CNode {
        CEnum() : CNode(KIND_ENUM) {}
        std::string name;
        struct Field {
            std::string name;
            int value;
        };
        DynamicArray<Field> fields;
    };
    struct CVariable : CNode {
        CVariable() : CNode(KIND_VARIABLE) {}
        std::string name;
        CType* type;
    };
    struct CTypedef : CNode {
        CTypedef() : CNode(KIND_TYPEDEF) {}
        CType* type;
        bool weak_void_type;
        
        struct TypeName {
            std::string name;
            int ptr_level;
        };
        DynamicArray<TypeName> typeNames;
    };
    struct CRoot {
        DynamicArray<CNode*> nodes;
    };

    struct ParserContext {
        CPreprocContext preproc;
        LexerContext lexer;

        std::string origin_path;
        CompileOptions* compile_options;

        CRoot root{};
        BucketArray<CFunction> functions{500};
        BucketArray<CStruct> structs{500};
        BucketArray<CEnum> enums{100};
        BucketArray<CType> types{500};
        BucketArray<CVariable> variables{500};
        BucketArray<CTypedef> typedefs{500};

        struct UnnamedType {
            std::string unique_name;
            CType* type;
        };
        DynamicArray<UnnamedType> unnamed_types; // items are popped when written
        int unnamed_count = 0;

        struct WeakStruct {
            // std::string name;
            bool defined;
        };
        std::unordered_map<std::string, WeakStruct> weak_structs;

        std::unordered_map<std::string, CNode*> identifiers;

        std::unordered_map<std::string, CNode*> function_map;

        void set_identifier(const std::string& name, CNode* node) {
            // engone::log::out << "SET ID " << name << "\n";
            identifiers[name] = node;
        }
        
        void mark_weak(const std::string& name) {
            auto pair = weak_structs.find(name);
            if(pair == weak_structs.end()) {
                weak_structs[name] = {};
            }
        }
        void mark_strong(const std::string& name) {
            auto pair = weak_structs.find(name);
            if(pair == weak_structs.end()) {
                auto& o = weak_structs[name] = {};
                o.defined = true;
            } else {
                pair->second.defined = true;
            }
        }
        int REGISTER_SIZE = 0;

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
        CEnum* create_enum() {
            CEnum* node;
            enums.add(nullptr, &node);
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
            auto& tok = gettok(head);
            if(tok.kind == kind) {
                head++;
                return true;
            } else {
                return false;
            }
        }

        CRoot* parse_top();
        bool parse_type(int& head, CType** type);
        bool parse_struct_fields(int& head, CStruct* obj);
        bool parse_enum_fields(int& head, CEnum* obj);
        bool parse_function_parameters(int& head, CFunction* obj);
        bool parse_literal(int& head, i64* number);
        void skip_construct(int& head);

        void walk();
        std::string type_to_string(CType* type);
    };
}

std::string PreprocessText(CPreprocContext* context, const std::string& text, const std::string& origin_path);