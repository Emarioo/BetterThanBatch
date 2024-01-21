#pragma once

#include "BetBat/AST.h"
#include "BetBat/PhaseContext.h"
#include "BetBat/Lexer.h"

struct CompileInfo;
namespace parser {
// TODO: Rename to ParseContext?
struct ParseInfo : public PhaseContext {
    lexer::Lexer* lexer=nullptr;
    // Compiler* compiler=nullptr;
    AST* ast=nullptr;
    Reporter* reporter = nullptr;

    // ParseInfo() : tokens(tokens){}
    // int index=0; // index of the next token, index-1 is the current token
    // TokenStream* tokens;
    int funcDepth=0;

    QuickArray<ContentOrder> nextContentOrder;
    ContentOrder getNextOrder() { Assert(nextContentOrder.size()>0); return nextContentOrder.last(); }

    ScopeId currentScopeId=0;
    std::string currentNamespace = "";
    bool ignoreErrors = false;

    struct LoopScope {
        DynamicArray<ASTStatement*> defers; // apply these when continue, break or return is encountered
    };
    // function or global scope
    struct FunctionScope {
        DynamicArray<ASTStatement*> defers; // apply these when return is encountered
        DynamicArray<LoopScope> loopScopes;
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

    u32 import_id=0;
    u32 head=0;
    lexer::Import* lexer_import=nullptr;
    DynamicArray<lexer::Chunk*> lexer_chunks;
    void setup_token_iterator() {
        Assert(lexer);
        Assert(import_id != 0);
        lexer_import = lexer->getImport_unsafe(import_id);
        FOR(lexer_import->chunk_indices) {
            lexer_chunks.add(lexer->getChunk_unsafe(it));
        }
    }
    // TODO: Optimize these functions by keeping track of the previously accessed chunk and tokens pointer
    //  If head+off was the same as last time then you can just return the same token info as last time.
    lexer::TokenInfo* getinfo(StringView* string = nullptr, int off = 0) {
        static lexer::TokenInfo eof{lexer::TOKEN_EOF, };
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
        if(string && (info->flags & lexer::TOKEN_FLAG_HAS_DATA))
            *string = {(char*)chunk->aux_data + info->data_offset + 1, chunk->aux_data[info->data_offset]};

        return info;
    }
    lexer::Token gettok(int off = 0) {
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
            // return out;
        }

        out.flags = info->flags;
        out.type = info->type;
        out.origin = lexer->encode_origin(chunk->chunk_index,tindex);
        return out;
    }
    lexer::Token gettok(StringView* string, int off = 0) {
        u32 fcindex,tindex;
        lexer->decode_import_token_index(head + off,&fcindex,&tindex);
    
        lexer::Token out{};
        if(lexer_chunks.size() <= fcindex) {
            // out.type = lexer::TOKEN_EOF;
            if(string)
                *string = {"",1};
            return lexer_import->geteof();
        }
        lexer::Chunk* chunk = lexer_chunks[fcindex];

        auto info = chunk->tokens.getPtr(tindex);
        if(!info) {
            if(string)
                *string = {"",1};
            // out.type = lexer::TOKEN_EOF;
            return lexer_import->geteof();
            // return out;
        }
        if(string && (info->flags & lexer::TOKEN_FLAG_HAS_DATA))
            *string = {(char*)chunk->aux_data + info->data_offset + 1, chunk->aux_data[info->data_offset]};

        out.flags = info->flags;
        out.type = info->type;
        out.origin = lexer->encode_origin(chunk->chunk_index,tindex);
        return out;
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

};

ASTScope* ParseImport(u32 import_id, Compiler* compiler);
}