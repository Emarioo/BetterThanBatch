#include "BetBat/Reformatter.h"

void ReformatDumpbinAsm(LinkerChoice linker, QuickArray<char>& inBuffer, QuickArray<char>* outBuffer, bool includeBytes) {
    using namespace engone;
    Assert(!outBuffer); // not implemented
    
    int startIndex = 0;
    int endIndex = 0;
    switch(linker){
    case LINKER_MSVC: {
     // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 5)
            break;
    }
    startIndex = index;
    index = 0;
    // Find summary
    const char* summary_str = "Summary";
    int summary_len = strlen(summary_str);
    int summary_correct = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        
        if(summary_str[summary_correct] == chr) {
            summary_correct++;
            if(summary_correct == summary_len) {
                endIndex = index - summary_len - 1 + 1; // -1 to skip newline before Summary, +1 for exclusive index
                break;
            }
        } else {
            summary_correct = 0;
        }
    }
    // endIndex = index; endIndex is set in while loop
    break;
    }
    case LINKER_GNU: {
    // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 6)
            break;
    }
    startIndex = index;
    index = 0;
    endIndex = inBuffer.size();
    break;
    }
    default: {
        Assert(false);
        break;
    }
    }
    // Print disassembly
    int len = endIndex - startIndex;
    if(outBuffer) {
        outBuffer->resize(len);
        memcpy(outBuffer->data(), inBuffer.data() + startIndex, len);
    } else {
        log::out.print(inBuffer.data() + startIndex, len, true);
    }
}
/*
bin/inline_asm.asm: Assembler messages:
bin/inline_asm.asm:3: Error: no such instruction: `eae'
bin/inline_asm.asm:5: Error: no such instruction: `hoiho eax,9'

*/
void ReformatAssemblerError(LinkerChoice linker, Bytecode::ASM& asmInstance, QuickArray<char>& inBuffer, int line_offset) {
    using namespace engone;
    // Assert(!outBuffer); // not implemented
    
    int startIndex = 0;
    int endIndex = 0;
    struct ASMError {
        std::string file;
        int line;
        std::string message;
    };
    DynamicArray<ASMError> errors{};
    
if(linker == LINKER_MSVC) {
     // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 1)
            break;
    }
    
    
    
    // NOTE: We assume that each line is an error. As far as I have seen, this seems to be the case.
    ASMError asmError{};
    bool parseFileAndLine = true;
    int startOfFile = 0;
    int endOfFile = 0; // exclusive
    int startOfLineNumber = 0; // index where we suspect it to be
    int endOfLineNumber = 0; // exclusive
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        
        if(chr == '\n' || inBuffer.size() == index) {
            Assert(!parseFileAndLine);
            errors.add(asmError);
            asmError = {};
            parseFileAndLine = true;
            startOfFile = index;
            continue;
        }
        
        if(parseFileAndLine) {
            if(chr == '(') {
                endOfFile = index - 1;
                startOfLineNumber = index;
            }
            if(chr == ')')
                endOfLineNumber = index-1;
            
            if(chr == ':') {
                // parse number
                char* str = inBuffer.data() + startOfLineNumber;
                inBuffer[endOfLineNumber] = 0;
                asmError.line = atoi(str);
                
                asmError.file.resize(endOfFile - startOfFile);
                memcpy((char*)asmError.file.data(), inBuffer.data() + startOfFile, endOfFile - startOfFile);
                
                parseFileAndLine = false;
                continue;
            }
            endOfFile = index;
        } else { // parse message
            asmError.message += chr;
        }
    }
} else {
     // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 1)
            break;
    }
    
    // NOTE: We assume that each line is an error. As far as I have seen, this seems to be the case.
    ASMError asmError{};
    bool parseFileAndLine = true;
    bool parseFindFinalColon = false;
    int startOfFile = 0;
    int endOfFile = 0; // exclusive
    int startOfLineNumber = 0; // index where we suspect it to be
    int endOfLineNumber = 0; // exclusive
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        
        if(chr == '\n' || inBuffer.size() == index) {
            Assert(!parseFileAndLine);
            errors.add(asmError);
            asmError = {};
            parseFileAndLine = true;
            startOfFile = index;
            continue;
        }
        
        if(parseFileAndLine) {
            if(chr == ':') {
                if(!parseFindFinalColon) {
                    endOfFile = index - 1;
                    startOfLineNumber = index;
                    parseFindFinalColon = true;
                } else {
                    parseFindFinalColon = false;
                    endOfLineNumber = index-1;
                    // parse number
                    char* str = inBuffer.data() + startOfLineNumber;
                    inBuffer[endOfLineNumber] = 0;
                    asmError.line = atoi(str);
                    
                    asmError.file.resize(endOfFile - startOfFile);
                    memcpy((char*)asmError.file.data(), inBuffer.data() + startOfFile, endOfFile - startOfFile);
                    
                    parseFileAndLine = false;
                    continue;
                }
            }
            endOfFile = index;
        } else { // parse message
            asmError.message += chr;
        }
    }
}
    
    FOR(errors) {
        int line = asmInstance.lineStart + it.line + line_offset;
        if(linker == LINKER_MSVC)
            line-=1;
        log::out << log::RED << TrimCWD(asmInstance.file) << ":" << (line) << " (assembler error):";
        // if(linker == LINKER_MSVC)
        //     log::out << " ";
        log::out << log::NO_COLOR << it.message << "\n";
    }
}

int ReformatLinkerError(LinkerChoice linker, QuickArray<char>& inBuffer, X64Program* program) {
    using namespace engone;
    Assert(program);
    
    struct ErrorInfo {
        std::string func;
        std::string textOffset_hex;
        std::string message;
        int textOffset;
        bool no_location = false;
        // std::string symbolName;
    };
    DynamicArray<ErrorInfo> errors{};
    ErrorInfo errorInfo{};

    switch(linker) {
    case LINKER_MSVC: {
    // NOTE: MSVC linker may sometimes provide hints about potential functions you meant to use.
    //  We ignore those at the moment. Perhaps we shouldn't?

    enum State {
        BEGIN, // beginning of a new error
        MESSAGE, // the error message
    };
    State state = BEGIN;
    std::string lastSymbol = "";
    std::string lastFunc = "";
    int msg_start = 0;
    int index = 0;
    int colon_count = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        char chr1 = 0;
        if(index+1<inBuffer.size())
            chr1 = inBuffer[index+1];
        char chr2 = 0;
        if(index+2<inBuffer.size())
            chr2 = inBuffer[index+2];
        index++;
        
        switch(state) {
            case BEGIN: {
                if(chr == ' ' && chr1 == ':' && chr2 == ' ') {
                    index+=2; // skip ': '
                    state = MESSAGE;
                    msg_start = index;
                }
                break;
            }
            case MESSAGE: {
                if(chr == '\n') {
                    int msg_end = index-1; // -1 to exclude newline
                    state = BEGIN;
                    errorInfo.message = std::string(inBuffer.data() + msg_start, msg_end - msg_start);
                    
                    int extras = 0;
                    if(program && !lastSymbol.empty()) {
                        // this is really slow
                        for(int i=0;i<program->namedUndefinedRelocations.size();i++) {
                            auto& rel = program->namedUndefinedRelocations[i];
                            if(rel.name == lastSymbol) {
                                errorInfo.textOffset = rel.textOffset;
                                errorInfo.textOffset_hex = NumberToHex(rel.textOffset);
                                errorInfo.func = lastFunc;
                                errorInfo.no_location = false;
                                errors.add(errorInfo);
                                extras++;
                            }
                        }
                    }
                    if(extras == 0) {
                        errorInfo.no_location = true;
                        errors.add(errorInfo);
                    }
                    lastFunc = "";
                    lastSymbol = "";
                    break;
                }
                const char* symbol_str = "external symbol ";
                int symbol_len = strlen(symbol_str);
                int correct = 0;
                for(;correct<symbol_len && index+correct < inBuffer.size();correct++) {
                    char c = inBuffer[index + correct];
                    if(c != symbol_str[correct])
                        break;
                }
                if(correct == symbol_len) {
                    index += symbol_len;
                    int symbol_start = index;
                    while(index<inBuffer.size()) {
                        char chr = inBuffer[index];
                        index++;
                        if(chr == ' ')
                            break;
                    }   
                    int symbol_end = index-1;
                    lastSymbol = std::string(inBuffer.data() + symbol_start, symbol_end - symbol_start);
                }
                const char* func_str = "in function ";
                int func_len = strlen(func_str);
                correct = 0;
                for(;correct<func_len && index+correct < inBuffer.size();correct++) {
                    char c = inBuffer[index + correct];
                    if(c != func_str[correct])
                        break;
                }
                if(correct == func_len) {
                    index += func_len;
                    int func_start = index;
                    while(index<inBuffer.size()) {
                        char chr = inBuffer[index];
                        index++;
                        if(chr == ' ' || chr == '\n') {
                            index--;
                            break;
                        }
                    }
                    int func_end = index-1;
                    lastFunc = std::string(inBuffer.data() + func_start, func_end - func_start);
                }
                break;
            }
            default: {
                Assert(false);
                break;
            }
        }
    }
    break;
    }
    case LINKER_GNU: {
    // HOLY CRAP THIS CODE IS BEAUTIFUL AND SIMPLE
    enum State {
        BEGIN, // beginning of a new error
        FUNCTION,
        OFFSET, // parse text offset
        MESSAGE, // the error message
    };
    State state = BEGIN;
    int hex_start = 0;
    int msg_start = 0;
    int func_start = 0;
    int index = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        char chr1 = 0;
        if(index+1<inBuffer.size())
            chr1 = inBuffer[index+1];
        char chr2 = 0;
        if(index+2<inBuffer.size())
            chr2 = inBuffer[index+2];
        index++;
        
        switch(state) {
            case BEGIN: {
                if(chr == '+' && chr1 == '0' && chr2 == 'x') {
                    state = OFFSET;
                    index+=2;
                    hex_start = index;
                }
                // This catches the usr/bin/ld message which may contain good info such as: /usr/bin/ld: DWARF error: offset (4294967292) greater than or equal to .debug_abbrev size (117)
                
                if(chr == 'l' && chr1 == 'd' && chr2 == ':') {
                    state = MESSAGE;
                    errorInfo.no_location = true;
                    index+=3; // skip "d: "
                    msg_start = index;
                }
                if(chr == ':') {
                    static const char* const func_msg = " in function";
                    static const int func_msg_len = strlen(func_msg);
                    int correct = 0;
                    for(;correct<func_msg_len && index+correct < inBuffer.size();correct++) {
                        char c = inBuffer[index + correct];
                        if(c != func_msg[correct])
                            break;
                    }
                    if(correct == func_msg_len) {
                        index += func_msg_len +2; // +2 to skip ' `'
                        state = FUNCTION;
                        func_start = index;
                    }
                }
                break;
            }
            case FUNCTION: {
                if(chr == '\'') {
                    int func_end = index-1;
                    index++; // skip :
                    state = BEGIN;
                    errorInfo.func = std::string(inBuffer.data() + func_start, func_end - func_start);
                }
                break;
            }
            case OFFSET: {
                // Parse hexidecimal
                if(chr == ')') {
                    int hex_end = index-1; // exclusive, -1 since index is at : right now

                    u64 off = ConvertHexadecimal_content(inBuffer.data() + hex_start, hex_end - hex_start);
                    errorInfo.no_location = false;
                    errorInfo.textOffset = off;
                    errorInfo.textOffset_hex = std::string(inBuffer.data() + hex_start, hex_end - hex_start);

                    index+=2; // skip ': '

                    state = MESSAGE;
                    msg_start = index;
                }
                break;
            }
            case MESSAGE: {
                if(chr == '\n') {
                    int msg_end = index-1; // -1 to exclude newline
                    state = BEGIN;
                    errorInfo.message = std::string(inBuffer.data() + msg_start, msg_end - msg_start);
                    errors.add(errorInfo);
                }
                break;
            }
            default: {
                Assert(false);
                break;
            }
        }
    }
    break;
    }
    default: {
        log::out << log::GOLD << "Reformat of linker errors for "<<ToString(linker) << " is incomplete. Printing direct errors from linker.\n";
        log::out.print(inBuffer.data(), inBuffer.size());
        log::out.flush();
        break;
    }
    }
    
    if(errors.size() > 0 && (!program || !program->debugInformation)) {
        // NOTE: program == nullptr shouldn't really happen
        log::out << log::GOLD << "Enabling debug information will display "<<log::RED<<"file:line"<<log::GOLD<<" instead of "<<log::RED<<".text+0x0"<<log::GOLD<<"\n";
    }
    FOR(errors) {
        bool found = false;
        if(!it.no_location && program && program->debugInformation) {
            auto d = program->debugInformation;
            int lineNumber = 0;
            int fileIndex = 0;
            // TODO: This will be slow with many functions

            Assert(false); // nocheckin, uncomment code and fix
            // for(int k=0;k<d->functions.size();k++) {
            //     auto& func = d->functions[k];
            //     if(func-.funcStart <= it.textOffset && it.textOffset < func.funcEnd) {
            //         fileIndex = func.fileIndex;
            //         if(func.lines.size()>0)
            //             lineNumber = func.lines[0].lineNumber;
            //         for(int i=0;i < func.lines.size();i++) {
            //             auto& line = func.lines[i];
            //             if(it.textOffset < line.asm_address) { // NOTE: Is funcStart + funcOffset correct?
            //             // if(it.textOffset < func.funcStart + line.funcOffset) { // NOTE: Is funcStart + funcOffset correct?
            //                 found = true;
            //                 break;
            //             }
            //             lineNumber = line.lineNumber;
            //         }
            //         if(!found) {
            //             if(func.lines.size()>0)
            //                 lineNumber = func.lines.last().lineNumber;
            //             found = true;
            //         }
            //         if(found)
            //             break;
            //     }
            // }
            if(found) {
                log::out << log::RED << TrimCWD(d->files[fileIndex])<<":" << lineNumber<<" (linker error): ";
                log::out << log::NO_COLOR << it.message << "\n";
            }
        }
        if(!found) {
            if(it.no_location) {
                log::out << log::RED << "linker: ";
            } else if(it.func.empty()) {
                log::out << log::RED << ".text+0x" << it.textOffset_hex<<": ";
            } else {
                log::out << log::RED << ".text+0x" << it.textOffset_hex<<" ("<<it.func<<"): ";
            }
            log::out << log::NO_COLOR << it.message << "\n";
        }
    }
    return errors.size();
}