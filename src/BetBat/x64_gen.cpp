
/*
Hello!
    The x64 generator is based on SSA (static single-assignment).
    The bytecode is converted to a tree of nodes where registers and
    the value on the stack determines the relations between the nodes.
    This results in a couple of root nodes which are instructions with
    side effects such as "mov [rbp], eax" or "add rsp, 8".
    x64 registers are allocated and freed when traversing the nodes and
    generating x64 instructions. There will probably be a refactor in the future for
    further optimized code generation (reuse of values in registers).

When writing code in this file:
    The code for the generator is messy since this is a first proper
    implementation of a decent generator (x64 is also a bit scuffed,
    but with good documentation).

    I urge you to be in a smart and consistent mood when changing the code.
    It is easy to accidently emit the wrong instruction SO be sure to
    create GREAT test cases so that all edge cases are covered.

    Good luck!

Notes:
    I tried to think of a linear approach (instead of SSA) where you can perform
    optimizations (dead code elimination, efficient usage of registers) but I could
    not make it work. A bottom-up and then top-down approach with a tree is much
    easier.

    If an assert fires then it could be a bug in the generator.
    Perhaps a an artifical register was never set.

*/

#include "BetBat/x64_gen.h"
#include "BetBat/x64_defs.h"

#include "BetBat/Compiler.h"
#include "BetBat/CompilerEnums.h"

// DO NOT REMOVE NATIVE REGISTER FUNCTIONS JUST BECAUSE WE ONLY
// HAVE BC_REG_LOCALS. YES it's just one register BUT WHO KNOWS
// IF WE WILL HAVE MORE IN THE FUTURE!
X64Register ToNativeRegister(BCRegister reg) {
    if (reg == BC_REG_LOCALS)
        return X64_REG_BP;
    return X64_REG_INVALID;
}
bool IsNativeRegister(BCRegister reg) {
    return ToNativeRegister(reg) != X64_REG_INVALID;
}
bool IsNativeRegister(X64Register reg) {
    return reg == X64_REG_BP || reg == X64_REG_SP;
}

bool GenerateX64_finalize(Compiler *compiler) {
    Assert(compiler->program); // don't call this function if there is no program
    Assert(compiler->program->debugInformation); // we expect debug information?

    auto prog = compiler->program;
    auto bytecode = compiler->bytecode;

    prog->index_of_main = bytecode->index_of_main;

    // prog->debugInformation = bytecode->debugInformation;
    // bytecode->debugInformation = nullptr;

    if (bytecode->dataSegment.size() != 0) {
        prog->globalData = TRACK_ARRAY_ALLOC(u8, bytecode->dataSegment.size());
        // prog->globalData = (u8*)engone::Allocate(bytecode->dataSegment.used);
        Assert(prog->globalData);
        prog->globalSize = bytecode->dataSegment.size();
        memcpy(prog->globalData, bytecode->dataSegment.data(), prog->globalSize);

        // OutputAsHex("data.txt", (char*)prog->globalData, prog->globalSize);
    }

    for (int i = 0; i < compiler->bytecode->exportedFunctions.size(); i++) {
        auto &sym = compiler->bytecode->exportedFunctions[i];
        prog->addExportedSymbol(sym.name, sym.tinycode_index);
        // X64Program::ExportedSymbol tmp{};
        // tmp.name = sym.name;
        // tmp.textOffset = addressTranslation[sym.location];
        // prog->exportedSymbols.add(tmp);
    }

    prog->compute_libraries();
    return true;
}
void X64Program::compute_libraries() {
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

bool GenerateX64(Compiler *compiler, TinyBytecode *tinycode) {
    using namespace engone;
    TRACE_FUNC()
    
    ZoneScopedC(tracy::Color::SkyBlue1);

    _VLOG(log::out << log::BLUE << "x64 Converter:\n";)


    // make sure dependencies have been fixed first
    bool yes = tinycode->applyRelocations(compiler->bytecode);
    if (!yes) {
        log::out << "Incomplete call relocation\n";
        return false;
    }

    X64Builder builder{};
    builder.init(compiler->program);
    builder.compiler = compiler;
    builder.bytecode = compiler->bytecode;
    builder.tinycode = tinycode;
    builder.tinyprog = compiler->program->createProgram(tinycode->index);
    builder.current_tinyprog_index = tinycode->index;

    yes = builder.generateFromTinycode_v2(builder.bytecode, builder.tinycode);
    return yes;
}

void X64Builder::free_all_registers() { registers.clear(); }
X64Register X64Builder::alloc_register(int artifical, X64Register reg, bool is_float, bool is_signed) {
    using namespace engone;
// #define DO_LOG(X) X
#define DO_LOG(X)
    if (!is_float) {
        if (reg != X64_REG_INVALID) {
            auto pair = registers.find(reg);
            if (pair == registers.end()) {
                registers[reg] = {};
                registers[reg].used = true;
                registers[reg].artifical_reg = artifical;
                DO_LOG(log::out << "alloc " << reg << "\n";)
                return reg;
            } else if (!pair->second.used) {
                pair->second.used = true;
                pair->second.artifical_reg = artifical;
                DO_LOG(log::out << "alloc " << reg << "\n";)
                return reg;
            }
        } else {
            static const X64Register regs[]{
                    // X64_REG_A,
                    X64_REG_C, X64_REG_D, X64_REG_B,
                    // X64_REG_SI,
                    // X64_REG_DI,

                    X64_REG_R8, X64_REG_R9, X64_REG_R10, X64_REG_R11, X64_REG_R12,
                    // X64_REG_R13, // if you add these, then make sure to callee save
                    // them since they are non-volatile
                    // X64_REG_R14,
                    // X64_REG_R15,
            };
            for (int i = 0; i < sizeof(regs) / sizeof(*regs); i++) {
                X64Register reg = regs[i];
                auto pair = registers.find(reg);
                if (pair == registers.end()) {
                    registers[reg] = {};
                    registers[reg].used = true;
                    registers[reg].artifical_reg = artifical;
                    Assert(reg != RESERVED_REG0 && reg != RESERVED_REG1);
                    DO_LOG(log::out << "alloc " << reg << "\n";)
                    return reg;
                } else if (!pair->second.used) {
                    pair->second.used = true;
                    pair->second.artifical_reg = artifical;
                    Assert(reg != RESERVED_REG0 && reg != RESERVED_REG1);
                    DO_LOG(log::out << "alloc " << reg << "\n";)
                    return reg;
                }
            }
        }
    } else {
        if (reg != X64_REG_INVALID) {
            auto pair = registers.find(reg);
            if (pair == registers.end()) {
                registers[reg] = {};
                registers[reg].used = true;
                registers[reg].artifical_reg = artifical;
                DO_LOG(log::out << "alloc " << reg << "\n";)
                return reg;
            } else if (!pair->second.used) {
                pair->second.used = true;
                pair->second.artifical_reg = artifical;
                DO_LOG(log::out << "alloc " << reg << "\n";)
                return reg;
            }
        } else {
            static const X64Register regs[]{
                X64_REG_XMM0,
                X64_REG_XMM1,
                X64_REG_XMM2,
                X64_REG_XMM3,
            };
            for (int i = 0; i < sizeof(regs) / sizeof(*regs); i++) {
                X64Register reg = regs[i];
                auto pair = registers.find(reg);
                if (pair == registers.end()) {
                    registers[reg] = {};
                    registers[reg].used = true;
                    registers[reg].artifical_reg = artifical;
                    DO_LOG(log::out << "alloc " << reg << "\n";)
                    return reg;
                } else if (!pair->second.used) {
                    pair->second.used = true;
                    pair->second.artifical_reg = artifical;
                    DO_LOG(log::out << "alloc " << reg << "\n";)
                    return reg;
                }
            }
        }
    }
    // Assert(("out of registers",false));
    return X64_REG_INVALID;
}
bool X64Builder::is_register_free(X64Register reg) {
    auto pair = registers.find(reg);
    if (pair == registers.end()) {
        return true;
    }
    return !pair->second.used;
}
void X64Builder::free_register(X64Register reg) {
    auto pair = registers.find(reg);
    if (pair == registers.end()) {
        Assert(false);
    }
    pair->second.used = false;
    pair->second.artifical_reg = 0;
}
bool X64Builder::reserve(u32 newAllocationSize) {
    if (newAllocationSize == 0) {
        if (tinyprog->_allocationSize != 0) {
            TRACK_ARRAY_FREE(tinyprog->text, u8, tinyprog->_allocationSize);
            // engone::Free(text, _allocationSize);
        }
        tinyprog->text = nullptr;
        tinyprog->_allocationSize = 0;
        tinyprog->head = 0;
        return true;
    }
    if (!tinyprog->text) {
        // text = (u8*)engone::Allocate(newAllocationSize);
        tinyprog->text = TRACK_ARRAY_ALLOC(u8, newAllocationSize);
        Assert(tinyprog->text);
        // initialization of elements is done when adding them
        if (!tinyprog->text)
            return false;
        tinyprog->_allocationSize = newAllocationSize;
        return true;
    } else {
        u8 *newText = TRACK_ARRAY_REALLOC(
                tinyprog->text, u8, tinyprog->_allocationSize, newAllocationSize);
        // TRACK_DELS(u8, tinyprog->_allocationSize);
        // u8* newText = (u8*)engone::Reallocate(tinyprog->text,
        // tinyprog->_allocationSize, newAllocationSize); TRACK_ADDS(u8,
        // newAllocationSize);
        Assert(newText);
        if (!newText)
            return false;
        tinyprog->text = newText;
        tinyprog->_allocationSize = newAllocationSize;
        if (tinyprog->head > newAllocationSize) {
            tinyprog->head = newAllocationSize;
        }
        return true;
    }
    return false;
}

void X64Builder::emit1(u8 byte) {
    ensure_bytes(1);
    *(tinyprog->text + tinyprog->head) = byte;
    tinyprog->head++;
}
void X64Builder::emit2(u16 word) {
    ensure_bytes(2);

    auto ptr = (u8 *)&word;
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    tinyprog->head += 2;
}
void X64Builder::emit3(u32 word) {
    if (tinyprog->head > 0) {
        // cvtsi2ss instructions have rex smashed in between the opcode or something like that, this assert will catch it.
        // TODO: Just because the value of the previous byte is REXW doesn't mean it's meant to be. It could be an immediate or modrm byte that happens to be equal to REXW. We can't assert like this. We should still catch bugs where REXW was specified before cvtsi2ss instead of inside.
        // Assert(*(tinyprog->text + tinyprog->head - 1) != PREFIX_REXW);
    }
    Assert(0 == (word & 0xFF000000));
    ensure_bytes(3);

    auto ptr = (u8 *)&word;
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    tinyprog->head += 3;
}
void X64Builder::emit4(u32 dword) {
    // wish we could assert to find bugs but immediates may be zero and won't
    // work. we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(4);
    auto ptr = (u8 *)&dword;

    // deals with non-alignment
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    *(tinyprog->text + tinyprog->head + 3) = *(ptr + 3);
    tinyprog->head += 4;
}
void X64Builder::emit8(u64 qword) {
    // wish we could assert to find bugs but immediates may be zero and won't
    // work. we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(8);
    auto ptr = (u8 *)&qword;

    // deals with non-alignment
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    *(tinyprog->text + tinyprog->head + 3) = *(ptr + 3);
    *(tinyprog->text + tinyprog->head + 4) = *(ptr + 4);
    *(tinyprog->text + tinyprog->head + 5) = *(ptr + 5);
    *(tinyprog->text + tinyprog->head + 6) = *(ptr + 6);
    *(tinyprog->text + tinyprog->head + 7) = *(ptr + 7);
    tinyprog->head += 8;
}
void X64Builder::fix_jump_here_imm8(u32 offset) {
    *(u8 *)(tinyprog->text + offset) = (code_size() - (offset + 1));
}
void X64Builder::emit_jmp_imm8(u32 offset) {
    emit1((u8)(offset - (code_size() + 1)));
}
void X64Builder::set_imm32(u32 offset, u32 value) {
    *(u32 *)(tinyprog->text + offset) = value;
}
void X64Builder::set_imm8(u32 offset, u8 value) {
    *(u8 *)(tinyprog->text + offset) = value;
}
void X64Builder::emit_bytes(const u8 *arr, u64 len) {
    ensure_bytes(len);

    memcpy(tinyprog->text + tinyprog->head, arr, len);
    tinyprog->head += len;
}
void X64Builder::emit_modrm_slash(u8 mod, u8 reg, X64Register _rm) {
    if (_rm == X64_REG_SP && mod != MODE_REG) {
        // SP register is not allowed with standard modrm byte, we must use a SIB
        emit_modrm_sib_slash(mod, reg, SIB_SCALE_1, SIB_INDEX_NONE, _rm);
        return;
    }
    u8 rm = _rm - 1;
    Assert((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    // You probably made a mistake and used REG_BP thinking it works with just
    // ModRM byte.
    Assert(("Use addModRM_SIB instead", !(mod != 0b11 && rm == 0b100)));
    // X64_REG_SP isn't available with mod=0b00, see intel x64 manual about 32 bit
    // addressing for more details
    Assert(("Use addModRM_disp32 instead", !(mod == 0b00 && rm == 0b101)));
    // Assert(("Use addModRM_disp32 instead",(mod!=0b10)));
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
void X64Builder::emit_modrm(u8 mod, X64Register _reg, X64Register _rm) {
    u8 reg = _reg - 1;
    u8 rm = _rm - 1;
    if (_rm == X64_REG_SP && mod != MODE_REG) {
        // SP register is not allowed with standard modrm byte, we must use a SIB
        emit_modrm_sib(mod, _reg, SIB_SCALE_1, SIB_INDEX_NONE, _rm);
        return;
    }
    if (_rm == X64_REG_BP && mod == MODE_DEREF) {
        // BP register is not allowed with without a displacement.
        // But we can use a displacement of zero. HOWEVER!
        // bp with zero displacement points to the previous frame's base pointer
        // which we shouldn't overwrite so we SHOULD assert.
        if (!disable_modrm_asserts) {
            Assert(("mov reg, [bp] is a bug", false));
        }
        mod = MODE_DEREF_DISP8;
        emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
        emit1((u8)0);
        return;
    }
    Assert((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    // You probably made a mistake and used REG_BP thinking it works with just
    // ModRM byte.
    Assert(("Use addModRM_SIB instead", !(mod != 0b11 && rm == 0b100)));
    // X64_REG_BP isn't available with mod=0b00, You must use a displacement. see
    // intel x64 manual about 32 bit addressing for more details
    Assert(("Use addModRM_disp32 instead", !(mod == 0b00 && rm == 0b101)));
    // Assert(("Use addModRM_disp32 instead",(mod!=0b10)));
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
void X64Builder::emit_modrm_rip32(X64Register _reg, u32 disp32) {
    u8 reg = _reg - 1;
    u8 mod = 0b00;
    u8 rm = 0b101;
    Assert((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit4(disp32);
}
void X64Builder::emit_modrm_rip32_slash(u8 _reg, u32 disp32) {
    u8 reg = _reg;
    u8 mod = 0b00;
    u8 rm = 0b101;
    Assert((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit4(disp32);
}
void X64Builder::emit_modrm_sib(u8 mod, X64Register _reg, u8 scale, u8 index, X64Register _base) {
    u8 base = _base - 1;
    u8 reg = _reg - 1;
    //  register to register (mod = 0b11) doesn't work with SIB byte
    Assert(("Use addModRM instead", mod != 0b11));

    Assert(("Ignored meaning in SIB byte. Look at intel x64 manual and fix it.",
                    base != 0b101));

    u8 rm = 0b100;
    Assert((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    Assert((scale & ~3) == 0 && (index & ~7) == 0 && (base & ~7) == 0);

    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit1((u8)(base | (index << (u8)3) | (scale << (u8)6)));
}
void X64Builder::emit_movsx(X64Register reg, X64Register rm, InstructionControl control) {
    Assert(!IS_REG_XMM(reg) && !IS_REG_XMM(rm));
    Assert(!IS_CONTROL_FLOAT(control));
    Assert(!IS_CONTROL_UNSIGNED(
            control)); // why would we sign extend unsigned operand?

    emit_prefix(PREFIX_REXW, reg, rm);
    if (GET_CONTROL_SIZE(control) == CONTROL_8B) {
        emit2(OPCODE_2_MOVSX_REG_RM8);
    } else if (GET_CONTROL_SIZE(control) == CONTROL_16B) {
        emit2(OPCODE_2_MOVSX_REG_RM16);
    } else if (GET_CONTROL_SIZE(control) == CONTROL_32B) {
        emit1(OPCODE_MOVSXD_REG_RM);
    } else {
        emit1(OPCODE_MOV_REG_RM);
    }
    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
}
void X64Builder::emit_operation(u8 opcode, X64Register reg, X64Register rm, InstructionControl control) {
    int size = 1 << GET_CONTROL_SIZE(control);
    if (size == 1) {
        emit_prefix(0, reg, rm);
        // 8-bit instructions are usually one less than the regular instruction
        // assuming this is perhaps not a great idea
        u8 opcode_8bit = opcode - 1;
        emit1(opcode_8bit);
    } else if (size == 2) {
        emit1(PREFIX_16BIT);
        emit_prefix(0, reg, rm);
        emit1(opcode);
    } else if (size == 4) {
        emit_prefix(0, reg, rm);
        emit1(opcode);
    } else if (size == 8) {
        emit_prefix(PREFIX_REXW, reg, rm);
        emit1(opcode);
    } else
        Assert(false);
    // Invalid registers, could indicate a bug in the code generator
    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
}
void X64Builder::emit_movzx(X64Register reg, X64Register rm, InstructionControl control) {
    Assert(!IS_REG_XMM(reg) && !IS_REG_XMM(rm));
    Assert(!IS_CONTROL_FLOAT(control));
    Assert(
            !IS_CONTROL_SIGNED(control)); // It's not good to zero extend signed value

    if (GET_CONTROL_SIZE(control) == CONTROL_64B && reg == rm)
        return; // do nothing

    if (GET_CONTROL_SIZE(control) == CONTROL_8B) {
        emit_prefix(PREFIX_REXW, reg, rm);
        emit2(OPCODE_2_MOVZX_REG_RM8);
    } else if (GET_CONTROL_SIZE(control) == CONTROL_16B) {
        emit_prefix(PREFIX_REXW, reg, rm);
        emit2(OPCODE_2_MOVZX_REG_RM16);
    } else if (GET_CONTROL_SIZE(control) == CONTROL_32B) {
        emit_prefix(0, reg, rm);
        emit1(OPCODE_MOV_REG_RM);
    } else if (GET_CONTROL_SIZE(control) == CONTROL_64B) {
        emit_prefix(PREFIX_REXW, reg, rm);
        emit1(OPCODE_MOV_REG_RM);
    }
    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
}
void X64Builder::emit_modrm_sib_slash(u8 mod, u8 reg, u8 scale, u8 index, X64Register _base) {
    u8 base = _base - 1;
    //  register to register (mod = 0b11) doesn't work with SIB byte
    Assert(("Use addModRM instead", mod != 0b11));

    Assert(("Ignored meaning in SIB byte. Look at intel x64 manual and fix it.",
                    base != 0b101));

    u8 rm = 0b100;
    Assert((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    Assert((scale & ~3) == 0 && (index & ~7) == 0 && (base & ~7) == 0);

    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit1((u8)(base | (index << (u8)3) | (scale << (u8)6)));
}
#ifdef gone
void X64Program::printHex(const char *path) {
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

/* dumpbin
Dump of file bin\dev.obj

File Type: COFF OBJECT

main:
    0000000000000000: FF F5              push        rbp
    0000000000000002: 48 8B EC           mov         rbp,rsp
    0000000000000005: 48 81 EC 08 00 00  sub         rsp,8
                                        00
    000000000000000C: 33 C0              xor         eax,eax
    000000000000000E: FF F0              push        rax
    0000000000000010: 8F C0              pop         rax
    0000000000000012: 89 45 F8           mov         dword ptr [rbp-8],eax
    0000000000000015: 48 81 EC 08 00 00  sub         rsp,8
                                        00
    000000000000001C: 48 33 C0           xor         rax,rax
    000000000000001F: C7 C0 41 00 00 00  mov         eax,41h
    0000000000000025: FF F0              push        rax
    0000000000000027: 48 33 C0           xor         rax,rax
    000000000000002A: 8F C0              pop         rax
    000000000000002C: 88 45 F0           mov         byte ptr [rbp-10h],al
    000000000000002F: 48 81 C4 10 00 00  add         rsp,10h
                                        00
    0000000000000036: 8F C5              pop         rbp
    0000000000000038: C3                 ret

    Summary

                    12 .debug_abbrev
                    4B .debug_info
                    31 .debug_line
                    39 .text
*/
/* objdump
wa.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_Z3heyii>:
     0:   f3 0f 1e fa             endbr64
     4:   55                      push   %rbp
     5:   48 89 e5                mov    %rsp,%rbp
     8:   89 7d fc                mov    %edi,-0x4(%rbp)
     b:   89 75 f8                mov    %esi,-0x8(%rbp)
     e:   8b 55 fc                mov    -0x4(%rbp),%edx
    11:   8b 45 f8                mov    -0x8(%rbp),%eax
    14:   01 d0                   add    %edx,%eax
    16:   5d                      pop    %rbp
    17:   c3                      ret

0000000000000018 <main>:
    18:   f3 0f 1e fa             endbr64
    1c:   55                      push   %rbp
    1d:   48 89 e5                mov    %rsp,%rbp
    20:   be 05 00 00 00          mov    $0x5,%esi
    25:   bf 02 00 00 00          mov    $0x2,%edi
    2a:   e8 00 00 00 00          call   2f <main+0x17>
    2f:   b8 00 00 00 00          mov    $0x0,%eax
    34:   5d                      pop    %rbp
    35:   c3                      ret
*/
void X64Program::Destroy(X64Program *program) {
    using namespace engone;
    Assert(program);
    program->~X64Program();
    TRACK_FREE(program, X64Program);
    // engone::Free(program,sizeof(X64Program));
}
X64Program *X64Program::Create() {
    using namespace engone;

    // auto program = (X64Program*)engone::Allocate(sizeof(X64Program));
    auto program = TRACK_ALLOC(X64Program);
    new (program) X64Program();
    return program;
}

void X64Builder::emit_push(X64Register reg, int size) {
    if (IS_REG_XMM(reg)) {
        emit_sub_imm32(X64_REG_SP, 8);
        if (size == 4)
            emit3(OPCODE_3_MOVSS_RM_REG);
        else if (size == 8)
            emit3(OPCODE_3_MOVSD_RM_REG);
        else
            Assert(false);
        emit_modrm(MODE_DEREF, CLAMP_XMM(reg), X64_REG_SP);
    } else {
        emit_prefix(0, X64_REG_INVALID, reg);
        emit1(OPCODE_PUSH_RM_SLASH_6);
        emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(reg));
    }
    if (push_offsets.size())
        push_offsets.last() += 8;
    ret_offset += 8;
}
void X64Builder::emit_pop(X64Register reg, int size) {
    if (IS_REG_XMM(reg)) {
        if (size == 4)
            emit3(OPCODE_3_MOVSS_REG_RM);
        else if (size == 8)
            emit3(OPCODE_3_MOVSD_REG_RM);
        else
            Assert(false);
        emit_modrm(MODE_DEREF, CLAMP_XMM(reg), X64_REG_SP);
        emit_add_imm32(X64_REG_SP, 8);
    } else {
        emit_prefix(0, X64_REG_INVALID, reg);
        emit1(OPCODE_POP_RM_SLASH_0);
        emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg));
    }
    if (push_offsets.size())
        push_offsets.last() -= 8;
    ret_offset -= 8;
}
void X64Builder::emit_add_imm32(X64Register reg, i32 imm32) {
    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
    emit1(OPCODE_ADD_RM_IMM_SLASH_0);
    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg));
    emit4((u32)imm32); // NOTE: cast from i16 to i32 to u32, should be fine
}
void X64Builder::emit_sub_imm32(X64Register reg, i32 imm32) {
    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
    emit1(OPCODE_SUB_RM_IMM_SLASH_5);
    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(reg));
    emit4((u32)imm32);
}
void X64Builder::emit_prefix(u8 inherited_prefix, X64Register reg, X64Register rm) {
    inherited_prefix = construct_prefix(inherited_prefix, reg, rm);
    if (inherited_prefix)
        emit1(inherited_prefix);
}
u8 X64Builder::construct_prefix(u8 inherited_prefix, X64Register reg, X64Register rm) {
    if (reg != X64_REG_INVALID) {
        if (IS_REG_EXTENDED(reg)) {
            inherited_prefix |= PREFIX_REXR;
        } else if (IS_REG_NORM(reg)) {

        } else
            Assert(false); // float register not allowed like this
    }
    if (rm != X64_REG_INVALID) {
        if (IS_REG_EXTENDED(rm)) {
            inherited_prefix |= PREFIX_REXB;
        } else if (IS_REG_NORM(rm)) {

        } else
            Assert(false); // float register not allowed like this
    }
    return inherited_prefix;
}
void X64Builder::emit_mov_reg_mem(X64Register reg, X64Register rm, InstructionControl control, int disp) {
    int size = GET_CONTROL_SIZE(control);
    // if (IS_CONTROL_FLOAT(control)) {
    if (IS_REG_XMM(reg)) {
        emit_prefix(0, X64_REG_INVALID, rm);
        if (size == CONTROL_32B)
            emit3(OPCODE_3_MOVSS_REG_RM);
        else if (size == CONTROL_64B)
            emit3(OPCODE_3_MOVSD_REG_RM);
        else
            Assert(false);

        u8 mode = MODE_DEREF_DISP32;
        if (disp == 0) {
            mode = MODE_DEREF;
        } else if (disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
        }
            emit_modrm(mode, CLAMP_XMM(reg), CLAMP_EXT_REG(rm));
        if (mode == MODE_DEREF) {

        } else if (mode == MODE_DEREF_DISP8)
            emit1((u8)(i8)disp);
        else
            emit4((u32)(i32)disp);
    } else {
        Assert(!IS_CONTROL_FLOAT(control));
        if (size == CONTROL_16B) {
            emit1(PREFIX_16BIT);
        }
        if (size == CONTROL_64B || size == CONTROL_8B) { // REX is needed with MOV_REG8_RM8, we want to
            // access lower bits of DI, SI, not dh, bh
            emit_prefix(PREFIX_REXW, reg, rm);
        } else {
            emit_prefix(0, reg, rm);
        }
        if (size == CONTROL_8B) {
            emit1(OPCODE_MOV_REG8_RM8);
        } else {
            emit1(OPCODE_MOV_REG_RM);
        }

        u8 mode = MODE_DEREF_DISP32;
        if (disp == 0) {
            mode = MODE_DEREF;
        } else if (disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
        }
        emit_modrm(mode, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
        if (mode == MODE_DEREF) {

        } else if (mode == MODE_DEREF_DISP8)
            emit1((u8)(i8)disp);
        else
            emit4((u32)(i32)disp);
    }
}
void X64Builder::emit_mov_mem_reg(X64Register rm, X64Register reg, InstructionControl control, int disp) {
    int size = GET_CONTROL_SIZE(control);
    // if (IS_CONTROL_FLOAT(control)) {
    if (IS_REG_XMM(reg)) {
            emit_prefix(0, X64_REG_INVALID, rm);
            if (size == CONTROL_32B)
                emit3(OPCODE_3_MOVSS_RM_REG);
            else if (size == CONTROL_64B)
                emit3(OPCODE_3_MOVSD_RM_REG);
            else
                Assert(false);

    u8 mode = MODE_DEREF_DISP32;
    if (disp == 0) {
            mode = MODE_DEREF;
    } else if (disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
    }
    emit_modrm(mode, CLAMP_XMM(reg), CLAMP_EXT_REG(rm));
    if (mode == MODE_DEREF) {

    } else if (mode == MODE_DEREF_DISP8)
        emit1((u8)(i8)disp);
    else
        emit4((u32)(i32)disp);
    } else {
        Assert(!IS_CONTROL_FLOAT(control));
        if (size == CONTROL_16B) {
            emit1(PREFIX_16BIT);
        }
        if (size == CONTROL_64B) {
            emit_prefix(PREFIX_REXW, reg, rm);
        } else {
            emit_prefix(0, reg, rm);
        }
        if (size == CONTROL_8B) {
            emit1(OPCODE_MOV_RM_REG8);
        } else {
            emit1(OPCODE_MOV_RM_REG);
        }

        u8 mode = MODE_DEREF_DISP32;
        if (disp == 0) {
            mode = MODE_DEREF;
        } else if (disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
        }
        emit_modrm(mode, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
        if (mode == MODE_DEREF) {

        } else if (mode == MODE_DEREF_DISP8)
            emit1((u8)(i8)disp);
        else
            emit4((u32)(i32)disp);
    }
}
void X64Builder::emit_mov_reg_reg(X64Register reg, X64Register rm, int size) {
    if (IS_REG_XMM(reg) && IS_REG_XMM(rm)) {
        if (size == 4)
            emit3(OPCODE_3_MOVSS_REG_RM);
        else if (size == 8)
            emit3(OPCODE_3_MOVSD_REG_RM);
        else
            Assert(false);
        emit_modrm(MODE_REG, CLAMP_XMM(reg), CLAMP_XMM(rm));
    } else if (IS_REG_XMM(reg) && !IS_REG_XMM(rm)) {
        Assert(false); // did you mean do move xmm to general?
        emit_prefix(PREFIX_REXW, rm, X64_REG_SP);
        emit1(OPCODE_MOV_RM_REG);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(rm), X64_REG_SP);
        emit1((u8)-8);

        emit3(OPCODE_3_MOVSD_REG_RM);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(reg), X64_REG_SP);
        emit1((u8)-8);
    } else if (!IS_REG_XMM(reg) && IS_REG_XMM(rm)) {
        // this code can be called with BC_TEST_VALUE where a register is xmm
        if (size == 4)
            emit3(OPCODE_3_MOVSS_RM_REG);
        else if (size == 8)
            emit3(OPCODE_3_MOVSD_RM_REG);
        else
            Assert(false);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(rm), X64_REG_SP);
        emit1((u8)-8);

        // TODO: Don't always move 64-bit register
        emit_prefix(PREFIX_REXW, reg, X64_REG_SP);
        emit1(OPCODE_MOV_REG_RM);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(reg), X64_REG_SP);
        emit1((u8)-8);

        InstructionControl control = CONTROL_8B;
        if (size == 1)
            control = CONTROL_8B;
        else if (size == 2)
            control = CONTROL_16B;
        else if (size == 4)
            control = CONTROL_32B;
        else if (size == 8)
            control = CONTROL_64B;
        emit_movzx(reg, reg, control);
    } else if (!IS_REG_XMM(reg) && !IS_REG_XMM(rm)) {
        emit_prefix(PREFIX_REXW, reg, rm);
        emit1(OPCODE_MOV_REG_RM);
        emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
    }
}

const char *x64_register_names[]{
        "INVALID", // X64_REG_INVALID = 0,
        "A",       // X64_REG_A,
        "C",       // X64_REG_C,
        "D",       // X64_REG_D,
        "B",       // X64_REG_B,
        "SP",      // X64_REG_SP,
        "BP",      // X64_REG_BP,
        "SI",      // X64_REG_SI,
        "DI",      // X64_REG_DI,
        "R8",      // X64_REG_R8,
        "R9",      // X64_REG_R9,
        "R10",     // X64_REG_R10,
        "R11",     // X64_REG_R11,
        "R12",     // X64_REG_R12,
        "R13",     // X64_REG_R13,
        "R14",     // X64_REG_R14,
        "R15",     // X64_REG_R15,
        "XMM0",    // X64_REG_XMM0,
        "XMM1",    // X64_REG_XMM1,
        "XMM2",    // X64_REG_XMM2,
        "XMM3",    // X64_REG_XMM3,
        "XMM4",    // X64_REG_XMM4,
        "XMM5",    // X64_REG_XMM5,
        "XMM6",    // X64_REG_XMM6,
        "XMM7",    // X64_REG_XMM7,
};
engone::Logger &operator<<(engone::Logger &l, X64Register r) {
    l << x64_register_names[r];
    return l;
}