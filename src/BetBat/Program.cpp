#include "BetBat/Program.h"

#include "BetBat/Compiler.h"

void Program::compute_libraries() {
    libraries.resize(0);

    for (auto &rel : namedUndefinedRelocations) {
        bool found = false;
        if (rel.library_path == "") {
            // intrinsic functions or 'prints' creates a relocation
            // to GetStdHandle and WriteFile without referring to
            // a library. Perhaps this shouldn't be allowed but
            // i believe linkers automatically link with the
            // basic libraries which GetStdHandle and WriteFile comes from.

            // Inline assembly may also create relocations without library names.
            // We just have to trust that the user linked with the library manually.
            continue;
        }
        for (auto &s : libraries) {
            if (s == rel.library_path) {
                found = true;
                break;
            }
        }
        if (!found)
            libraries.add(rel.library_path);
    }
    for (auto &fl : forced_libraries) {
        bool found = false;
        for (auto &s : libraries) {
            if (s == fl) {
                found = true;
                break;
            }
        }
        if (!found)
            libraries.add(fl);
    }
}

void Program::Destroy(Program *program) {
    using namespace engone;
    Assert(program);
    program->~Program();
    TRACK_FREE(program, Program);
    // engone::Free(program,sizeof(Program));
}
Program *Program::Create() {
    using namespace engone;

    // auto program = (Program*)engone::Allocate(sizeof(Program));
    auto program = TRACK_ALLOC(Program);
    new (program) Program();
    return program;
}

bool Program::finalize_program(Compiler* compiler) {
    Assert(debugInformation); // we expect debug information?

    auto bytecode = compiler->bytecode;

    index_of_main = bytecode->index_of_main;

    // prog->debugInformation = bytecode->debugInformation;
    // bytecode->debugInformation = nullptr;

    if (bytecode->dataSegment.size() != 0) {
        globalData = TRACK_ARRAY_ALLOC(u8, bytecode->dataSegment.size());
        // prog->globalData = (u8*)engone::Allocate(bytecode->dataSegment.used);
        Assert(globalData);
        globalSize = bytecode->dataSegment.size();
        memcpy(globalData, bytecode->dataSegment.data(), globalSize);

        // OutputAsHex("data.txt", (char*)prog->globalData, prog->globalSize);
    }

    for (int i = 0; i < bytecode->exportedFunctions.size(); i++) {
        auto &sym = bytecode->exportedFunctions[i];
        addExportedSymbol(sym.name, sym.tinycode_index);
        // X64Program::ExportedSymbol tmp{};
        // tmp.name = sym.name;
        // tmp.textOffset = addressTranslation[sym.location];
        // prog->exportedSymbols.add(tmp);
    }

    compute_libraries();
    return true;
}

void ProgramBuilder::init(Compiler* compiler, TinyBytecode* tinycode) {
    this->compiler = compiler;
    program = compiler->program;
    current_funcprog_index = tinycode->index;
    bytecode = compiler->bytecode;
    this->tinycode = tinycode;
    funcprog = compiler->program->createProgram(tinycode->index);
}

bool ProgramBuilder::reserve(u32 newAllocationSize) {
    if (newAllocationSize == 0) {
        if (funcprog->_allocationSize != 0) {
            TRACK_ARRAY_FREE(funcprog->text, u8, funcprog->_allocationSize);
            // engone::Free(text, _allocationSize);
        }
        funcprog->text = nullptr;
        funcprog->_allocationSize = 0;
        funcprog->head = 0;
        return true;
    }
    if (!funcprog->text) {
        // text = (u8*)engone::Allocate(newAllocationSize);
        funcprog->text = TRACK_ARRAY_ALLOC(u8, newAllocationSize);
        Assert(funcprog->text);
        // initialization of elements is done when adding them
        if (!funcprog->text)
            return false;
        funcprog->_allocationSize = newAllocationSize;
        return true;
    } else {
        u8 *newText = TRACK_ARRAY_REALLOC(
                funcprog->text, u8, funcprog->_allocationSize, newAllocationSize);
        // TRACK_DELS(u8, funcprog->_allocationSize);
        // u8* newText = (u8*)engone::Reallocate(funcprog->text,
        // funcprog->_allocationSize, newAllocationSize); TRACK_ADDS(u8,
        // newAllocationSize);
        Assert(newText);
        if (!newText)
            return false;
        funcprog->text = newText;
        funcprog->_allocationSize = newAllocationSize;
        if (funcprog->head > newAllocationSize) {
            funcprog->head = newAllocationSize;
        }
        return true;
    }
    return false;
}

void ProgramBuilder::emit1(u8 byte) {
    ensure_bytes(1);
    *(funcprog->text + funcprog->head) = byte;
    funcprog->head++;
}
void ProgramBuilder::emit2(u16 word) {
    ensure_bytes(2);

    auto ptr = (u8 *)&word;
    *(funcprog->text + funcprog->head + 0) = *(ptr + 0);
    *(funcprog->text + funcprog->head + 1) = *(ptr + 1);
    funcprog->head += 2;
}
void ProgramBuilder::emit3(u32 word) {
    if (funcprog->head > 0) {
        // cvtsi2ss instructions have rex smashed in between the opcode or something like that, this assert will catch it.
        // TODO: Just because the value of the previous byte is REXW doesn't mean it's meant to be. It could be an immediate or modrm byte that happens to be equal to REXW. We can't assert like this. We should still catch bugs where REXW was specified before cvtsi2ss instead of inside.
        // Assert(*(funcprog->text + funcprog->head - 1) != PREFIX_REXW);
    }
    Assert(0 == (word & 0xFF000000));
    ensure_bytes(3);

    auto ptr = (u8 *)&word;
    *(funcprog->text + funcprog->head + 0) = *(ptr + 0);
    *(funcprog->text + funcprog->head + 1) = *(ptr + 1);
    *(funcprog->text + funcprog->head + 2) = *(ptr + 2);
    funcprog->head += 3;
}
void ProgramBuilder::emit4(u32 dword) {
    // wish we could assert to find bugs but immediates may be zero and won't
    // work. we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(4);
    auto ptr = (u8 *)&dword;

    // deals with non-alignment
    *(funcprog->text + funcprog->head + 0) = *(ptr + 0);
    *(funcprog->text + funcprog->head + 1) = *(ptr + 1);
    *(funcprog->text + funcprog->head + 2) = *(ptr + 2);
    *(funcprog->text + funcprog->head + 3) = *(ptr + 3);
    funcprog->head += 4;
}
void ProgramBuilder::emit8(u64 qword) {
    // wish we could assert to find bugs but immediates may be zero and won't
    // work. we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(8);
    auto ptr = (u8 *)&qword;

    // deals with non-alignment
    *(funcprog->text + funcprog->head + 0) = *(ptr + 0);
    *(funcprog->text + funcprog->head + 1) = *(ptr + 1);
    *(funcprog->text + funcprog->head + 2) = *(ptr + 2);
    *(funcprog->text + funcprog->head + 3) = *(ptr + 3);
    *(funcprog->text + funcprog->head + 4) = *(ptr + 4);
    *(funcprog->text + funcprog->head + 5) = *(ptr + 5);
    *(funcprog->text + funcprog->head + 6) = *(ptr + 6);
    *(funcprog->text + funcprog->head + 7) = *(ptr + 7);
    funcprog->head += 8;
}
void ProgramBuilder::emit_bytes(const u8 *arr, u64 len) {
    ensure_bytes(len);

    memcpy(funcprog->text + funcprog->head, arr, len);
    funcprog->head += len;
}


void FunctionProgram::printHex(const char *path) {
    using namespace engone;
    Assert(this);
    if (path) {
        OutputAsHex(path, (char *)text, head);
    } else {
#define HEXIFY(X) (char)(X < 10 ? '0' + X : 'A' + X - 10)
        log::out << log::LIME << "HEX:\n";
        for (int i = 0; i < head; i++) {
            u8 a = text[i] >> 4;
            u8 b = text[i] & 0xF;
            log::out << HEXIFY(a) << HEXIFY(b) << " ";
        }
        log::out << "\n";
#undef HEXIFY
    }
}
#ifdef gone
void X64Program::printAsm(const char *path, const char *objpath) {
    using namespace engone;
    Assert(this);
    if (!objpath) {
#ifdef OS_WINDOWS
        FileCOFF::WriteFile("bin/garb.obj", this);
#else
        FileELF::WriteFile("bin/garb.o", this);
#endif
    }

    auto file = FileOpen(path, FILE_CLEAR_AND_WRITE);

#ifdef OS_WINDOWS
    std::string cmd = "dumpbin /NOLOGO /DISASM:BYTES ";
#else
    // -M intel for intel syntax
    std::string cmd = "objdump -M intel -d ";
#endif
    if (!objpath)
        cmd += "bin/garb.obj";
    else
        cmd += objpath;

    engone::StartProgram((char *)cmd.c_str(), PROGRAM_WAIT, nullptr, {}, file);

    engone::FileClose(file);
}
#endif