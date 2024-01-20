
#pragma once

#include "BetBat/Lexer.h"

#include "BetBat/Tokenizer.h"
#include "BetBat/PhaseContext.h"
#include "Engone/Util/BucketArray.h"

#include "BetBat/Util/Perf.h"

struct Compiler;

namespace preproc {
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

    struct {
        Reporter* reporter = nullptr;
        bool ignoreErrors = false;
        int errors=0;
    } info; // struct because error macros expects info.reporter and some other fields
    
    bool evaluateTokens=false;
    u32 new_import_id=0;
    lexer::Import* new_lexer_import = nullptr;
    u32 import_id=0;
    Preprocessor::Import* current_import = nullptr;
    u32 head=0;
    
    bool quick_iterator = false;
    lexer::Import* lexer_import=nullptr;
    DynamicArray<lexer::Chunk*> lexer_chunks;
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
            lexer::Chunk* chunk = lexer_chunks[fcindex];

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
    lexer::TokenInfo* getinfo(StringView* string = nullptr, int off = 0) {
        static lexer::TokenInfo eof{lexer::TOKEN_EOF};
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
            lexer::Chunk* chunk = lexer_chunks[fcindex];

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
    void advance(int n = 1) {
        head += n;
    }
    // returns token index into import of the next token to read
    // The token gettok will read.
    u32 gethead() { return head; }
    
    // void parseAll();
    SignalIO parseOne();
    
    // NOTE: We assume that the hashtag and directive identifiers were parsed.
    //   Next is the content
    SignalIO parseMacroDefinition();
    SignalIO parseMacroEvaluation();
    SignalIO parseLink();
    SignalIO parseImport();
    SignalIO parseIf();

    // incomplete
    SignalIO parseUndef();
    SignalIO parseInclude();
    SignalIO parsePredefinedMacros();
};
}