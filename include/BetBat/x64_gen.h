#pragma once
#include "BetBat/Bytecode.h"

#include "BetBat/Util/StringBuilder.h"
#include "BetBat/CompilerEnums.h"

/*
Common mistakes

x64 program requires this:
    stack pointer end with the value it began with
    ret must exist at the end

-1073741819 as error code means memory access violation
you may have forgotten ret or messed up the stack pointer

using a macro or some other function when converting bytecode operands to
x64 operands

*/

enum X64Register : u8 {
    X64_REG_INVALID = 0,
    
    X64_REG_BEGIN,
    X64_REG_A = X64_REG_BEGIN, // DO NOT CHANGE THE ORDER OF THE REGISTERS! intel manual specifies this specific order!
    X64_REG_C,
    X64_REG_D,
    X64_REG_B,
    X64_REG_SP,
    X64_REG_BP,
    X64_REG_SI,
    X64_REG_DI,

    X64_REG_R8, 
    X64_REG_R9, 
    X64_REG_R10,
    X64_REG_R11,
    X64_REG_R12,
    X64_REG_R13,
    X64_REG_R14,
    X64_REG_R15,

    X64_REG_XMM0,
    X64_REG_XMM1,
    X64_REG_XMM2,
    X64_REG_XMM3,
    X64_REG_XMM4,
    X64_REG_XMM5,
    X64_REG_XMM6,
    X64_REG_XMM7,
    X64_REG_END,
};
#define IS_REG_XMM(R) (R >= X64_REG_XMM0 && R <= X64_REG_XMM7)
#define IS_REG_EXTENDED(R) (R >= X64_REG_R8 && R <= X64_REG_R15)
#define IS_REG_NORM(R) (R >= X64_REG_A && R <= X64_REG_DI)

#define CLAMP_XMM(R) (Assert(IS_REG_XMM(R)), (X64Register)(((R-1) & 7) + 1))
#define CLAMP_EXT_REG(R) (Assert(IS_REG_EXTENDED(R)||IS_REG_NORM(R)), (X64Register)(((R-1) & 7) + 1))

struct X64TinyProgram {
    u8* text=nullptr;
    u64 _allocationSize=0;
    u64 head=0;
};
struct X64Program {
    ~X64Program(){
        TRACK_ARRAY_FREE(globalData, u8, globalSize);
        // engone::Free(globalData, globalSize);
        dataRelocations.cleanup();
        namedUndefinedRelocations.cleanup();
        if(debugInformation) {
            DebugInformation::Destroy(debugInformation);
            debugInformation = nullptr;
        }
    }
    
    DynamicArray<X64TinyProgram*> tinyPrograms;
    X64TinyProgram* createProgram() {
        auto ptr = new X64TinyProgram();
        tinyPrograms.add(ptr);
        return ptr;
    }
    
    // u64 size() const { return head; }

    u8* globalData = nullptr;
    u64 globalSize = 0;

    struct DataRelocation {
        u32 dataOffset; // offset in data segment
        u32 textOffset; // where to modify        
    };
    // struct PtrDataRelocation {
    //     u32 referer_dataOffset;
    //     u32 target_dataOffset;
    // };
    struct NamedUndefinedRelocation {
        std::string name; // name of symbol
        u32 textOffset; // where to modify
    };
    struct ExportedSymbol {
        std::string name; // name of symbol
        u32 textOffset; // where to modify?
    };

    // DynamicArray<PtrDataRelocation> ptrDataRelocations;
    DynamicArray<DataRelocation> dataRelocations;
    DynamicArray<NamedUndefinedRelocation> namedUndefinedRelocations;
    DynamicArray<ExportedSymbol> exportedSymbols;

    DebugInformation* debugInformation = nullptr;

    void printHex(const char* path = nullptr);
    void printAsm(const char* path, const char* objpath = nullptr);

    static void Destroy(X64Program* program);
    static X64Program* Create();

    // void generateFromTinycode(Bytecode* code, TinyBytecode* tinycode);

    // static X64Program* ConvertFromBytecode(Bytecode* code);
private:
    
};

// This structure is WAY to large, how to minimize it?
struct OPNode {
    OPNode(u32 bc_index, InstructionType type) : bc_index(bc_index), opcode(type) {}
    u32 bc_index = 0;
    InstructionType opcode = BC_HALT;
    i64 imm = 0;
    BCRegister op0 = BC_REG_INVALID;
    BCRegister op1 = BC_REG_INVALID;
    BCRegister op2 = BC_REG_INVALID;
    
    // TODO: Union on these?
    InstructionControl control = CONTROL_NONE;
    InstructionCast cast = CAST_UINT_UINT;
    
    LinkConventions link = LinkConventions::NONE;
    CallConventions call = CallConventions::BETCALL;
    
    OPNode* in0 = nullptr;
    OPNode* in1 = nullptr;
    OPNode* in2 = nullptr;
};

struct X64Builder {
    X64Program* prog = nullptr;
    X64TinyProgram* tinyprog = nullptr;
    Bytecode* bytecode = nullptr;
    TinyBytecode* tinycode = nullptr;
    
    QuickArray<OPNode*> nodes;
    
    // QuickArray<u32> instruction_indices;
    
    OPNode* createNode(u32 bc_index, InstructionType opcode) {
        auto ptr = new OPNode(bc_index, opcode);
        return ptr;
    }
    
    // struct ValueLocation {
    //     enum Kind {
    //         NONE,
    //         IN_REGISTER,
    //         IN_STACK,
    //     };
    //     Kind kind = NONE;
    //     int reg; // which register
    //     int stack_offset; // where in stack
    // };
    // std::unordered_map<BCRegister, ValueLocation*> valueLocations;
    
    struct RegisterInfo {
        bool used = false;
        
        // OPNode* node = nullptr;
    };
    std::unordered_map<X64Register, RegisterInfo> registers;
    
    std::unordered_map<BCRegister, OPNode*> reg_values;
    QuickArray<OPNode*> stack_values;
    
    X64Register alloc_register(X64Register reg = X64_REG_INVALID, bool is_float = false);
    bool is_register_free(X64Register reg);
    void free_register(X64Register reg);
    
    int size() const { return tinyprog->head; }
    void ensure_bytes(int size) {
        if(tinyprog->head + size >= tinyprog->_allocationSize ){
            bool yes = _reserve(tinyprog->_allocationSize * 2 + 50 + size);
            Assert(yes);
        }
    }

    void init(X64Program* p) {
        prog = p;
    }

    void emit1(u8 byte);
    void emit2(u16 word);
    void emit3(u32 word);
    void emit4(u32 dword);
    void emit8(u64 dword);
    void emit_modrm(u8 mod, X64Register reg, X64Register rm);
    void emit_modrm_slash(u8 mod, u8 reg, X64Register rm);
    // RIP-relative addressing
    void emit_modrm_rip(X64Register reg, u32 disp32);
    void emit_modrm_sib(u8 mod, X64Register reg, u8 scale, u8 index, X64Register base_reg);
    void emit_modrm_sib_slash(u8 mod, u8 reg, u8 scale, u8 index, X64Register base_reg);

    void emit_bytes(u8* arr, u64 len);

    // These are here to prevent implicit casting
    // of arguments which causes mistakes.
    void emit1(i64 _);
    void emit2(i64 _);
    void emit3(i64 _);
    void emit4(i64 _);
    void emit8(i8 _);
    void emit_modrm_rip(u8, i64);
    
    void fix_relative_jump_imm8(u32 offset, u8 value);
    void set_imm32(u32 offset, u32 value);
    
    bool _reserve(u32 size);
    
    void emit_push(X64Register reg);
    void emit_pop(X64Register reg);
    void emit_add_imm32(X64Register reg, i32 imm32);
    void emit_sub_imm32(X64Register reg, i32 imm32);


    void generateFromTinycode(Bytecode* code, TinyBytecode* tinycode);

    void generateInstructions_slow();

    int get_node_depth(OPNode* n) {
        if(!n) return 0;
        
        int a = get_node_depth(n->in0);
        int b = get_node_depth(n->in1);

        if (a > b)
            return a + 1;
        return b + 1;
    }
    
    static const X64Register RESERVED_REG0 = X64_REG_DI;
    static const X64Register RESERVED_REG1 = X64_REG_SI;
    
private:
    // recursively
    void generateInstructions(int depth = 0, BCRegister find_reg = BC_REG_INVALID, int inst_index = 0, X64Register* out_reg = nullptr);
};

// X64Program* ConvertTox64(Bytecode* bytecode);
// The function will print the reformatted content if outBuffer is null
void ReformatDumpbinAsm(LinkerChoice linker, QuickArray<char>& inBuffer, QuickArray<char>* outBuffer, bool includeBytes);

struct Compiler;
void GenerateX64(Compiler* compiler, TinyBytecode* tinycode);