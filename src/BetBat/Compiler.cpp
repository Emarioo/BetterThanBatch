#include "BetBat/Compiler.h"

#include <string.h>

void CompileInfo::cleanup(){
    for(auto& pair : tokenStreams){
        TokenStream::Destroy(pair.second.stream);
    }
}
bool CompileInfo::addStream(TokenStream* stream){
    auto pair = tokenStreams.find(stream->streamName);
    if(pair!=tokenStreams.end()) return false;
    auto ptr = &(tokenStreams[stream->streamName] = {});
    ptr->stream = stream;
    return true;
}
CompileInfo::FileInfo* CompileInfo::getStream(const std::string& name){
    auto pair = tokenStreams.find(name);
    if(pair == tokenStreams.end()) return 0;
    return &pair->second;
}
// does not handle backslash
bool ParseFile(CompileInfo& info, const std::string& name){
    using namespace engone;
    TokenStream* tokenStream = TokenStream::Tokenize(name);
    if(!tokenStream){
        log::out << log::RED << "Failed tokenizing " << name <<"\n";
        return false;
    }
    info.lines += tokenStream->lines;
    info.readBytes += tokenStream->readBytes;
     if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        TokenStream* old = tokenStream;
        tokenStream = Preprocess(tokenStream, &info.errors);
        if (tokenStream->enabled == LAYER_PREPROCESSOR)
            tokenStream->print();
        TokenStream::Destroy(old);
    }
    info.addStream(tokenStream);
    std::string path = "";
    int slashI = name.find_last_of("/");
    if(slashI!=-1){
        path = name.substr(0,slashI + 1);
    }
    for(std::string importName : tokenStream->importList){
        if(importName.find("./")==0){
            importName = path + importName.substr(2);
        }
        auto fileInfo = info.getStream(importName);
        if(!fileInfo){
            bool yes = ParseFile(info, importName);
        }
    }
    ASTBody* body = ParseTokens(tokenStream,info.ast,&info.errors);
    if(!body)
        return false;
    bool yes = EvaluateTypes(info.ast,body,&info.errors);
    if(yes){
        info.ast->appendToMainBody(body);
    }
    // info.ast->print();
    return true;
}

void CompileScript(const char *path, int extra) {
    using namespace engone;
    int err = 0;
    AST* ast=0;
    Bytecode bytecode{};
    BytecodeX* bytecodeX=0;
    double seconds = 0;
    std::string dis;
    int bytes = 0;
    
    // NOTE: Parser and generator uses tokens. Do not free tokens before compilation is complete.

    auto startCompileTime = engone::MeasureSeconds();
    
    CompileInfo compileInfo{};
    compileInfo.ast = AST::Create();
    EvaluateTypes(compileInfo.ast,compileInfo.ast->mainBody,&compileInfo.errors);
    ParseFile(compileInfo, path);
    ast = compileInfo.ast;
    err = compileInfo.errors;

    // if (tokens.enabled == 0)
    //     tokens.printTokens(14, true);
    // TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    
    // if (tokens.enabled & LAYER_PREPROCESSOR) {
    //     Preprocess(tokens, &err);
    //     if (tokens.enabled == LAYER_PREPROCESSOR)
    //         tokens.print();
    // }
    // if (err)
    //     goto COMP_SCRIPT_END;

    // if (tokens.enabled & LAYER_PARSER){
    //     if(tokens.isVersion("2"))
    //         ast = ParseTokens(tokens, 0, &err);
    //     else
    //         bytecode = GenerateScript(tokens, &err);
    // }
    // if (err)
    //     goto COMP_SCRIPT_END;
    
    _VLOG(compileInfo.ast->print();)
    // if (tokens.enabled & LAYER_GENERATOR){
    if(compileInfo.errors==0 && ast)
        bytecodeX = Generate(ast, &err);
    // }
    if (err)
        goto COMP_SCRIPT_END;

    // dis = Disassemble(bytecode);
    // WriteFile("dis.txt",dis);
    // if (tokens.enabled & LAYER_OPTIMIZER){
    //     if(bytecodeX)
    //         ;
    //     else
    //         OptimizeBytecode(bytecode);
    // }
    _VLOG(log::out << "\n";)
    seconds = engone::StopMeasure(startCompileTime);
    if(bytecodeX)
        bytes = bytecodeX->getMemoryUsage();
    else
        bytes = bytecode.getMemoryUsage();
    // _VLOG(
        log::out << "Compiled " << FormatUnit((uint64)compileInfo.lines) << " lines ("<<FormatBytes(compileInfo.readBytes)<<") in " << FormatTime(seconds) << "\n (" << FormatUnit(compileInfo.lines / seconds) << " lines/s)\n";
    // )

    // if (tokens.enabled & LAYER_INTERPRETER) {
    //     if(bytecodeX){
        {
            Interpreter inp{};
            inp.execute(bytecodeX);
            inp.cleanup();
        }
        // } else
        //     Context::Execute(bytecode);

        // int totalinst = 0;
        // double combinedtime = 0;
        // Performance perf;
        // int times = extra;
        // for (int i = 0; i < times; i++)
        // {
            // Context::Execute(bytecode, &perf);
            // totalinst += perf.instructions;
            // combinedtime += perf.exectime;
        // }
        // // log::out << "Total " << totalinst<<" insts (avg "<<(totalinst/times)<<")\n";
        // if (times!=1){
        //     _VLOG(log::out << "Combined " << FormatTime(combinedtime) << " time (avg " << FormatTime(combinedtime / times) << ")\n";)
        // }
    // }


COMP_SCRIPT_END:
    bytecode.cleanup();
    BytecodeX::Destroy(bytecodeX);
    AST::Destroy(ast);
    compileInfo.cleanup();
    // tokens.cleanup();
    // text.resize(0);
}
/* #region  */
// void CompileInstructions(const char *path)
// {
//     using namespace engone;
//     auto text = ReadFile(path);
//     auto startCompileTime = engone::MeasureSeconds();
//     Tokens toks = Tokenize(text);
//     // toks.printTokens(14,TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
//     // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
//     Bytecode bytecode = GenerateInstructions(toks);
//     OptimizeBytecode(bytecode);
//     double seconds = engone::StopMeasure(startCompileTime);
//     log::out << "Compiled in " << FormatTime(seconds) << "\n";
//     Context::Execute(bytecode);

//     text.resize(0);
//     toks.cleanup();
//     bytecode.cleanup();
// }
void CompileDisassemble(const char *path)
{
    using namespace engone;
    log::out << log::RED << __FUNCTION__ << " is broken\n";
    // auto text = ReadFile(path);
    // auto startCompileTime = engone::MeasureSeconds();
    // TokenStream toks = TokenStream::Tokenize(text);
    // Bytecode bytecode = GenerateScript(toks);
    // OptimizeBytecode(bytecode);
    // std::string textcode = Disassemble(bytecode);
    // double seconds = engone::StopMeasure(startCompileTime);
    // log::out << "Compiled in " << FormatTime(seconds) << "\n";
    // Memory tempBuffer{1};
    // tempBuffer.data = (char *)textcode.data();
    // tempBuffer.max = tempBuffer.used = textcode.length();
    // WriteFile("dis.txt", tempBuffer);
    // // Context::Execute(bytecode);
    // text.resize(0);
    // toks.cleanup();
    // bytecode.cleanup();
}
/* #endregion */