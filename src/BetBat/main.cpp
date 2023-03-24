#include <stdio.h>

#include "BetBat/Utility.h"
#include "BetBat/Tokenizer.h"
#include "BetBat/Generator.h"
#include "BetBat/Context.h"

void run(const char* path){
    Tokens toks = Tokenize(ReadFile(path));
    toks.printTokens(14,TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    Bytecode bytecode = CompileInstructions(toks);
    
    std::string text = Disassemble(bytecode);
    
    auto file = engone::FileOpen("out.txt",0,engone::FILE_WILL_CREATE);
    engone::FileWrite(file,text.data(),text.length());
    engone::FileClose(file);
    
    Context::Execute(bytecode);
}
int main(int argc, const char** argv){
    // engone::Memory sample = ReadFile("tests/token/words.txt");
    // Tokens tokens = Tokenize(sample);
    // Bytecode bytecode = CompileScript(tokens);

    // Tokens toks = Tokenize(ReadFile("tests/token/words.txt"));
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    
    // run("tests/inst/func.txt");
    // run("tests/inst/numstr.txt");
    // run("tests/inst/apicalls.txt");
    for(int i=1;i<argc;i++){
        run(argv[i]);
    }
    
    if(argc<2){
        run("tests/inst/apicalls.txt");
    }
    
    
    // // Tokens toks = Tokenize(ReadFile("tests/inst/func.txt"));
    // Tokens toks = Tokenize(ReadFile("tests/inst/main.txt"));
    // toks.printTokens(14,TOKEN_PRINT_SUFFIXES);
    // // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);
    // Bytecode bytecode = CompileInstructions(toks);
    
    // Context::Execute(bytecode);
}