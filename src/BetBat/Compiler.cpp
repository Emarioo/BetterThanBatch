#include "BetBat/Compiler.h"

#include <string.h>

void CompilerFile(const char *path)
{
    int pathlen = strlen(path);
    Token extension{};
    for (int i = pathlen - 1; i >= 0; i++)
    {
        char chr = path[i];
        if (chr == '.')
        {
            extension.str = (char *)(path + i); // Note: Extension.str is never modified. Casting away const should therfore be fine.
            extension.length = pathlen - i;
            break;
        }
    }
    if (extension == ".btb")
    {
        CompileScript(path);
    }
    else if (extension == ".btbi")
    {
        // CompileInstructions(path);
    }
}

void CompileScript(const char *path, int extra)
{
    using namespace engone;
    auto text = ReadFile(path);
    if (!text.data)
        return;
    Tokens tokens{};
    int err = 0;
    Bytecode bytecode{};
    double seconds = 0;
    std::string dis;

    auto startCompileTime = engone::MeasureSeconds();
    tokens = Tokenize(text);
    if (tokens.enabled == 0)
        tokens.printTokens(14, true);
    // TOKEN_PRINT_SUFFIXES|TOKEN_PRINT_QUOTES);
    // toks.printTokens(14,TOKEN_PRINT_LN_COL|TOKEN_PRINT_SUFFIXES);

    if (tokens.enabled & LAYER_PREPROCESSOR)
    {
        Preprocess(tokens, &err);
        if (tokens.enabled == LAYER_PREPROCESSOR)
            tokens.print();
    }
    if (err)
        goto COMP_SCRIPT_END;

    if (tokens.enabled & LAYER_PARSER)
        bytecode = GenerateScript(tokens, &err);
    if (err)
        goto COMP_SCRIPT_END;

    // dis = Disassemble(bytecode);
    // WriteFile("dis.txt",dis);
    if (tokens.enabled & LAYER_OPTIMIZER)
        OptimizeBytecode(bytecode);
    _VLOG(log::out << "\n";)
    seconds = engone::StopMeasure(startCompileTime);
    _VLOG(log::out << "Compiled " << FormatUnit((uint64)tokens.lines) << " lines in " << FormatTime(seconds) << "\n (" << FormatUnit(tokens.lines / seconds) << " lines/s, " << FormatBytes(bytecode.getMemoryUsage()) << ")\n";)

    if (tokens.enabled & LAYER_INTERPRETER)
    {
        int totalinst = 0;
        double combinedtime = 0;
        Performance perf;
        int times = extra;
        for (int i = 0; i < times; i++)
        {
            Context::Execute(bytecode, &perf);
            totalinst += perf.instructions;
            combinedtime += perf.exectime;
        }
        // log::out << "Total " << totalinst<<" insts (avg "<<(totalinst/times)<<")\n";
        if (times!=1){
            _VLOG(log::out << "Combined " << FormatTime(combinedtime) << " time (avg " << FormatTime(combinedtime / times) << ")\n";)
        }
    }

COMP_SCRIPT_END:
    bytecode.cleanup();
    tokens.cleanup();
    text.resize(0);
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
    auto text = ReadFile(path);
    auto startCompileTime = engone::MeasureSeconds();
    Tokens toks = Tokenize(text);
    Bytecode bytecode = GenerateScript(toks);
    OptimizeBytecode(bytecode);
    std::string textcode = Disassemble(bytecode);
    double seconds = engone::StopMeasure(startCompileTime);
    log::out << "Compiled in " << FormatTime(seconds) << "\n";
    Memory tempBuffer{1};
    tempBuffer.data = (char *)textcode.data();
    tempBuffer.max = tempBuffer.used = textcode.length();
    WriteFile("dis.txt", tempBuffer);
    // Context::Execute(bytecode);
    text.resize(0);
    toks.cleanup();
    bytecode.cleanup();
}
/* #endregion */