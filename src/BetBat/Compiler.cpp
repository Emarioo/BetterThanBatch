#include "BetBat/Compiler.h"

void CompileScript(const char* path){
    using namespace engone;
    auto text = ReadFile(path);
    
    auto startCompileTime = engone::MeasureSeconds();
    Tokens toks = Tokenize(text);
    text.resize(0);
    toks.printTokens(14,TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    Bytecode bytecode = GenerateScript(toks);
    double seconds = engone::StopMeasure(startCompileTime);
    log::out << "Compiled in "<<seconds<<" seconds\n";

    Context::Execute(bytecode);

    bytecode.cleanup();
    toks.cleanup();
}
void CompileInstructions(const char* path){
    using namespace engone;
    auto text = ReadFile(path);
    auto startCompileTime = engone::MeasureSeconds();
    Tokens toks = Tokenize(text);
    text.resize(0);
    toks.printTokens(14,TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    Bytecode bytecode = GenerateInstructions(toks);
    double seconds = engone::StopMeasure(startCompileTime);
    log::out << "Compiled in "<<seconds<<" seconds\n";
    Context::Execute(bytecode);
}
void test(){

    // std::string text = Disassemble(bytecode);
    // auto file = engone::FileOpen("out.txt",0,engone::FILE_WILL_CREATE);
    // engone::FileWrite(file,text.data(),text.length());
    // engone::FileClose(file);
}