#pragma once

#include "basin/core/AST.h"
#include "basin/core/Bytecode.h"
#include "basin/core/PhaseContext.h"
#include "basin/core/DebugInformation.h"
#include "basin/util/Profiler.h"
#include "basin/core/VirtualMachine.h"

struct GlobalRunDirective;
struct CompilerImport;
struct GenContext : public PhaseContext {
    TinyBytecode* tinycode = nullptr;
    Bytecode* bytecode=nullptr;
    AST* ast=nullptr;
    Reporter* reporter=nullptr;
    CompilerImport* imp = nullptr;
    DynamicArray<TinyBytecode*>* out_codes = nullptr;
    
    int FRAME_SIZE = -1;
    int REGISTER_SIZE = -1;
    
    GenContext() : info(*this) { } // well this is dumb
    GenContext& info;

    int currentFrameOffset = 0;

    bool inside_compile_time_execution = false;
    bool inside_global = false;
    bool at_top_level = false;

    BytecodeBuilder builder{};

    void popInstructions(u32 count);

    engone::LinearAllocator scratch_allocator{};

    QuickArray<ASTNode*> nodeStack; // kind of like a stack trace
    
    DynamicArray<lexer::SourceLocation> source_trace{}; // printed when compiler bugs occur
    
    u32 lastLine = 0;
    u32 lastLocationIndex = (u32)-1;
    void pushNode(ASTNode* node);
    void popNode();

    void emit_abstract_dataptr(BCRegister reg, int offset, IdentifierVariable* global_ident);

    void generate_ext_dataptr(BCRegister reg, IdentifierVariable* varinfo);
    // void generate_dataptr(BCRegister reg, int offset);

    void addExternalRelocation(const std::string& name, int lib_index, u32 codeAddress, ExternalRelocationType rel_type, FunctionSignature* signature) {
        if(!disableCodeGeneration)
            bytecode->addExternalRelocation(name, lib_index, tinycode->index, codeAddress, rel_type, signature);
    }
    QuickArray<u32> indexOfNonImmediates{}; // this list is probably inefficient but other solutions are tedious.

    DebugFunction* debugFunction = nullptr;
    ASTFunction* currentFunction=nullptr;
    FuncImpl* currentFuncImpl=nullptr;
    ScopeId currentScopeId = 0;
    ScopeId fromScopeId = 0; // AST_FROM_NAMESPACE

    int frame_offset_of_assigned_initializer_pointer = -1; // -1 means not allocated on stack, it is reset per function generation

    u32 currentPolyVersion=0;
    int currentScopeDepth = 0; // necessary for scoped variables in debug information

    // won't work with multiple threads
    // bool disableCodeGeneration = false; // used with @no-code
    bool ignoreErrors = false; // used with @no-code
    bool showErrors = true;

    bool gen_func_with_run_directives = false;

    int funcDepth=0;
    struct LoopScope {
        // Index of instruction where looping starts.h
        int continueAddress = 0;
        int stackMoment=0;
        QuickArray<int> resolveBreaks;
        Identifier counter = {}; // may not be used right now but the size of
        // loop scope collides with std::string when tracking allocations.
    };
    QuickArray<LoopScope*> loopScopes;
    
    LoopScope* pushLoop();
    LoopScope* getLoop(int index);
    bool popLoop();

    // DynamicArray<int> frame_size_fixes{};
    // int max_size_of_function_arguments = 0;
    // int sum_frame_size = 0;
    // void alloc_local(int size) {
    //     sum_frame_size += size;
    // }
    // void add_frame_fix(int index) {
    //     if(disableCodeGeneration) return;
    //     frame_size_fixes.add(index);
    // }
    void fix_frame_values(FuncImpl* funcImpl, TinyBytecode* tinycode) {
        int frame_size = funcImpl->get_frame_size();
        // for(auto index : frame_size_fixes) {
        //     builder.fix_local_imm(index, frame_size);
        // }
        Assert(tinycode->frame_size == 0); // if programmer set frame_size and we try to override it then we made a whoopsie
        tinycode->frame_size = frame_size;

        for(auto& block : tinycode->try_blocks) {
            block.frame_offset_before_try = -frame_size; // it should be -frame_size
        }
        // frame_size_fixes.clear();
    }

    struct ResolveCall {
        int bcIndex = 0;
        FuncImpl* funcImpl = nullptr;
    };
    DynamicArray<ResolveCall> callsToResolve;
    void addCallToResolve(int bcIndex, FuncImpl* funcImpl);

    SignalIO framePush(TypeId typeId, i32* outFrameOffset, bool genDefault);
    SignalIO generatePush(BCRegister baseReg, int offset, TypeId typeId);
    SignalIO generatePop(BCRegister baseReg, int offset, TypeId typeId);
    
    enum PushPopKind {
        GEN_PUSH_REG       = 0x10,
        GEN_PUSH_GET_PARAM = 0x11,
        GEN_PUSH_GET_VAL   = 0x12,
        GEN_POP_REG        = 0x20,
        GEN_POP_SET_ARG    = 0x21,
        GEN_POP_SET_RET    = 0x22,
    };
    SignalIO generatePushPop_base(PushPopKind kind, BCRegister baseReg, int offset, TypeId typeId, int arg_index = -1);

    SignalIO generatePush_get_param (int arg_index, int offset, TypeId typeId);
    SignalIO generatePop_set_arg    (int arg_index, int offset, TypeId typeId);
    SignalIO generatePush_get_val   (int arg_index, int offset, TypeId typeId);
    SignalIO generatePop_set_ret    (int arg_index, int offset, TypeId typeId);

    SignalIO generateArtificialPush(TypeId typeId);
    // Generate a push from pointer (baseReg) where a list of pushed values are stored. generatePush reads memory from a struct layout while this function "copies" pushed values from a pointer.
    SignalIO generatePushFromValues(BCRegister baseReg, int baseOffset, TypeId typeId, int* movingOffset = nullptr);
    SignalIO generatePushedLiterals(VirtualMachine* vm, TypeId type, char* stack, ASTExpression* expression, TypeInfo* structImpl = nullptr, int memberIndex = 0);
    void genMemzero(BCRegister ptr_reg, BCRegister size_reg, int size, int offset);
    void genMemcpy(BCRegister dst_reg, BCRegister src_reg, int size);
    
    SignalIO generateAssignedInitializing(BCRegister baseReg, int offset, TypeId typeId, ASTExpressionInitializer* expression, ScopeId scopeId = -1);
    
    SignalIO generateDefaultValue(BCRegister baseReg, int offset, TypeId typeId, lexer::SourceLocation* location = nullptr, bool zeroInitialize=true);
    SignalIO generateReference(ASTExpression* _expression, TypeId* outTypeId, ScopeId idScope = -1, bool* wasNonReference = nullptr);
    SignalIO generateFncall(ASTExpression* expression, QuickArray<TypeId>* outTypeIds, bool isOperator);
    SignalIO generateSpecialFncall(ASTExpressionCall* expression);
    SignalIO generateExpression(ASTExpression *expression, TypeId *outTypeIds, ScopeId idScope = -1);
    SignalIO generateExpression(ASTExpression *expression, QuickArray<TypeId> *outTypeIds, ScopeId idScope = -1);
    SignalIO generateFunction(ASTFunction* function, ASTStruct* astStruct = nullptr);
    SignalIO generateFunctions(ASTScope* body);
    SignalIO generateBody(ASTScope *body);
    SignalIO generateStatement(ASTStatement *statement);
    
    SignalIO generatePreload();
    SignalIO preparePreloadData();
    SignalIO resetPreload();
    SignalIO generateData();
    SignalIO generateGlobalData(); // runs after all functions have been generated, that way we know that applyRelocations won't fail because of missing tinycodes.
    SignalIO executeGlobalRunDirective(GlobalRunDirective* run_directive);
    
    bool performSafeCast(TypeId from, TypeId to, bool less_strict = false);

    void init_context(Compiler* compiler);
    
    void printVMFailedMessage(VirtualMachine& vm, lexer::SourceLocation location);
};
struct NodeScope {
    NodeScope(GenContext* info) : info(info) {}
    ~NodeScope() {
        info->popNode();
    }
    GenContext* info = nullptr;
};
// Bytecode* Generate(AST* ast, CompileInfo* compileInfo);
bool GenerateScope(ASTScope* scope, Compiler* compiler, CompilerImport* imp, DynamicArray<TinyBytecode*>* out_codes, bool is_initial_import, bool gen_func_with_run_directives);

LinkConvention DetermineLinkConvention(const std::string& lib_path);