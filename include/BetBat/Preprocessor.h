
#pragma once

#include "BetBat/Lexer.h"

// #include "BetBat/Tokenizer.h"
#include "BetBat/PhaseContext.h"
#include "Engone/Util/BucketArray.h"

#include "BetBat/Util/Perf.h"
#include "BetBat/Signals.h"

struct Compiler;

namespace preproc {
struct MacroSpecific {
    lexer::SourceLocation location;
    lexer::TokenRange content;
    
    DynamicArray<lexer::Token> parameters;
    void addParam(lexer::Token name, bool variadic);
    int indexOfVariadic=-1; // tells you which argument is the infinite one, -1 for none
    // returns index to argumentNames
    bool isVariadic() { return indexOfVariadic != -1; }
    u32 nonVariadicArguments() { return isVariadic() ? parameters.size() - 1 : parameters.size(); }
    // -1 if not found
    int matchArg(lexer::Token token, lexer::Lexer* lexer);

    bool canMatchArg() {
        return parameters.size();
    }
};
struct MacroRoot {
    ~MacroRoot() {
        for(auto& pair : specificMacros) {
            pair.second->~MacroSpecific();
            TRACK_FREE(pair.second, MacroSpecific);
        }
    }
    // lexer::Token origin; // soure location
    std::string name;
    
    std::unordered_map<int,MacroSpecific*> specificMacros;
    bool hasVariadic=false;
    bool hasBlank=false;
    MacroSpecific variadicMacro;
};
struct PreprocContext;
struct Preprocessor {
    void cleanup() {
        imports.cleanup();
    }

    void init(lexer::Lexer* lexer, Compiler* compiler) { this->lexer = lexer; this->compiler = compiler; }
    
    // returns preprocessed import_id (unsigned 32-bit integer where 0 means failure)
    u32 process(u32 import_id, bool phase_2);
    
    MacroRoot* create_or_get_macro(u32 import_id, lexer::Token name, bool ensure_blank);
    void insertCertainMacro(u32 import_id, MacroRoot* rootMacro, MacroSpecific* localMacro);
    bool removeCertainMacro(u32 import_id, MacroRoot* rootMacro, int argumentAmount, bool variadic);
    MacroRoot* matchMacro(u32 import_id, const std::string& name, PreprocContext* context = nullptr);
    MacroSpecific* matchArgCount(MacroRoot* root, int count);
private:
    lexer::Lexer* lexer=nullptr;
    Compiler* compiler=nullptr;
    
    struct Import {
        ~Import() {
            for(auto& pair : rootMacros) {
                pair.second->~MacroRoot();
                TRACK_FREE(pair.second, MacroRoot);
            }
            // engone::log::out << "yes\n";160
        }
        bool processed_directives = false;
        bool evaluated_macros = false;
        // DynamicArray<u32> import_dependencies; //import_ids
        std::unordered_map<std::string, MacroRoot*> rootMacros;
    };
    
    BucketArray<Import> imports{256};
    MUTEX(lock_imports);
    
    friend struct PreprocContext;
};
typedef DynamicArray<lexer::Token> TokenList;
struct PreprocContext : PhaseContext {
    Preprocessor* preprocessor=nullptr;
    lexer::Lexer* lexer=nullptr;
    Reporter* reporter=nullptr;
    
    PreprocContext() : info(*this) { } // well this is dumb
    PreprocContext& info;

    bool ignoreErrors = false; // used with @no-code (unused in preprocessor but MessageTool needs it in generator and therefore also here, not elegant)
    bool showErrors = true;

    bool inside_conditional = false;
    
    bool evaluateTokens=false;
    u32 new_import_id=0;
    lexer::Import* new_lexer_import = nullptr;
    u32 import_id=0;
    lexer::Import* old_lexer_import = nullptr;
    Preprocessor::Import* current_import = nullptr;
    u32 head=0;

    struct CachedMacro {
        MacroRoot* root;
    };
    std::unordered_map<std::string, CachedMacro> cached_macro_names;

    DynamicArray<u32> ids_to_check{};
    bool has_computed_deps = false;

    struct Layer {
        Layer(bool eval_content) : eval_content(eval_content) {
            // Assert(eval_content);
            specific = nullptr;
            root = nullptr;
            // if(eval_content) {
                // new(&input_arguments)DynamicArray<TokenList>();
            // } else {
                import_id = 0;
                top_caller = nullptr;
                paren_depth = 0;
            // }
        }
        ~Layer() {
            // if(eval_content) {
                for(auto& l : input_arguments) {
                    l.cleanup();
                }
                input_arguments.cleanup();
            // } else {
                
            // }
        }
        u32 _head = 0;
        u32 start_head = 0;
        u32* ref_head = nullptr;
        u16 ending_suffix = 0;
        bool eval_content = true;
        bool unwrapped = false;
        bool concat_next_token = false;
        bool quote_next_token = false;
        MacroSpecific* specific;
        MacroRoot* root;
        // union {
        //     struct {
                DynamicArray<TokenList> input_arguments{};
                
            // };
            // struct {
                u32 import_id = 0;
                Layer* top_caller=nullptr; // the parent macro that called this macro (used by argument parsing or body parsing)
                Layer* adjacent_callee=nullptr; // when this layer is parsing arguments, callee refers to the layer (macro) that uses the arguments
                int paren_depth=0;
            // };
        // };
        void add_input_arg(engone::Allocator* allocator) {
            input_arguments.add({});
            input_arguments.last().init(allocator);
        }
        lexer::Token get(lexer::Lexer* lexer, int off = 0, StringView* string = nullptr) {
            if(import_id!=0 && !eval_content) {
                u32 index = *ref_head + off;
                return lexer->getTokenFromImport(import_id, *ref_head + off, string);
            } else {
                auto spec = specific;
                if(top_caller)
                    spec = top_caller->specific; // for arguments
                Assert(specific);
                u32 index = spec->content.token_index_start + *ref_head + off;
                if(index >= spec->content.token_index_end)
                    return {lexer::TOKEN_EOF};
                return lexer->getTokenFromImport(spec->content.importId, index, string);
            }
        }
        bool is_last(lexer::Lexer* lexer, int off = 0) {
            return get(lexer,off).type == lexer::TOKEN_EOF;
        }
        void step(int n = 1) {
            *ref_head+=n;
        }
        void sethead(u32 head) {
            _head = head;
            start_head = head;
            ref_head = &_head;
        }
        void sethead(u32* phead) {
            this->ref_head = phead;
            this->start_head = *phead;
        }
        u32 gethead() {
            return *ref_head;
        }
    };
    // DynamicArray<Layer*> cached_layers{}; // caching memory

    engone::LinearAllocator scratch_allocator{}; // TODO: Preserve allocator between threads, each thread can allocate memory for preprocessor once.

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
                // out.type = lexer::TOKEN_EOF;
                return lexer_import->geteof();
            }
            lexer::Chunk* chunk = lexer_chunks[fcindex];

            auto info = chunk->tokens.getPtr(tindex);
            if(!info) {
                // out.type = lexer::TOKEN_EOF;
                return lexer_import->geteof();
            }

            #ifdef LEXER_DEBUG_DETAILS
            if(info->flags & lexer::TOKEN_FLAG_HAS_DATA) {
                out.s = chunk->get_data(info->data_offset);
            }
            #endif
    
            out.flags = info->flags;
            out.type = info->type;
            out.origin = lexer->encode_origin(chunk->chunk_index,tindex);
            return out;
        }
        return lexer->getTokenFromImport(import_id, head + off);
    }
    lexer::TokenSource* getsource(int off = 0) {
        if(quick_iterator) {
            u32 fcindex,tindex;
            lexer->decode_import_token_index(head + off,&fcindex,&tindex);
        
            lexer::Token out{};
            if(lexer_chunks.size() <= fcindex) {
                // out.type = lexer::TOKEN_EOF;
                return nullptr;
            }
            lexer::Chunk* chunk = lexer_chunks[fcindex];

            auto info = chunk->sources.getPtr(tindex);
            if(!info) {
                // out.type = lexer::TOKEN_EOF;
                return nullptr;
            }
            return info;
        }
        lexer::TokenSource* src = nullptr;
        lexer->getTokenInfoFromImport(import_id, head + off, &src);
        return src;
    }
    lexer::Token gettok(StringView* string, int off = 0) {
        if(quick_iterator) {
            u32 fcindex,tindex;
            lexer->decode_import_token_index(head + off,&fcindex,&tindex);
        
            lexer::Token out{};
            if(lexer_chunks.size() <= fcindex) {
                if(string)
                    *string = {"",1};
                return lexer_import->geteof();
            }
            lexer::Chunk* chunk = lexer_chunks[fcindex];

            auto info = chunk->tokens.getPtr(tindex);
            if(!info) {
                if(string)
                    *string = {"",1};
                return lexer_import->geteof();
            }

            if(string && (info->flags & lexer::TOKEN_FLAG_HAS_DATA)) {
                *string = chunk->get_string(info->data_offset);
                #ifdef LEXER_DEBUG_DETAILS
                out.s = chunk->get_data(info->data_offset);
                #endif
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
        
        
            // lexer::Token out{};
            if(lexer_chunks.size() <= fcindex) {
                // out.type = lexer::TOKEN_EOF;
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
            if(string && (info->flags & lexer::TOKEN_FLAG_HAS_DATA)) {
                *string = chunk->get_string(info->data_offset);
                #ifdef LEXER_DEBUG_DETAILS
                info->s = chunk->get_data(info->data_offset);
                #endif
            }
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
    SignalIO parseMacroDefinition(bool is_global_macro = false);
    SignalIO parseMacroEvaluation();
    SignalIO parseLink();
    SignalIO parseLoad();
    SignalIO parseImport();
    SignalIO parseIf();
    SignalIO parseInformational(lexer::Token hashtag_tok, lexer::Token directive_tok, StringView directive_str, lexer::Token* out_tok, std::string* out_str);

    // incomplete
    SignalIO parseUndef();
    SignalIO parseInclude();
};
}