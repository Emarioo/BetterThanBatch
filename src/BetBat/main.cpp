#include <stdio.h>

#include "BetBat/Utility.h"
#include "BetBat/Tokenizer.h"
#include "BetBat/Generator.h"
#include "BetBat/Context.h"

int main(int argc, const char** argv){
    // engone::Memory sample = ReadFile("tests/token/words.txt");
    // Tokens tokens = Tokenize(sample);
    // Bytecode bytecode = CompileScript(tokens);

    Tokens toks = Tokenize(ReadFile("tests/inst/asm.txt"));
    toks.printTokens(20,TOKEN_PRINT_LN_COL);
    Bytecode bytecode = CompileInstructions(toks);
    
    Context context{};
    
    context.load(bytecode);
    context.execute();

    // tokens.printTokens();
    // tokens.printData();
}