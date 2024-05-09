/*
    Reformatting of messages from third-party tools
    (linkers, assemblers)
*/

#include "BetBat/CompilerEnums.h"
#include "Engone/Util/Array.h"
#include "BetBat/Bytecode.h"
#include "BetBat/x64_gen.h"

void ReformatDumpbinAsm(LinkerChoice linker, QuickArray<char>& inBuffer, QuickArray<char>* outBuffer, bool includeBytes);
/*
bin/inline_asm.asm: Assembler messages:
bin/inline_asm.asm:3: Error: no such instruction: `eae'
bin/inline_asm.asm:5: Error: no such instruction: `hoiho eax,9'
*/
void ReformatAssemblerError(LinkerChoice linker, Bytecode::ASM& asmInstance, QuickArray<char>& inBuffer, int line_offset);

// returns amount of errors
int ReformatLinkerError(LinkerChoice linker, QuickArray<char>& inBuffer, X64Program* program);