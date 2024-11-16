#pragma once

#include "BetBat/AST.h"
#include "BetBat/PhaseContext.h"
#include "BetBat/Lexer.h"

struct CompileInfo;
namespace parser {
    
    
enum ParseFlags : u32 {
    PARSE_NO_FLAGS = 0x0,
    // INPUT
    PARSE_INSIDE_SWITCH     = 0x1,
    PARSE_TRULY_GLOBAL      = 0x2,
    PARSE_SKIP_ENTRY_BRACES = 0x10, // Treat first found brace in parseBody as a statement
                                    // instead of the start and end of the body
    PARSE_FROM_FUNC         = 0x20,                                    
                                    
    // OUTPUT
    PARSE_HAS_CURLIES = 0x4,
    PARSE_HAS_CASE_FALL = 0x8, // annotation @fall for switch cases
};
    
// TODO: Rename to ParseContext?
struct ParseContext : public PhaseContext {
    lexer::Lexer* lexer=nullptr;
    // Compiler* compiler=nullptr;
    AST* ast=nullptr;
    Reporter* reporter = nullptr;
    
    ParseContext() : info(*this) { } // well this is dumb
    ParseContext& info;

    // ParseContext() : tokens(tokens){}
    // int index=0; // index of the next token, index-1 is the current token
    // TokenStream* tokens;
    int funcDepth=0;

    QuickArray<ContentOrder> nextContentOrder;
    ContentOrder getNextOrder() { Assert(nextContentOrder.size()>0); return nextContentOrder.last(); }

    ScopeId currentScopeId=0;
    std::string currentNamespace = "";
    bool ignoreErrors = false;
    bool showErrors = true;

    bool is_inside_try_block = false; // defer is not allowed in try blocks since we may crash and then the defer won't be called. We may want to change this behaviour in the future but we do this just to be safe and prevent simple stupid mistakes.
    bool allow_inferred_initializers = false;
    
    bool allow_assignments = false; // THIS SHOULD BE FALSE BY DEFAULT in while, if, for... EVERYWHERE okay!! if you set it to true then remember to switch it back after the scope, see usages of allow_assignments to see what I mean.

    struct LoopScope {
        DynamicArray<ASTStatement*> defers; // apply these when continue, break or return is encountered
    };
    struct AnyScope {
        int defer_size;
    };
    // function or global scope
    struct FunctionScope {
        DynamicArray<ASTStatement*> defers; // apply these when return is encountered
        DynamicArray<LoopScope> loopScopes;
        DynamicArray<AnyScope> anyScopes;
        ASTFunction* function;
    };
    DynamicArray<FunctionScope> functionScopes;

    // Does not handle out of bounds
    // Token &prev();
    // Token& next();
    // Token& now();
    // bool revert();
    // Token& get(uint index);
    // int at();
    // bool end();

    struct TokenIteratorState {
        u32 import_id=0;
        u32 head=0;
        lexer::Token prev_tok{};
        u32 prev_index;
        lexer::Import* lexer_import=nullptr;
        DynamicArray<lexer::Chunk*> lexer_chunks;
        DynamicArray<QuickArray<lexer::TokenInfo>*> lexer_tokens;    
    };
    u32 import_id=0;
    u32 head=0;
    lexer::Token prev_tok{};
    u32 prev_index=-1;
    lexer::Import* lexer_import=nullptr;
    DynamicArray<lexer::Chunk*> lexer_chunks;
    DynamicArray<QuickArray<lexer::TokenInfo>*> lexer_tokens;
    void setup_token_iterator() {
        Assert(lexer);
        Assert(import_id != 0);
        lexer_import = lexer->getImport_unsafe(import_id);
        FOR(lexer_import->chunk_indices) {
            auto chunk = lexer->getChunk_unsafe(it);
            lexer_chunks.add(chunk);
            lexer_tokens.add(&chunk->tokens);
        }
    }
    void replace_token_state(TokenIteratorState* prev_state, u32 new_import_id) {
        prev_state->import_id=import_id;
        prev_state->head=head;
        prev_state->prev_tok=prev_tok;
        prev_state->prev_index=prev_index;
        prev_state->lexer_import=lexer_import;
        prev_state->lexer_chunks=lexer_chunks;
        prev_state->lexer_tokens=lexer_tokens;
        lexer_tokens.resize(0);
        lexer_chunks.resize(0);
        lexer_import = nullptr;
        prev_index = -1;
        prev_tok = {};
        head=0;
        import_id=new_import_id;
        setup_token_iterator();
    }
    void restore_token_state(TokenIteratorState* state) {
        import_id=state->import_id;
        head=state->head;
        prev_tok=state->prev_tok;
        prev_index=state->prev_index;
        lexer_import=state->lexer_import;
        lexer_chunks=state->lexer_chunks;
        lexer_tokens=state->lexer_tokens;  
    }
    
    
    // u32 last_fcindex=-1;
    // lexer::Chunk* last_chunk = nullptr;
    // lexer::TokenInfo* tokens_base = nullptr;
    // TODO: Optimize these functions by keeping track of the previously accessed chunk and tokens pointer
    //  If head+off was the same as last time then you can just return the same token info as last time.
    lexer::TokenInfo* getinfo(int off = 0) {
        static lexer::TokenInfo eof{lexer::TOKEN_EOF, };
        u32 fcindex,tindex;

        u32 ind = head + off;
        fcindex = ind >> TOKEN_ORIGIN_TOKEN_BITS;
        tindex = ind & ((1<<TOKEN_ORIGIN_TOKEN_BITS) - 1);
        // lexer->decode_import_token_index(head + off,&fcindex,&tindex);

        // if(last_fcindex == fcindex) {
        //     if(last_chunk->tokens.size() <= tindex)
        //         return &eof;
            
        //     auto info = tokens_base + tindex;
        //     return info;
        // }


        // lexer::Token out{};
        if(lexer_chunks.size() <= fcindex) {
            // out.type = lexer::TOKEN_EOF;
            return &eof;
        }

        lexer::TokenInfo* info = nullptr;

        // lexer::Chunk* chunk = lexer_chunks[fcindex];
        // info = chunk->tokens.getPtr(tindex);
        info = lexer_tokens[fcindex]->getPtr(tindex);

        if(!info) {
            return &eof;
        }
        #ifdef LEXER_DEBUG_DETAILS
        lexer::Chunk* chunk = lexer_chunks[fcindex];
        if(info->flags & lexer::TOKEN_FLAG_HAS_DATA)
            info->s = chunk->get_data(info->data_offset);
        #endif

        return info;
    }
    lexer::TokenInfo* getinfo(StringView* string, int off = 0) {
        static lexer::TokenInfo eof{lexer::TOKEN_EOF, };
        u32 fcindex,tindex;
        if(string) *string = {};

        u32 ind = head + off;
        fcindex = ind >> TOKEN_ORIGIN_TOKEN_BITS;
        tindex = ind & ((1<<TOKEN_ORIGIN_TOKEN_BITS) - 1);
        // lexer->decode_import_token_index(head + off,&fcindex,&tindex);

        // if(last_fcindex == fcindex) {
        //     if(last_chunk->tokens.size() <= tindex)
        //         return &eof;
            
        //     auto info = tokens_base + tindex;
        //     if(string && (info->flags & lexer::TOKEN_FLAG_HAS_DATA))
        //         *string = last_chunk->get_string(info->data_offset);
        //     return info;
        // }


        // lexer::Token out{};
        if(lexer_chunks.size() <= fcindex) {
            // out.type = lexer::TOKEN_EOF;
            // if(string)
            //     *string = {"",1};
            return &eof;
        }

        lexer::TokenInfo* info = nullptr;
        lexer::Chunk* chunk = lexer_chunks[fcindex];
        info = chunk->tokens.getPtr(tindex);
        // last_fcindex = fcindex;
        // last_chunk = chunk;
        // tokens_base = chunk->tokens.data();

        if(!info) {
            // if(string)
            //     *string = {"",1};
            return &eof;
        }
        if(string && (info->flags & lexer::TOKEN_FLAG_HAS_DATA)) {
            *string = chunk->get_string(info->data_offset);
            info->s = chunk->get_data(info->data_offset);
        }

        return info;
    }
    
    
    lexer::Token gettok(int off = 0) {
        u32 fcindex,tindex;
        u32 ind = head + off;
        // if(ind == prev_index)
        //     return prev_tok;

        // lexer->decode_import_token_index(head + off,&fcindex,&tindex);
        fcindex = ind >> TOKEN_ORIGIN_TOKEN_BITS;
        tindex = ind & ((1<<TOKEN_ORIGIN_TOKEN_BITS) - 1);
    
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
            // return out;
        }

        #ifdef LEXER_DEBUG_DETAILS
        if(info->flags & lexer::TOKEN_FLAG_HAS_DATA)
            out.s = chunk->get_data(info->data_offset);
        #endif

        out.flags = info->flags;
        out.type = info->type;
        out.origin = lexer->encode_origin(chunk->chunk_index,tindex);
        prev_tok = out;
        prev_index = ind;
        return out;
    }
    lexer::Token gettok(StringView* string, int off = 0) {
        u32 fcindex,tindex;
        lexer->decode_import_token_index(head + off,&fcindex,&tindex);
        if(string) *string = {};


        lexer::Token out{};
        if(lexer_chunks.size() <= fcindex) {
            // out.type = lexer::TOKEN_EOF;
            // if(string)
            //     *string = {"",1};
            return lexer_import->geteof();
        }
        lexer::Chunk* chunk = lexer_chunks[fcindex];

        auto info = chunk->tokens.getPtr(tindex);
        if(!info) {
            // if(string)
            //     *string = {"",1};
            // out.type = lexer::TOKEN_EOF;
            return lexer_import->geteof();
            // return out;
        }
        if((info->flags & lexer::TOKEN_FLAG_HAS_DATA)) {
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
    lexer::SourceLocation getloc(int off = 0) {
        return { gettok(off) };
    }
    void advance(int n = 1) {
        head += n;
    }
    lexer::SourceLocation srcloc(lexer::Token tok) {
        return {tok};
    }
    // returns token index into import of the next token to read
    // The token gettok will read.
    u32 gethead() { return head; }
    void sethead(int n) { head = n; }

    SignalIO parseTypeId(std::string& outTypeId, int* tokensParsed = nullptr);
    SignalIO parseStruct(ASTStruct*& astStruct);
    SignalIO parseNamespace(ASTScope*& astNamespace);
    SignalIO parseEnum(ASTEnum*& astEnum);
    // out_arguments may be null to parse but ignore arguments
    SignalIO parseAnnotationArguments(lexer::TokenRange* out_arguments);
    SignalIO parseAnnotation(StringView* out_annotation_name, lexer::TokenRange* out_arguments);
    // parses arguments and puts them into fncall->left
    SignalIO parseArguments(ASTExpression* fncall, int* count);
    SignalIO parseExpression(ASTExpression*& expression);
    SignalIO parseFlow(ASTStatement*& statement);
    // returns 0 if syntax is wrong for flow parsing
    SignalIO parseFunction(ASTFunction*& function, ASTStruct* parentStruct, bool is_operator);
    SignalIO parseDeclaration(ASTStatement*& statement);
    // out token contains a newly allocated string. use delete[] on it
    SignalIO parseBody(ASTScope*& bodyLoc, ScopeId parentScope, ParseFlags in_flags = PARSE_NO_FLAGS, ParseFlags* out_flags = nullptr);

};

ASTScope* ParseImport(u32 import_id, Compiler* compiler);
}