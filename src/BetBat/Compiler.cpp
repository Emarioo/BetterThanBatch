#include "BetBat/Compiler.h"

#include <string.h>

void CompilerFile(const char* path){
    int pathlen = strlen(path);
    Token extension{};
    for(int i=pathlen-1;i>=0;i++){
        char chr = path[i];
        if(chr == '.'){
            extension.str = (char*)(path + i); // Note: Extension.str is never modified. Casting away const should therfore be fine.
            extension.length = pathlen-i;
            break;
        }
    }
    if(extension == ".btb"){
        CompileScript(path);
    }else if(extension == ".btbi"){
        CompileInstructions(path);
    }
}

void CompileScript(const char* path){
    using namespace engone;
    auto text = ReadFile(path);
    Tokens tokens{};
    int err=0;
    Bytecode bytecode{};
    double seconds=0;
    
    auto startCompileTime = engone::MeasureSeconds();
    tokens = Tokenize(text);
    tokens.printTokens(14,0);
    // TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    
    Preprocess(tokens,&err);
    
    if(err)
        goto COMP_SCRIPT_END;
    
    // tokens.print();
    
    bytecode = GenerateScript(tokens,&err);
    if(err)
        goto COMP_SCRIPT_END;
    
    // OptimizeBytecode(bytecode);
    
    seconds = engone::StopMeasure(startCompileTime);
    log::out << "\nFully compiled "<<bytecode.getMemoryUsage()<<" bytes of bytecode in "<<(seconds*1e6)<<" us\n";

    Context::Execute(bytecode);

COMP_SCRIPT_END:
    bytecode.cleanup();
    tokens.cleanup();
    text.resize(0);
}
void CompileInstructions(const char* path){
    using namespace engone;
    auto text = ReadFile(path);
    auto startCompileTime = engone::MeasureSeconds();
    Tokens toks = Tokenize(text);
    // toks.printTokens(14,TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    Bytecode bytecode = GenerateInstructions(toks);
    OptimizeBytecode(bytecode);
    double seconds = engone::StopMeasure(startCompileTime);
    log::out << "Compiled in "<<seconds<<" seconds\n";
    Context::Execute(bytecode);

    text.resize(0);
    toks.cleanup();
    bytecode.cleanup();
}
void CompileDisassemble(const char* path){
    using namespace engone;
    auto text = ReadFile(path);
    auto startCompileTime = engone::MeasureSeconds();
    Tokens toks = Tokenize(text);
    Bytecode bytecode = GenerateScript(toks);
    OptimizeBytecode(bytecode);
    std::string textcode = Disassemble(bytecode);
    double seconds = engone::StopMeasure(startCompileTime);
    log::out << "Compiled in "<<seconds<<" seconds\n";
    Memory tempBuffer{1};
    tempBuffer.data = (char*)textcode.data();
    tempBuffer.max = tempBuffer.used = textcode.length();
    WriteFile("dis.txt",tempBuffer);
    // Context::Execute(bytecode);
    text.resize(0);
    toks.cleanup();
    bytecode.cleanup();
}
void test(){

    // std::string text = Disassemble(bytecode);
    // auto file = engone::FileOpen("out.txt",0,engone::FILE_WILL_CREATE);
    // engone::FileWrite(file,text.data(),text.length());
    // engone::FileClose(file);
}