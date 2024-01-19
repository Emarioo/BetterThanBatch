
#pragma once

#include "BetBat/Lexer.h"

#include "BetBat/Tokenizer.h"
#include "BetBat/PhaseContext.h"
#include "Engone/Util/BucketArray.h"

#include "BetBat/Util/Perf.h"

struct TokenRef{
    uint16 index=0;
    uint16 flags=0;
    
    operator int() { return index; };
};
// Tokens in argument
typedef QuickArray<TokenRef> TokenList;
typedef DynamicArray<TokenList> Arguments;
struct TokenSpan {
    int start=0;
    int end=0;
    TokenStream* stream=nullptr;
};
engone::Logger& operator<<(engone::Logger& logger, const TokenSpan& span);
struct CertainMacro {
    ~CertainMacro(){
        cleanup();
    }
    void cleanup() {
        // sortedParameters.cleanup();
        parameters.cleanup();
    }
    Token name{};
    TokenSpan contentRange{};
    int start=-1;
    int end=-1;

    // struct Parameter {
    //     Token name{};
    //     int index=-1;
    // };

    // DynamicArray<Parameter> sortedParameters{};

    // I tested how fast an unordered_map would be and it was slower than a normal array.
    DynamicArray<Token> parameters;
    void addParam(const Token& name);
    int indexOfVariadic=-1; // tells you which argument is the infinite one, -1 for none
    // returns index to argumentNames
    bool isVariadic() { return indexOfVariadic != -1; }
    u32 nonVariadicArguments() { return isVariadic() ? parameters.size() - 1 : parameters.size(); }
    // bool isBlank() { return blank; }
    // bool blank = false;
    // -1 if not found
    int matchArg(const Token& token);
    // int matchArg(Token token);
};
struct RootMacro {
    Token name;
    // TODO: Allocator for macros
    std::unordered_map<int, CertainMacro*> certainMacros;
    bool hasVariadic = false;
    bool hasBlank = false;
    CertainMacro variadicMacro{};
    // CertainMacro* matchArgCount(int count, bool includeInf = true);
};
struct EvalInfo {
    // step 1
    RootMacro* rootMacro=0;
    Arguments arguments{};
    int argIndex=0;
    TokenList workingRange{};
    int workIndex=0;
    Token macroName{};

    int finalFlags=0;

    QuickArray<CertainMacro*> superMacros{};
    QuickArray<Arguments*> superArgs{};

    bool matchSuperArg(const Token& name, CertainMacro*& superMacro, Arguments*& superArgs, int& argIndex);

    CertainMacro* macro=0;

    TokenList output{};
};
struct Env {
    TokenSpan range{};
    int callIndex=0;
    int ownsCall=-1; // env will pop the call when popped

    int arg_start = 0;
    int arg_count = 0;

    bool firstTime=true;
    bool unwrapOutput=false;
    int outputToCall=-1; // index of call
    // int calculationArgIndex=-1; // used with unwrap

    int finalFlags = 0;
};
struct MacroCall {
    RootMacro* rootMacro = nullptr;
    CertainMacro* certainMacro = nullptr;
    DynamicArray<TokenSpan> inputArgumentRanges{}; // fetched arguments for the macro call, the input arguments to the macro call
    DynamicArray<int> realArgs_per_range{};
    int realArgumentCount = 0; // inputArguments may contain ... which doesn't tell you how many actual arguments are there
    bool unwrapped=false;
    bool useDetailedArgs=false; // caused by #unwrap
    DynamicArray<DynamicArray<const Token*>> detailedArguments{};
};
struct CompileInfo;
struct PreprocInfo : public PhaseContext {
    // TokenStream inTokens{};
    // TokenStream tokens{};
    TokenStream* inTokens = 0;
    TokenStream* outTokens = 0;
    TokenStream* tempStream = 0; // Used when parsing final contents of a macro.
    bool usingTempStream = false; // used to find recursion bugs


    u32 warnings = 0;
    bool ignoreErrors = false;
    // int ifdefDepth=0;
    // token range
    // start ... end-1
    // end index is not included
    // std::unordered_map<std::string,RootMacro> _macros; // moved to compiler info for global access
    
    Token parsedMacroName{}; // #line and #column use this when inside macros

    int macroRecursionDepth=0;

    // DynamicArray<CertainMacro*> superMacros{};
    // DynamicArray<Arguments*> superArgs{};
    // bool matchSuperArg(Token& name, CertainMacro*& superMacro, Arguments*& superArgs, int& argIndex);

    // DynamicArray<std::string> defineStack;
    
    // ptr may be invalidated if you add defines to the unordered map.
    // RootMacro* createRootMacro(const Token& name);
    // RootMacro* matchMacro(const Token& token);
    
    void addToken(Token inToken);
    
    int index=0; // using readHead from inTokens instead
    int at();
    bool end();
    Token& now();
    Token& next();
    void reverse();
    int length();
    Token& get(int index);
    // void nextline();

    // Used when parsing macros
    bool macroArraysInUse = false;
    DynamicArray<const Token*> outputTokens{};
    DynamicArray<i16> outputTokensFlags{};
    // outputTokens._reserve(15000);
    DynamicArray<MacroCall*> calls{};
    // calls._reserve(30);
    DynamicArray<Env*> environments{};
    // environments._reserve(50);
    DynamicArray<bool> unwrappedArgs{};
};
TokenStream* Preprocess(CompileInfo* compileInfo, TokenStream* tokens);
// void PreprocessImports(CompileInfo* compileInfo, TokenStream* tokens); deprecated


struct MacroSpecific {
    lexer::TokenRange content;
    
    DynamicArray<lexer::Token> parameters;
    void addParam(lexer::Token name, bool variadic);
    int indexOfVariadic=-1; // tells you which argument is the infinite one, -1 for none
    // returns index to argumentNames
    bool isVariadic() { return indexOfVariadic != -1; }
    u32 nonVariadicArguments() { return isVariadic() ? parameters.size() - 1 : parameters.size(); }
    // -1 if not found
    int matchArg(lexer::Token token, lexer::Lexer* lexer);
};
struct MacroRoot {
    // lexer::Token origin; // soure location
    std::string name;
    
    std::unordered_map<int,MacroSpecific*> specificMacros;
    bool hasVariadic=false;
    bool hasBlank=false;
    MacroSpecific variadicMacro;
};
struct Compiler;
struct Preprocessor {
    
    void init(lexer::Lexer* lexer, Compiler* compiler) { this->lexer = lexer; this->compiler = compiler; }
    
    // returns preprocessed import_id (unsigned 32-bit integer where 0 means failure)
    u32 process(u32 import_id, bool phase_2);
    
    MacroRoot* create_or_get_macro(u32 import_id, lexer::Token name, bool ensure_blank);
    void insertCertainMacro(u32 import_id, MacroRoot* rootMacro, MacroSpecific* localMacro);
    bool Preprocessor::removeCertainMacro(u32 import_id, MacroRoot* rootMacro, int argumentAmount, bool variadic);
    MacroRoot* matchMacro(u32 import_id, const std::string& name);
    MacroSpecific* matchArgCount(MacroRoot* root, int count);
private:
    lexer::Lexer* lexer=nullptr;
    Compiler* compiler=nullptr;
    
    struct Import {
        bool processed_directives = false;
        bool evaluated_macros = false;
        DynamicArray<u32> import_dependencies; //import_ids
        std::unordered_map<std::string, MacroRoot*> rootMacros;
    };
    
    BucketArray<Import> imports{256};
    engone::Mutex lock_imports;
    
    friend class PreprocContext;
};
struct PreprocContext {
    Preprocessor* preprocessor=nullptr;
    lexer::Lexer* lexer=nullptr;
    Compiler* compiler=nullptr;
    
    bool evaluateTokens=false;
    u32 new_import_id=0;
    lexer::Lexer::Import* new_lexer_import = nullptr;
    u32 import_id=0;
    Preprocessor::Import* current_import = nullptr;
    u32 head=0;
    
    bool quick_iterator = false;
    lexer::Lexer::Import* lexer_import=nullptr;
    DynamicArray<lexer::Lexer::Chunk*> lexer_chunks;
    void setup_token_iterator() {
        Assert(lexer);
        Assert(import_id != 0);
        quick_iterator = true;
        lexer_import = lexer->getImport_unsafe(import_id);
        FOR(lexer_import->chunk_indices) {
            lexer_chunks.add(lexer->getChunk_unsafe(it));
        }
    }
    
    // The token that will be returned is the one at index = head + off
    lexer::Token gettok(int off = 0) {
        if(quick_iterator) {
            u32 fcindex,tindex;
            lexer->decode_import_token_index(head + off,&fcindex,&tindex);
        
            lexer::Token out{};
            if(lexer_chunks.size() <= fcindex) {
                out.type = lexer::TOKEN_EOF;
                return out;
            }
            lexer::Lexer::Chunk* chunk = lexer_chunks[fcindex];

            auto info = chunk->tokens.getPtr(tindex);
            if(!info) {
                out.type = lexer::TOKEN_EOF;
                return out;
            }
    
            out.flags = info->flags;
            out.type = info->type;
            out.origin = lexer->encode_origin(chunk->chunk_index,tindex);
            return out;
        }
        return lexer->getTokenFromImport(import_id, head + off);
    }
    lexer::Lexer::TokenInfo* getinfo(StringView* string = nullptr, int off = 0) {
        static lexer::Lexer::TokenInfo eof{lexer::TOKEN_EOF};
        Assert(quick_iterator);
        if(quick_iterator) {
            u32 fcindex,tindex;
            lexer->decode_import_token_index(head + off,&fcindex,&tindex);
        
        
            lexer::Token out{};
            if(lexer_chunks.size() <= fcindex) {
                out.type = lexer::TOKEN_EOF;
                if(string)
                    *string = {"",1};
                return &eof;
            }
            lexer::Lexer::Chunk* chunk = lexer_chunks[fcindex];

            auto info = chunk->tokens.getPtr(tindex);
            if(!info) {
                if(string)
                    *string = {"",1};
                return &eof;
            }
            if(string)
                *string = {(char*)chunk->aux_data + info->data_offset + 1, chunk->aux_data[info->data_offset]};
            return info;
        }
        // return lexer->getTokenFromImport(import_id, head + off);
        return nullptr;
    }
    // StringView getstring(int off = 0) {
    //     Assert(quick_iterator);
        
    //     if(quick_iterator) {
    //         u32 fcindex,tindex;
    //         lexer->decode_import_token_index(head + off,&fcindex,&tindex);
        
    //         StringView out={"",0};
    //         if(lexer_chunks.size() <= fcindex) {
    //             return out;
    //         }
    //         lexer::Lexer::Chunk* chunk = lexer_chunks[fcindex];
    //         auto info = chunk->tokens.getPtr(tindex);
            
    //         if(!info)
    //             return out;
    //         return info;
    //     }
    //     // return lexer->getTokenFromImport(import_id, head + off);
    //     return nullptr;
    // }
    void advance(int n = 1) {
        head += n;
    }
    // returns token index into import of the next token to read
    // The token gettok will read.
    u32 gethead() { return head; }
    
    // void parseAll();
    bool parseOne();
    
    // NOTE: We assume that the hashtag and directive identifiers were parsed.
    //   Next is the content
    void parseMacroDefinition();
    bool parseMacroEvaluation();
    void parseLink();
    void parseImport();
    void parseIf();
    
    void parseUndef();
    void parseInclude();
    void parsePredefinedMacros();
};