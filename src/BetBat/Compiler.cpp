#include "BetBat/Compiler.h"

void CompileScript(const char* path){
    using namespace engone;
    auto text = ReadFile(path);
    
    auto startCompileTime = engone::MeasureSeconds();
    Tokens toks = Tokenize(text);
    // toks.printTokens(14,TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    Bytecode bytecode = GenerateScript(toks);
    bytecode.printStats();
    OptimizeBytecode(bytecode);
    bytecode.printStats();
    double seconds = engone::StopMeasure(startCompileTime);
    log::out << "Compiled in "<<seconds<<" seconds\n";

    Context::Execute(bytecode);

    text.resize(0);
    bytecode.cleanup();
    toks.cleanup();
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