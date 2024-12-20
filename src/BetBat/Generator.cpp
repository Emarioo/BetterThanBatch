#include "BetBat/Generator.h"
#include "BetBat/Compiler.h"

// include Lang.h here so that we don't
// have to recompile everything if we
// make a change in it.
#include "BetBat/Lang.h"

#undef ERRTYPE
#undef ERRTYPE1
#define ERRTYPE1(R, LT, RT, M) ERR_SECTION(ERR_HEAD2(R, ERROR_TYPE_MISMATCH) ERR_MSG_LOG("Types do not match! This was found '" << engone::log::GREEN<< info.ast->typeToString(LT) <<engone::log::NO_COLOR<< "' while this was expected '" << engone::log::GREEN<<info.ast->typeToString(RT) << engone::log::NO_COLOR<< "' " << M << "\n\n") ERR_LINE2(R,"expects "+info.ast->typeToString(RT)))

// #define ERRTYPE(L, R, LT, RT, M) ERR_SECTION(ERR_HEAD2(L, ERROR_TYPE_MISMATCH) ERR_MSG("Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " " << M) ERR_LINE2(L,info.ast->typeToString(LT)) ERR_LINE2(R,info.ast->typeToString(RT)))
#define ERRTYPE(L, R, LT, RT, M) ERR_SECTION(ERR_HEAD2(L, ERROR_TYPE_MISMATCH) ERR_MSG("Type mismatch " << ast->typeToString(LT) << " - " << ast->typeToString(RT) << " " << M) ERR_LINE2(L,ast->typeToString(LT)) ERR_LINE2(R,ast->typeToString(RT)))

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION("Generator, ", CONTENT)

#undef LOGAT
#define LOGAT(R) R.firstToken.line << ":" << R.firstToken.column

#define MAKE_NODE_SCOPE(X) info.pushNode(dynamic_cast<ASTNode*>(X));NodeScope nodeScope{&info};

#undef TEMP_ARRAY
// #define TEMP_ARRAY(TYPE,NAME) QuickArray<TYPE> NAME;
// #define TEMP_ARRAY_N(TYPE,NAME,N) QuickArray<TYPE> NAME;
#define TEMP_ARRAY(TYPE,NAME) QuickArray<TYPE> NAME; NAME.init(&scratch_allocator);
#define TEMP_ARRAY_N(TYPE,NAME, N) QuickArray<TYPE> NAME; NAME.init(&scratch_allocator); NAME.reserve(N);


#define SOURCE_TRACE(LOC) source_trace.add(LOC); defer { source_trace.pop(); };

/* #region  */
GenContext::LoopScope* GenContext::pushLoop(){
    LoopScope* ptr = TRACK_ALLOC(LoopScope);
    new(ptr) LoopScope();
    if(!ptr)
        return nullptr;
    loopScopes.add(ptr);
    return ptr;
}
GenContext::LoopScope* GenContext::getLoop(int index){
    if(index < 0 || index >= (int)loopScopes.size()) return nullptr;
    return loopScopes[index];
}
LinkConvention DetermineLinkConvention(const std::string& lib_path) {
    using namespace engone;
    if (lib_path.size() == 0)
        return LinkConvention::IMPORT;

    // Examples:
    //   file.so
    //   file.so.1
    //   .file.so.2.5
    //   .file3.so
    //   file.mp3
    //   ok-1.2/file.so
    //   ok-1.2/.file3.so.2.3

    // Find the file extension, on Windows we can just lib_path.find_last_of(".") but on Linux there may be a version after the extension which we must skip first.
    bool only_numbers = true;
    int ext_end = lib_path.size();
    int ext_beg = lib_path.size();
    for(int i=lib_path.size()-1;i>=0;i--) {
        char chr = lib_path[i];
        if(only_numbers && ((chr >= '0' && chr <= '9') || chr == '.')) {
            
        } else {
            only_numbers = false;
        }
        if(chr == '/')
            break;
        if(chr == '.') {
            if(only_numbers) {
                ext_beg = i + 1;
                ext_end = i;
            } else {
                ext_beg = i + 1;
                break;
            }
        }
    }
    
    std::string format = "";
    if(ext_end > ext_beg)
        format = lib_path.substr(ext_beg, ext_end-ext_beg);
    else
        // assume static import, user may have linked with "c" or "m" which is math and C library.
        return LinkConvention::STATIC_IMPORT;
    // log::out << format << "\n";
        
    if (format == "dll" || format == "so") {
        return LinkConvention::DYNAMIC_IMPORT;
    } else if(format == "lib" || format == "a") { // TODO: Linux uses libNAME.a for static libraries, we only check .a here but we should perhaps check the prefix 'lib' too.
        return LinkConvention::STATIC_IMPORT;
    }
    return LinkConvention::IMPORT;
}
bool GenContext::popLoop(){
    if(loopScopes.size()==0)
        return false;
    LoopScope* ptr = loopScopes.last();
    ptr->~LoopScope();
    // engone::Free(ptr, sizeof(LoopScope));
    TRACK_FREE(ptr, LoopScope);
    loopScopes.pop();
    return true;
}
void GenContext::pushNode(ASTNode* node){
    nodeStack.add(node);
}
void GenContext::popNode(){
    nodeStack.pop();
}
void GenContext::addCallToResolve(i32 bcIndex, FuncImpl* funcImpl){
    if(disableCodeGeneration) return;
    // if(bcIndex == 97) {
    //     __debugbreak();
    // }
    // ResolveCall tmp{};
    // tmp.bcIndex = bcIndex;
    // tmp.funcImpl = funcImpl;
    // callsToResolve.add(tmp);

    tinycode->addRelocation(bcIndex, funcImpl);
}
// Will perform cast on float and integers with pop, cast, push
// uses register A
// TODO: handle struct cast?
// NOTE: performSafeCast() should allow the same casts as ast->castable()
bool GenContext::performSafeCast(TypeId from, TypeId to, bool less_strict) {
    if(!from.isValid() || !to.isValid())
        return false;
    if (from == to)
        return true;
    
    #define CAST_TO_BOOL builder.emit_pop(BC_REG_T0);            \
            builder.emit_li32(BC_REG_A, 0);                      \
            builder.emit_neq(BC_REG_T0, BC_REG_A, from_size, false);     \
            builder.emit_push(BC_REG_T0);                         

    auto from_size = ast->getTypeSize(from);

    if(from.isPointer()) {
        if (to == TYPE_BOOL) {
            CAST_TO_BOOL
            return true;
        }
        if ((to.baseType() == TYPE_UINT64 || to.baseType() == TYPE_INT64) && 
            from.getPointerLevel() - to.getPointerLevel() == 1 && (less_strict || from.baseType() == TYPE_VOID))
            return true;
        // if(to.baseType() == TYPE_VOID && from.getPointerLevel() == to.getPointerLevel())
        //     return true;
        if(to.baseType() == TYPE_VOID && to.getPointerLevel() > 0)
            return true;
    }
    if(to.isPointer()) {
        if ((from.baseType() == TYPE_UINT64 || from.baseType() == TYPE_INT64) && 
            to.getPointerLevel() - from.getPointerLevel() == 1 && (less_strict || to.baseType() == TYPE_VOID))
            return true;
        // if(from.baseType() == TYPE_VOID && from.getPointerLevel() == to.getPointerLevel())
        //     return true;
        if(from.baseType() == TYPE_VOID && from.getPointerLevel() > 0)
            return true;
    }

    // TODO: I just threw in currentScopeId. Not sure if it is supposed to be in all cases.
    // auto fti = info.ast->getTypeInfo(from);
    // auto tti = info.ast->getTypeInfo(to);
    auto to_size = ast->getTypeSize(to);
    BCRegister from_reg = BC_REG_T0;
    BCRegister to_reg = BC_REG_T1;
    if (AST::IsDecimal(from) && AST::IsInteger(to)) {
        builder.emit_pop(from_reg);
        // if(AST::IsSigned(to))
        builder.emit_cast(to_reg, from_reg, CAST_FLOAT_SINT, from_size, to_size);
            // builder.emit_({BC_CAST, CAST_FLOAT_UINT, reg0, reg1});
        builder.emit_push(to_reg);
        return true;
    }
    if (AST::IsInteger(from) && AST::IsDecimal(to)) {
        builder.emit_pop(from_reg);
        if(AST::IsSigned(from))
            builder.emit_cast(to_reg,from_reg, CAST_SINT_FLOAT, from_size, to_size);
        else
            builder.emit_cast(to_reg,from_reg, CAST_UINT_FLOAT, from_size, to_size);
        builder.emit_push(to_reg);
        return true;
    }
    if (AST::IsDecimal(from) && AST::IsDecimal(to)) {
        builder.emit_pop(from_reg);
        builder.emit_cast(to_reg,from_reg, CAST_FLOAT_FLOAT, from_size, to_size);
        builder.emit_push(to_reg);
        return true;
    }
    if ((AST::IsInteger(from) && to == TYPE_CHAR) ||
        (from == TYPE_CHAR && AST::IsInteger(to))) {
        // if(fti->size() != tti->size()){
        builder.emit_pop(from_reg);
        builder.emit_cast(to_reg,from_reg, CAST_SINT_SINT, from_size, to_size);
        builder.emit_push(to_reg);
        // }
        return true;
    }
    if ((AST::IsInteger(from) && to == TYPE_BOOL) ||
        (from == TYPE_BOOL && AST::IsInteger(to))) {
        // if(fti->size() != tti->size()){
        builder.emit_pop(from_reg);
        builder.emit_cast(to_reg,from_reg, CAST_SINT_SINT, from_size, to_size);
        builder.emit_push(to_reg);
        // }
        return true;
    }
    if(to == TYPE_BOOL) {
        auto finfo = info.ast->getTypeInfo(from);
        if(finfo->astEnum) {
            CAST_TO_BOOL
            return true;
        }
    }
    if (AST::IsInteger(from) && AST::IsInteger(to)) {
        builder.emit_pop(from_reg);
        if (AST::IsSigned(from) && AST::IsSigned(to))
            builder.emit_cast(to_reg,from_reg, CAST_SINT_SINT, from_size, to_size);
        if (AST::IsSigned(from) && !AST::IsSigned(to))
            builder.emit_cast(to_reg, from_reg,CAST_SINT_UINT, from_size, to_size);
        if (!AST::IsSigned(from) && AST::IsSigned(to))
            builder.emit_cast(to_reg, from_reg,CAST_UINT_SINT, from_size, to_size);
        if (!AST::IsSigned(from) && !AST::IsSigned(to))
            builder.emit_cast(to_reg, from_reg,CAST_SINT_UINT, from_size, to_size);
        builder.emit_push(to_reg);
        return true;
    }
    TypeInfo* from_typeInfo = nullptr;
    if(from.isNormalType())
        from_typeInfo = ast->getTypeInfo(from);
    TypeInfo* to_typeInfo = nullptr;
    if(to.isNormalType())
        to_typeInfo = ast->getTypeInfo(to);
    // int to_size = info.ast->getTypeSize(to);
    if(from_typeInfo && from_typeInfo->astEnum && AST::IsInteger(to)) {
        // TODO: Print an error that says big enums can't be casted to small integers.
        if(to_size >= from_typeInfo->_size) {
            // builder.emit_pop(reg);
            // builder.emit_push(reg);
            return true;
        }
    }
    if (from_typeInfo && from_typeInfo->funcType && to == TYPE_BOOL) {
        CAST_TO_BOOL
        return true;
    }
    auto voidp = TypeId::Create(TYPE_VOID, 1);
    if (from == voidp && to_typeInfo && to_typeInfo->funcType) {
        return true;
    }
    if (from_typeInfo && from_typeInfo->funcType && to == voidp) {
        return true;
    }
    // Asserts when we have missed a conversion
    if(!hasForeignErrors()){
        std::string l = ast->typeToString(from);
        std::string r = ast->typeToString(to);
        Assert(!ast->castable(from,to, less_strict));
    }
    return false;
}

InstructionOpcode ASTOpToBytecode(OperationType astOp, bool floatVersion){
    
// #define CASE(X, Y) case X: return Y;
#define CASE(X, Y) else if(op == X) return Y;
    OperationType op = astOp;
    if(false) ;
    CASE(AST_ADD, BC_ADD)
    CASE(AST_SUB, BC_SUB)
    CASE(AST_MUL, BC_MUL)
    CASE(AST_DIV, BC_DIV) 
    CASE(AST_MODULO, BC_MOD)
    CASE(AST_EQUAL, BC_EQ)
    CASE(AST_NOT_EQUAL, BC_NEQ) 
    CASE(AST_LESS, BC_LT) 
    CASE(AST_LESS_EQUAL, BC_LTE) 
    CASE(AST_GREATER, BC_GT) 
    CASE(AST_GREATER_EQUAL, BC_GTE) 
    CASE(AST_AND, BC_LAND)
    CASE(AST_OR, BC_LOR)
    CASE(AST_AND, BC_LNOT)
    CASE(AST_BAND, BC_BAND) 
    CASE(AST_BOR, BC_BOR) 
    CASE(AST_BXOR, BC_BXOR) 
    CASE(AST_BNOT, BC_BNOT) 
    CASE(AST_BLSHIFT, BC_BLSHIFT) 
    CASE(AST_BRSHIFT, BC_BRSHIFT) 
#undef CASE
    return (InstructionOpcode)0;
}
void GenContext::emit_abstract_dataptr(BCRegister reg, int offset, IdentifierVariable* global_ident) {
    Assert(global_ident->declaration);
    if(compiler->options->stable_global_data && !global_ident->declaration->is_notstable) {
                
        auto iden = ast->findIdentifier(currentScopeId, CONTENT_ORDER_MAX, "stable_global_memory", nullptr);
        Assert(iden);
        auto varinfo = iden->cast_var();
        auto version = currentPolyVersion; // globals do not share polyVersion with current scope, they can come from a different scope
            if(varinfo->is_import_global)
                version = 0;
        
        BCRegister reg_tmp = BC_REG_T1;

        Assert(offset != varinfo->versions_dataOffset[version]); // we shouldn't redirect stable_global_memory to heap allocated memory, stable memory pointer is always accessed and set in global data

        // get pointer to real global data
        builder.emit_dataptr(reg, varinfo->versions_dataOffset[version]);
        builder.emit_mov_rm(reg, reg, REGISTER_SIZE);
        // calcualte the offset to data object we want
        builder.emit_li32(reg_tmp, offset);
        builder.emit_add(reg, reg_tmp, REGISTER_SIZE, false);
    } else {
        builder.emit_dataptr(reg, offset);
    }
}
void GenContext::generate_ext_dataptr(BCRegister reg, IdentifierVariable* varinfo) {
    using namespace engone;

    std::string alias = varinfo->declaration->linked_alias.size() == 0 ? varinfo->declaration->varnames[0].name : varinfo->declaration->linked_alias;
    std::string lib_path = "";
    auto imp = info.compiler->lexer.getImport_unsafe(varinfo->declaration->location);
    
    auto func_imp = info.compiler->getImport(imp->file_id);
    Assert(func_imp);

    for(auto& lib : func_imp->libraries) {
        if(varinfo->declaration->linked_library == lib.named_as) {
            lib_path = lib.path;
            break;
        }
    }
    LinkConvention link_convention = varinfo->declaration->linkConvention;
    if (link_convention == LinkConvention::IMPORT && lib_path.size() != 0) {
        link_convention = DetermineLinkConvention(lib_path);
    }
    if (lib_path.size() == 0) {
        int errs = reporter->get_lib_errors(varinfo->declaration->linked_library);
        reporter->add_lib_error(varinfo->declaration->linked_library);
        if (errs >= Reporter::LIB_ERROR_LIMIT) {
            // log::out << "skip\n";
            return;
        } else {
            ERR_SECTION(
                ERR_HEAD2(varinfo->declaration->location)
                ERR_MSG_COLORED("'"<<log::LIME<<varinfo->declaration->linked_library<<log::NO_COLOR<<"' is not a library. Did you misspell it?")
                ERR_LINE2(varinfo->declaration->location, "this declaration")

                log::out << "These are the available libraries: ";
                for(int i=0;i<func_imp->libraries.size();i++){
                    if(i!=0) log::out << ", ";
                    log::out << log::LIME << func_imp->libraries[i].named_as << log::NO_COLOR;
                }
                log::out << "\n";
            )
            return;
        }
    }
    
    if(link_convention == LinkConvention::IMPORT) {
        builder.emit_ext_dataptr(reg, LinkConvention::NONE); // emit to prevent triggering asserts to make sure we keep compiling and catch more errors
        ERR_SECTION(
            ERR_HEAD2(varinfo->declaration->location)
            ERR_MSG_COLORED("Link convention (@import) for function could not be determined to @importdll or @importlib. dll/lib can be determined automatically based on the of the library or you can manually specify @importdll or @importlib. The library name and path was this: '"<<log::LIME << varinfo->declaration->linked_library <<log::NO_COLOR<<"', '"<<log::LIME<<lib_path<<log::NO_COLOR<<"'.")
            ERR_LINE2(varinfo->declaration->location,"this function")
        )
    } else {
        builder.emit_ext_dataptr(reg, link_convention);
        int reloc = builder.get_pc(); // ext_dataptr doesn't have an immediate,
        // reloc therefore points to the next instruction which may seem wierd BUT,
        // the x64 generator knows this and generates an instruction with an immediate
        // that is relocated. It's just that the bytecode doesn't need an immediate so we skip it.

        if(link_convention == LinkConvention::DYNAMIC_IMPORT && compiler->options->target == TARGET_WINDOWS_x64) {
            // Windows has an import table with a bunch of pointers.
            // Linux works a little differently and does not have this.
            alias = "__imp_" + alias;
        }
        if(varinfo->is_var()) {
            addExternalRelocation(alias, lib_path, reloc, BC_REL_GLOBAL_VAR);
        } else {
            addExternalRelocation(alias, lib_path, reloc, BC_REL_FUNCTION);
        }
    }
}
SignalIO GenContext::generatePushFromValues(BCRegister baseReg, int baseOffset, TypeId typeId, int* movingOffset){
    using namespace engone;
    
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    // u32 size = info.ast->getTypeSize(typeId);
    u32 size = REGISTER_SIZE;
    int _movingOffset = baseOffset;
    if(!movingOffset)
        movingOffset = &_movingOffset;
    
    if(!typeInfo || !typeInfo->astStruct) {
        // enum works here too
        BCRegister reg = BC_REG_T0;
        if(*movingOffset == 0){
            // If you are here to optimize some instructions then you are out of luck.
            // I checked where GeneratePush is used whether ADDI can LI can be removed and
            // replaced with a MOV_MR_DISP32 but those instructions come from GenerateReference.
            // What you need is a system to optimise away instructions while adding them (like pop after push)
            // or an optimizer which runs after the generator.
            // You need something more sophisticated to optimize further basically.
            builder.emit_mov_rm(reg, baseReg, size);
        }else{
            builder.emit_mov_rm_disp(reg, baseReg, size, *movingOffset);
        }
        builder.emit_push(reg);
        *movingOffset += size;
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePushFromValues(baseReg, baseOffset, memdata.typeId, movingOffset);
            *movingOffset += size;
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateArtificialPush(TypeId typeId) {
    using namespace engone;
    if(typeId == TYPE_VOID) {
        return SIGNAL_FAILURE;
    }
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(!typeInfo || !typeInfo->astStruct) {
        builder.emit_fake_push();
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generateArtificialPush(memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
// push of structs
SignalIO GenContext::generatePush(BCRegister baseReg, int offset, TypeId typeId){
    using namespace engone;
    if(typeId == TYPE_VOID) {
        return SIGNAL_FAILURE;
    }
    
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }

    if(!typeInfo || !typeInfo->astStruct) {
        BCRegister reg = BC_REG_T0;
        builder.emit_mov_rm_disp(reg, baseReg, size, offset);
        builder.emit_push(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePush(baseReg, offset + memdata.offset, memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
// pop of structs
SignalIO GenContext::generatePop(BCRegister baseReg, int offset, TypeId typeId){
    using namespace engone;
    // Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    int size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasAnyErrors());
        return SIGNAL_FAILURE;
    }
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        BCRegister reg = BC_REG_T0;
        builder.emit_pop(reg);
        if(baseReg==BC_REG_INVALID) {
            // throw away value
        } else {
            if(offset == 0){
                // The x64 architecture has a special meaning when using fp.
                // The converter asserts about using addModRM_disp32 instead. Instead of changing
                // it to allow this instruction we always use disp32 to avoid the complaint.
                // This instruction fires the assert: BC_MOV_RM rax, fp 
                builder.emit_mov_mr(baseReg, reg, size);
            }else{
                builder.emit_mov_mr_disp(baseReg, reg, size, offset);
            }
        }
    } else {
        for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            _GLOG(log::out << "move return value member " << member.name << "\n";)
            generatePop(baseReg, offset + memdata.offset, memdata.typeId);
        }
    }    
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePush_get_param (int offset, TypeId typeId) {
    using namespace engone;
    if(typeId == TYPE_VOID) {
        return SIGNAL_FAILURE;
    }
    
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }

    if(!typeInfo || !typeInfo->astStruct) {
        BCRegister reg = BC_REG_T0;
        builder.emit_get_param(reg, offset, size, AST::IsDecimal(typeId), AST::IsSigned(typeId));
        builder.emit_push(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePush_get_param(offset + memdata.offset, memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePop_set_arg    (int offset, TypeId typeId) {
    using namespace engone;
    // Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    int size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        BCRegister reg = BC_REG_T0;
        builder.emit_pop(reg);
        builder.emit_set_arg(reg, offset, size, AST::IsDecimal(typeId), AST::IsSigned(typeId));
    } else {
        for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            _GLOG(log::out << "move return value member " << member.name << "\n";)
            generatePop_set_arg(offset + memdata.offset, memdata.typeId);
        }
    }    
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePush_get_val   (int offset, TypeId typeId) {
    using namespace engone;
    if(typeId == TYPE_VOID) {
        return SIGNAL_FAILURE;
    }
    
    TypeInfo *typeInfo = nullptr;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }

    if(!typeInfo || !typeInfo->astStruct) {
        BCRegister reg = BC_REG_T0;
        builder.emit_get_val(reg, offset, size, AST::IsDecimal(typeId), AST::IsSigned(typeId));
        builder.emit_push(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePush_get_val(offset + memdata.offset, memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePop_set_ret    (int offset, TypeId typeId) {
    using namespace engone;
    // Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    int size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        BCRegister reg = BC_REG_T0;
        builder.emit_pop(reg);
        builder.emit_set_ret(reg, offset, size, AST::IsDecimal(typeId), AST::IsSigned(typeId));
    } else {
        for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            _GLOG(log::out << "move return value member " << member.name << "\n";)
            generatePop_set_ret(offset + memdata.offset, memdata.typeId);
        }
    }    
    return SIGNAL_SUCCESS;
}
// static memzero with a fixed size
void GenContext::genMemzero(BCRegister ptr_reg, BCRegister size_reg, int size, int offset) {
    // TODO: Move this logic into BytecodeBuilder? (emit_memzero)
    if(size <= 8) {
        if (hasAnyErrors())
            return;
        Assert(size == 1 || size == 2 || size == 4 || size == 8);
        builder.emit_bxor(size_reg, size_reg, size);
        if(offset != 0) {
            builder.emit_mov_mr_disp(ptr_reg, size_reg, size, offset);
        } else {
            builder.emit_mov_mr(ptr_reg, size_reg, size);
        }
    } else {
        int batch = 1;
        if(size % 8 == 0)
            batch = 8;
        else if(size % 4 == 0)
            batch = 4;
        else if(size % 2 == 0)
            batch = 2;
        builder.emit_li32(size_reg, size);

        // The x64 implementation of memzero needs to allocate 2 registers.
        // ptr_reg and size_reg is the registers we choose to be allocated.
        // If ptr_reg is reg_locals then only one register will be allocated automatically.
        // To avoid having to manually allocate a register in the x64 generator we
        // don't allow BC_REG_LOCALS as a register (well, we do but we swap it out for another register below)
        BCRegister reg = ptr_reg;
        if(ptr_reg == BC_REG_LOCALS) {
            // @TODO: This code is strange? reg = ptr_reg so we modify BC_REG_LOCALS except that only if offset is negative?
            reg = BC_REG_T0;
            if(offset > 0){
                builder.emit_mov_rr(reg, ptr_reg);
            } else {
                builder.emit_add_imm32(reg, ptr_reg, offset, 8);
            }
        }
        builder.emit_memzero(reg, size_reg, batch);
    }
}
void GenContext::genMemcpy(BCRegister dst_reg, BCRegister src_reg, int size) {
    if(size <= 8) {
        Assert(size == 1 || size == 2 || size == 4 || size == 8);
        builder.emit_mov_rm(BC_REG_T0, src_reg, size);
        builder.emit_mov_mr(dst_reg, BC_REG_T0, size);
    } else {
        // int batch = 1;
        // if(size % 8 == 0)
        //     batch = 8;
        // else if(size % 4 == 0)
        //     batch = 4;
        // else if(size % 2 == 0)
        //     batch = 2;
        builder.emit_li(BC_REG_T0, size, REGISTER_SIZE);
        builder.emit_memcpy(dst_reg, src_reg, BC_REG_T0);
    }
}
// baseReg as 0 will push default values to stack
// non-zero as baseReg will mov default values to the pointer in baseReg
SignalIO GenContext::generateDefaultValue(BCRegister baseReg, int offset, TypeId typeId, lexer::SourceLocation* location, bool zeroInitialize) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
   
    // MAKE_NODE_SCOPE(_expression);
    // Assert(typeInfo)

    Assert(!(baseReg == BC_REG_LOCALS && offset == 0));

    TypeInfo* typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    int size = ast->getTypeSize(typeId);
    if(baseReg != BC_REG_INVALID && zeroInitialize) {
        #ifndef DISABLE_ZERO_INITIALIZATION
        
        // builder.emit_li32(BC_REG_T1, offset);
        // builder.emit_add(BC_REG_T1, baseReg, false, REGISTER_SIZE);
        genMemzero(baseReg, BC_REG_T1, size, offset);
        
        #endif
    }
    if (typeInfo && typeInfo->astStruct) {
        for (int i = typeInfo->astStruct->members.size() - 1; i >= 0; i--) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            // log::out << "GEN "<<typeInfo->astStruct->name<<"."<<member.name<<"\n";
            // log::out << " alignedSize "<<info.ast->getTypeAlignedSize(memdata.typeId)<<"\n";
            // if(i+1<(int)typeInfo->astStruct->members.size()){
            //     // structs in structs will be aligned by their individual members
            //     // instead of the alignment of the structs as a whole.
            //     // This will make sure the structs are aligned.
            //     auto prevMem = typeInfo->getMember(i+1);
            //     u32 alignSize = info.ast->getTypeAlignedSize(prevMem.typeId);
            //     // log::out << "Try align "<<alignSize<<"\n";
            //     // info.addAlign(alignSize);
            // }
            
            if(member.array_length) {
                if(baseReg != BC_REG_INVALID) {
                    // builder.emit_li32(BC_REG_T1, offset + memdata.offset);
                    // builder.emit_add(BC_REG_T1, baseReg, false, REGISTER_SIZE);
                    int size = ast->getTypeSize(memdata.typeId);
                    genMemzero(baseReg, BC_REG_T1, size * member.array_length, offset + memdata.offset);
                } else {
                    // default value of array is zero even if struct has defaults because going through each element in the array is expensive
                    // SignalIO result = generateDefaultValue(baseReg, offset + memdata.offset, memdata.typeId, location, false);
                }
            } else if (member.defaultValue) {
                // TypeId tempTypeId = {};
                TEMP_ARRAY_N(TypeId, tempTypes, 5);
                SignalIO result = generateExpression(member.defaultValue, &tempTypes);
                
                if(tempTypes.size() == 1){
                    if (!performSafeCast(tempTypes[0], memdata.typeId)) {
                        // info.comp
                        ERRTYPE(member.location, member.defaultValue->location, tempTypes[0], memdata.typeId, "(default member)\n");
                    }
                    if(baseReg!=0){
                        SignalIO result = generatePop(baseReg, offset + memdata.offset, memdata.typeId);
                    }
                } else {
                    if (!hasAnyErrors()) {
                        StringBuilder values = {};
                        values.reserve(5);
                        FORN(tempTypes) {
                            auto& it = tempTypes[nr];
                            if(nr != 0)
                                values += ", ";
                            values += ast->typeToString(it);
                        }
                        ERR_SECTION(
                            ERR_HEAD2(member.defaultValue->location)
                            ERR_MSG("Default value of member produces more than one value but only one is allowed.")
                            ERR_LINE2(member.defaultValue->location, values.data())
                        )
                    }
                }
            } else {
                if(!memdata.typeId.isValid() || memdata.typeId == TYPE_VOID) {
                    Assert(hasForeignErrors());
                } else {
                    SignalIO result = generateDefaultValue(baseReg, offset + memdata.offset, memdata.typeId, location, false);
                }
            }
        }
    } else {
        Assert(size <= REGISTER_SIZE);
        #ifndef DISABLE_ZERO_INITIALIZATION
        // only structs have default values, otherwise zero is the default
        if(baseReg == 0){
            builder.emit_bxor(BC_REG_A, BC_REG_A, REGISTER_SIZE);
            builder.emit_push(BC_REG_A);
        } else {
            // we generate memzero above which zero initializes
            // builder.emit_bxor(BC_REG_A, BC_REG_A);
            // BCRegister reg = BC_REG_A;
            // builder.emit_mov_mr_disp(baseReg, reg, size, offset);
        }
        #else
        // Not setting zero here is certainly a bad idea
        if(baseReg == 0){
            builder.emit_push(BC_REG_A);
        }
        #endif
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::framePush(TypeId typeId, i32* outFrameOffset, bool genDefault, bool staticData){
    Assert(outFrameOffset);
    Assert(!staticData); // This is unsafe if functions use the static data before FramePush makes space for it.
    // This usually result static offsets always pointing at the start of the data segment. Then when FramePush
    // runs those offsets would be valid but it's already to late.
    // I am leaveing the comment here in case you decide to use FramePush to handle static data in the future WHICH YOU REALLY SHOULDN'T!

    // Also, when we generateData before generating bytecode for functions we need all data to be reserved.
    // otherwise some global data would not have been generated. So staticData really doesn't work.
    // I had one idea though with compile time execution which might create new functions to be checked and generated
    // but to execute it we need to have generate bytecode and globals, but then we get into trouble since the
    // bytecode may create new functions to check and generate which is problematic if they have globals in them.
    // Either we disallow global data in those functions, or we change the global data system to be more incremental.

    // IMPORTANT: This code has been copied to array assignment. Change that code
    //  when changing code here.
    i32 size = info.ast->getTypeSize(typeId);
    i32 asize = info.ast->getTypeAlignedSize(typeId);
    if(asize==0)
        return SIGNAL_FAILURE;
    
    if(staticData) {
        bytecode->ensureAlignmentInData(asize);
        int offset = bytecode->appendData(nullptr,size);
        
        Assert(false); // This is broken due to stable_global_data
        info.builder.emit_dataptr(BC_REG_D, offset);

        *outFrameOffset = offset;
        if(genDefault){
            SignalIO result = info.generateDefaultValue(BC_REG_D, 0, typeId);
            if(result!=SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;
        }
    } else {
        int sizeDiff = size % REGISTER_SIZE;
        if(sizeDiff != 0){
            size += REGISTER_SIZE - sizeDiff;
        }
        asize = REGISTER_SIZE;
        int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
        if (diff != asize) {
            info.currentFrameOffset -= diff; // align
            // builder.emit_alloc_local(BC_REG_INVALID, diff);
            currentFuncImpl->alloc_frame_space(diff);
        
        }
        info.currentFrameOffset -= size;
        *outFrameOffset = info.currentFrameOffset;
        
        // builder.emit_alloc_local(reg, size);
        currentFuncImpl->alloc_frame_space(size);
        BCRegister reg = genDefault ? BC_REG_F : BC_REG_INVALID; // TODO: Is F register in use?
        
        if(genDefault){
            SignalIO result = generateDefaultValue(BC_REG_LOCALS, info.currentFrameOffset, typeId);
            if(result!=SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;
        }
    }
    return SIGNAL_SUCCESS;
}
/*
##################################
    Generation functions below
##################################
*/
// SignalIO GenerateExpression(GenContext &info, ASTExpression *expression, TypeId *outTypeId, ScopeId idScope){
//     DynamicArray<TypeId> types{};
//     SignalIO result = GenerateExpression(info,expression,&types,idScope);
//     *outTypeId = TYPE_VOID;
//     if(types.size()>1){
//         ERR_SECTION(
//             ERR_HEAD2(expression->location)
//             ERR_MSG("Returns multiple values but the way you use the expression only supports one (or none).")
//             ERR_LINE2(expression->location,"returns "<<types.size() << " values")
//         )
//     } else {
//         if(types.size()==1)
//             *outTypeId = types[0];
//     }
//     return result;
// }
// outTypeId will represent the type (integer, struct...) but the value pushed on the stack will always
// be a pointer. EVEN when the outType isn't a pointer. It is an implicit extra level of indirection commonly
// used for assignment.
// wasNonReference is used to allow pointers as well as actual references (pointer to variable)
SignalIO GenContext::generateReference(ASTExpression* _expression, TypeId* outTypeId, ScopeId idScope, bool* wasNonReference, int* array_length){
    using namespace engone;

    TRACE_FUNC()

    CALLBACK_ON_ASSERT(
        ERR_LINE2(_expression->location,"bug")
    )

    ZoneScopedC(tracy::Color::Blue2);
    _GLOG(FUNC_ENTER)
    Assert(_expression);
    MAKE_NODE_SCOPE(_expression);
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;
    *outTypeId = TYPE_VOID;
    
    if(array_length)
        *array_length = 0;

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    TEMP_ARRAY(ASTExpression*, exprs);
    TEMP_ARRAY_N(TypeId, tempTypes, 5);
    
    TypeId endType = {};
    bool pointerType=false; // if the value on the stack is a direct pointer, false means a pointer to a pointer
    int arrayLength = 0;
    ASTExpression* next=_expression;
    while(next){
        ASTExpression* base_now = next;
        next = nullptr;
        // next = next->left;

        if(base_now->type == EXPR_IDENTIFIER) {
            auto now = base_now->as<ASTExpressionIdentifier>();
            // end point
            // auto id = info.ast->findIdentifier(idScope, now->name);
            auto id = now->identifier;
            if (!id || !id->is_var()) {
                if(!info.hasForeignErrors()){
                    ERR_SECTION(
                        ERR_HEAD2(now->location)
                        ERR_MSG("'"<< info.compiler->lexer.tostring(now->location) << "' is undefined.")
                        ERR_LINE2(now->location,"bad");
                    )
                }
                return SIGNAL_FAILURE;
            }

            auto varinfo = id->cast_var();
            // auto varinfo = info.ast->getVariableByIdentifier(id);
            _GLOG(log::out << " expr var push " << now->name << "\n";)
            // TOKENINFO(now->location)
            // char buf[100];
            // int len = sprintf(buf,"  expr push %s",now->name->c_str());
            // bytecode->addDebugText(buf,len);

            // TODO: Fix bug where local polyVersion is used for globals at import scope.

            /* NOTE: Thoughts on polyVersion and globals

                Each global variable has a list of types, each type used by a different polymorphic scope.
                When referencing a variable, we need to know which type in the list to use.
                That's where polyVersion comes in. The currentPolyVersion indicates the type to use
                in local scopes.

                A problem occurs when accessing variables outside the same polymorphic scope.
                A different polyVersion must be used.

                How do we know if a variable is inside or outside a polymorphic scope?

                Also, the code below is very problematic. The function bottom refers to a polymorphic global which exists in a different polymorphic scope than it's own function scope. The correct way of handling this would be to let bottom be a polymorphic function. Each generate top function would require it's own bottom function. Maybe we can change the polymorphic system to be per scope instead of per function and parent struct?
                
                    fn top<T>() {
                        global local_data: T

                        fn bottom() {
                            local_data++
                        }
                        bottom()
                    }
            
            */

            auto version = currentPolyVersion; // globals do not share polyVersion with current scope, they can come from a different scope
            if(varinfo->is_import_global)
                version = 0;

            TypeInfo *typeInfo = 0;
            TypeId typeId = varinfo->versions_typeId[version];
            if(typeId.isNormalType())
                typeInfo = info.ast->getTypeInfo(typeId);
            
            switch(varinfo->type) {
                case Identifier::GLOBAL_VARIABLE: {
                    if(varinfo->declaration && varinfo->declaration->isImported()) {
                        generate_ext_dataptr(BC_REG_B, varinfo);
                    } else {
                        emit_abstract_dataptr(BC_REG_B, varinfo->versions_dataOffset[version], varinfo);
                    }
                } break; 
                case Identifier::LOCAL_VARIABLE: {
                    builder.emit_ptr_to_locals(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                    // builder.emit_add(BC_REG_B, BC_REG_BP, false, REGISTER_SIZE);
                } break; 
                case Identifier::MEMBER_VARIABLE: {
                    // NOTE: Is member variable/argument always at this offset with all calling conventions?
                    // Assert(info.currentFunction->callConvention == BETCALL);
                    builder.emit_get_param(BC_REG_B, 0, REGISTER_SIZE, false, true);
                    
                    auto& mem = currentFunction->parentStruct->members[varinfo->memberIndex];
                    if (mem.array_length) {
                        arrayLength = mem.array_length;
                        // std::string real_type = "Slice<"+ast->typeToString(mem.stringType)+">";
                        // bool printed = false;
                        // typeId = ast->convertToTypeId(real_type, currentScopeId, true);

                        typeId.setPointerLevel(typeId.getPointerLevel() + 1);
                        
                        builder.emit_li32(BC_REG_A, varinfo->versions_dataOffset[info.currentPolyVersion]);
                        builder.emit_add(BC_REG_B, BC_REG_A, REGISTER_SIZE, false, true);
                        pointerType = true;
                    } else {
                        builder.emit_li32(BC_REG_A, varinfo->versions_dataOffset[info.currentPolyVersion]);
                        builder.emit_add(BC_REG_B, BC_REG_A, REGISTER_SIZE, false, true);
                    }
                } break;
                case Identifier::ARGUMENT_VARIABLE: {
                    builder.emit_ptr_to_params(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);

                    // Crazy idea:
                    //   BC_SET_PARAM and force the caller of this generateReference to directly set
                    //   instead of retrieving a pointer. It saves one instruction or so.

                    // ERR_SECTION(
                    //     ERR_HEAD2(now->location)
                    //     ERR_MSG("You cannot change the value of parameters. More specifically, you cannot take a reference to them.")
                    //     ERR_LINE2(now->location,"can't reference parameters")
                    // )
                } break;
                default: {
                    Assert(false);
                }
            }
            builder.emit_push(BC_REG_B);

            endType = typeId;
            break;
        } else if(base_now->type == EXPR_MEMBER) {
            auto now = base_now->as<ASTExpressionMember>();
            exprs.add(now);
            next = now->left;
        } else if (base_now->type == EXPR_OPERATION && base_now->as<ASTExpressionOperation>()->op_type  == AST_DEREF) {
            auto now = base_now->as<ASTExpressionOperation>();
            exprs.add(now);
            next = now->left;
        } else if(base_now->type == EXPR_OPERATION && base_now->as<ASTExpressionOperation>()->op_type == AST_INDEX) {
            auto now = base_now->as<ASTExpressionOperation>();
            FuncImpl* operatorImpl = nullptr;
            if (currentPolyVersion < now->versions_overload.size())
                operatorImpl = now->versions_overload[currentPolyVersion].funcImpl;
            if (operatorImpl) {
                TypeId typeId{};
                auto result = generateExpression(now, &tempTypes, idScope);
                if(result!=SIGNAL_SUCCESS || tempTypes.size()!=1){
                    return SIGNAL_FAILURE;
                }
                if (tempTypes.size() > 0)
                    typeId = tempTypes[0];

                if (typeId.getPointerLevel() == 0) {
                    ERR_SECTION(
                        ERR_HEAD2(now->location)
                        ERR_MSG("The expression is a user defined index operator and evaluates to '"<<ast->typeToString(typeId)<<"' which is a discrete value. The code tries to take a reference to this value which isn't possible. The index operator must return a pointer for this to be possible.")
                        ERR_LINE2(now->location, ast->typeToString(typeId))
                    )
                    return SIGNAL_FAILURE;
                }
                
                endType = typeId;
                pointerType=true;
                break;
            }
            // TODO: Refer is handled in GenerateExpression since refer will
            //   be combined with other operations. Refer on it's own isn't
            //   very useful and therefore not common so there is not
            //   benefit of handling refer here. Probably.
            exprs.add(base_now);
            next = now->left;
        } else {
            // end point
            tempTypes.resize(0);
            SignalIO result = generateExpression(base_now, &tempTypes, idScope);
            if(result!=SIGNAL_SUCCESS || tempTypes.size()!=1){
                return SIGNAL_FAILURE;
            }

            if (exprs.size() > 0 && exprs.last()->type == EXPR_MEMBER) {
                auto mem_expr = exprs.last()->as<ASTExpressionMember>();
                TypeId type = tempTypes[0];
                TypeInfo* typeInfo = nullptr;
                typeInfo = info.ast->getTypeInfo(type.baseType());

                if(!typeInfo || !typeInfo->astStruct){
                    ERR_SECTION(
                        ERR_HEAD2(base_now->location)
                        ERR_MSG("'"<<info.ast->typeToString(type)<<"' is not a struct. Cannot access member.")
                        ERR_LINE2(base_now->location, info.ast->typeToString(type).c_str())
                    )
                    return SIGNAL_FAILURE;
                }

                if (type.getPointerLevel() == 0) {
                    if (typeInfo->astStruct->name == "Slice" && mem_expr->name == "ptr") {
                        builder.emit_pop(BC_REG_B);
                        builder.emit_pop(BC_REG_T0); // pop len from slice
                        builder.emit_push(BC_REG_B);

                        auto memdata = typeInfo->getMember("ptr");

                        endType = memdata.typeId;
                        pointerType=true;
                        exprs.pop();
                        break;
                    }
                } else {
                    // if returned type is pointer then we simply handle it later
                    // it's when it's not a pointer where we need some special code
                }
            }

            endType = tempTypes.last();
            pointerType=true;
            break;
        }
    }

    for(int i=exprs.size()-1;i>=0;i--){
        ASTExpression* base_now = exprs[i];
        
        if(base_now->type == EXPR_MEMBER){
            auto now = base_now->as<ASTExpressionMember>();
            if(arrayLength) {
                if(now->name == "len") {
                    Assert(false);
                    // What do we do here. We can't exactly push a constant since generateReference
                    // is for actual references and pointers.
                    builder.emit_li32(BC_REG_T0, arrayLength);
                    builder.emit_push(BC_REG_T0);
                } else if(now->name == "ptr") {
                    Assert(false);
                }
                arrayLength = 0;
            }

            TypeInfo* typeInfo = nullptr;
            typeInfo = info.ast->getTypeInfo(endType.baseType());

            if(!typeInfo || !typeInfo->astStruct){
                ERR_SECTION(
                    ERR_HEAD2(now->location)
                    ERR_MSG("'"<<info.ast->typeToString(endType)<<"' is not a struct. Cannot access member.")
                    ERR_LINE2(now->left->location, info.ast->typeToString(endType).c_str())
                )
                return SIGNAL_FAILURE;
            }

            if(pointerType){
                pointerType=false;
                if(!endType.isPointer()) {
                    ASTExpression* prev = now->left;
                    if (i+1 < exprs.size())
                        prev = exprs[i+1];
                    ERR_SECTION(
                        ERR_HEAD2(prev->location)
                        ERR_MSG("'"<<compiler->lexer.tostring(prev->location)<<
                        "' must be a reference to some memory. "
                        "A variable, member or expression resulting in a dereferenced pointer would be. The type was '"<<ast->typeToString(endType)<<"'")
                        ERR_LINE2(prev->location, "must be a reference")
                    )
                    return SIGNAL_FAILURE;
                }
                endType.setPointerLevel(endType.getPointerLevel()-1);
            }
            if(endType.getPointerLevel()>1){ // one level of pointer is okay.
                ERR_SECTION(
                    ERR_HEAD2(now->location)
                    ERR_MSG("'"<<info.ast->typeToString(endType)<<"' has to many levels of pointing. Can only access members of a single or non-pointer.")
                    ERR_LINE2(now->location, "to pointy")
                )
                return SIGNAL_FAILURE;
            }
            // TypeInfo* typeInfo = nullptr;
            // typeInfo = info.ast->getTypeInfo(endType.baseType());
            // if(!typeInfo || !typeInfo->astStruct){
            //     ERR_SECTION(
            //         ERR_HEAD2(now->location)
            //         ERR_MSG("'"<<info.ast->typeToString(endType)<<"' is not a struct. Cannot access member.")
            //         ERR_LINE2(now->left->location, info.ast->typeToString(endType).c_str())
            //     )
            //     return SIGNAL_FAILURE;
            // }

            auto memberData = typeInfo->getMember(now->name);
            if(memberData.index==-1){
                Assert(info.hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            auto& mem = typeInfo->astStruct->members[memberData.index];
            if(mem.array_length > 0) {
                pointerType = true;
                arrayLength = mem.array_length;
                
                bool popped = false;
                BCRegister reg = BC_REG_B;
                if(endType.getPointerLevel()>0){
                    if(!popped)
                        builder.emit_pop(reg);
                    popped = true;
                    builder.emit_mov_rm(BC_REG_C, reg, REGISTER_SIZE);
                    reg = BC_REG_C;
                }
                if(memberData.offset!=0){
                    if(!popped)
                        builder.emit_pop(reg);
                    popped = true;
                    
                    builder.emit_li32(BC_REG_A, memberData.offset);
                    builder.emit_add(reg, BC_REG_A, REGISTER_SIZE, false, true);
                }
                if(popped)
                    builder.emit_push(reg);

                endType = memberData.typeId;
                endType.setPointerLevel(endType.getPointerLevel()+1);
                // // TODO: Improve error message, the message is bad partly because I don't know
                // //   what should happen or how referencing should work so figure that out.
                // ERR_SECTION(
                //     ERR_HEAD2(now->location)
                //     ERR_MSG("You cannot take a reference to an array inside a struct. Consider taking a poiner.")
                //     ERR_LINE2(now->location, "")
                //     ERR_LINE2(mem.location, "this is an array")
                // )
                // return SIGNAL_FAILURE;
                
            } else {
                // TODO: You can do more optimisations here as long as you don't
                //  need to dereference or use MOV_MR. You can have the RBX popped
                //  and just use ADDI on it to get the correct members offset and keep
                //  doing this for all member accesses. Then you can push when done.
                
                bool popped = false;
                BCRegister reg = BC_REG_B;
                if(endType.getPointerLevel()>0){
                    if(!popped)
                        builder.emit_pop(reg);
                    popped = true;
                    builder.emit_mov_rm(BC_REG_C, reg, REGISTER_SIZE);
                    reg = BC_REG_C;
                }
                if(memberData.offset!=0){
                    if(!popped)
                        builder.emit_pop(reg);
                    popped = true;
                    
                    builder.emit_li32(BC_REG_A, memberData.offset);
                    builder.emit_add(reg, BC_REG_A, REGISTER_SIZE, false, true);
                }
                if(popped)
                    builder.emit_push(reg);

                endType = memberData.typeId;
            }
        } else if(base_now->type == EXPR_OPERATION && base_now->as<ASTExpressionOperation>()->op_type == AST_DEREF) {
            auto now = base_now->as<ASTExpressionOperation>();
            arrayLength = 0;
            if(pointerType){
                // PROTECTIVE BARRIER took a hit
                pointerType=false;
                if(!endType.isPointer()) {
                    auto prev = exprs[i+1];
                    ERR_SECTION(
                        ERR_HEAD2(prev->location)
                        ERR_MSG("'"<<compiler->lexer.tostring(prev->location)<<
                        "' must be a reference to some memory. "
                        "A variable, member or expression resulting in a dereferenced pointer would be. The type was '"<<ast->typeToString(endType)<<"'")
                        ERR_LINE2(prev->location, "must be a reference")
                    )
                    return SIGNAL_FAILURE;
                }
                endType.setPointerLevel(endType.getPointerLevel()-1);
                continue;
            }
            if(endType.getPointerLevel()<1){
                ERR_SECTION(
                    ERR_HEAD2(now->left->location)
                    ERR_MSG("type '"<<info.ast->typeToString(endType)<<"' is not a pointer.")
                    ERR_LINE2(now->left->location,"must be a pointer")
                )
                return SIGNAL_FAILURE;
            }
            if(endType.getPointerLevel()>0){
                
                builder.emit_pop(BC_REG_B);
                builder.emit_mov_rm(BC_REG_C, BC_REG_B, REGISTER_SIZE);
                builder.emit_push(BC_REG_C);
            } else {
                // do nothing
            }
            endType.setPointerLevel(endType.getPointerLevel()-1);
        } else if(base_now->type == EXPR_OPERATION && base_now->as<ASTExpressionOperation>()->op_type == AST_INDEX) {
            auto now = base_now->as<ASTExpressionOperation>();
            arrayLength = 0;
            FuncImpl* operatorImpl = nullptr;
            if(now->versions_overload.size()>0)
                operatorImpl = now->versions_overload[info.currentPolyVersion].funcImpl;
            // I recently changed code in type checker for overload matching with polymorphism. Instead of == on types, we do check castable.

            if (operatorImpl) {
                // Handled when we add together exprs
                Assert(("operator overloading when referencing not implemented",!operatorImpl));
                // generateExpression(now, &tempTypes, idScope);
            } else {
                tempTypes.resize(0);
                SignalIO result = generateExpression(now->right, &tempTypes);
                if (result != SIGNAL_SUCCESS && tempTypes.size() != 1)
                    return SIGNAL_FAILURE;
                
                if(!AST::IsInteger(tempTypes.last())){
                    std::string strtype = info.ast->typeToString(tempTypes.last());
                    ERR_SECTION(
                        ERR_HEAD2(now->right->location)
                        ERR_MSG("Index operator (array[23]) requires integer type in the inner expression. '"<<strtype<<"' is not an integer. (Operator overloading doesn't work here)")
                        ERR_LINE2(now->right->location,strtype.c_str());
                    )
                    return SIGNAL_FAILURE;
                }

                TypeInfo* linfo = nullptr;
                if(endType.isNormalType()) {
                    linfo = ast->getTypeInfo(endType);
                }
                bool is_slice = linfo && linfo->astStruct && linfo->astStruct->name == "Slice";
                if(is_slice) {
                    // NOTE: You may think that operator overload for index operator is implemented and
                    //   we therefore don't need is_slice BUT we operator overloading works like a
                    //   function with two inputs and one output. Since the output is a discrete value,
                    //   generateReference doesn't work since you can't take a pointer to a value that 
                    //   that is being pushed and popped on the stack.
                } else if(!endType.isPointer()){
                    if(!info.hasForeignErrors()){
                        std::string strtype = info.ast->typeToString(endType);
                        ERR_SECTION(
                            ERR_HEAD2(now->left->location)
                            ERR_MSG("Index operator (array[23]) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer. (Operator overloading doesn't work here)")
                            ERR_LINE2(now->left->location,strtype.c_str());
                        )
                    }
                    return SIGNAL_FAILURE;
                }

                if(pointerType){
                    pointerType=false;
                    if(is_slice) {
                        auto& mem = linfo->structImpl->members[0];
                        Assert(linfo->astStruct->members[0].name == "ptr");

                        endType = mem.typeId;
                        endType.setPointerLevel(endType.getPointerLevel()-1);
                        u32 typesize = info.ast->getTypeSize(endType);
                        u32 rsize = info.ast->getTypeSize(tempTypes.last());
                        BCRegister indexer_reg = BC_REG_D;

                        builder.emit_pop(indexer_reg); // integer

                        builder.emit_pop(BC_REG_B); // slice.ptr
                        builder.emit_pop(BC_REG_F); // slice.len
                        
                        // TODO: BOUNDS CHECK
                        if(typesize>1){
                            builder.emit_li32(BC_REG_A, typesize);
                            builder.emit_mul(indexer_reg, BC_REG_A, 4, false, false);
                        }
                        builder.emit_add(BC_REG_B, indexer_reg, REGISTER_SIZE, false, true);

                        builder.emit_push(BC_REG_B);
                        continue;
                    } else {
                        if(!endType.isPointer()) {
                            auto prev = exprs[i+1];
                            ERR_SECTION(
                                ERR_HEAD2(prev->location)
                                ERR_MSG("'"<<compiler->lexer.tostring(prev->location)<<
                                "' must be a reference to some memory. "
                                "A variable, member or expression resulting in a dereferenced pointer would be. The type was '"<<ast->typeToString(endType)<<"'")
                                ERR_LINE2(prev->location, "must be a reference")
                            )
                            return SIGNAL_FAILURE;
                        }
                        endType.setPointerLevel(endType.getPointerLevel()-1);
                        
                        u32 typesize = info.ast->getTypeSize(endType);
                        u32 rsize = info.ast->getTypeSize(tempTypes.last());
                        BCRegister reg = BC_REG_C;
                        builder.emit_pop(reg); // integer
                        builder.emit_pop(BC_REG_B); // reference

                        if(typesize>1){
                            builder.emit_li32(BC_REG_A, typesize);
                            builder.emit_mul(reg, BC_REG_A, 4, false, false);
                        }
                        builder.emit_add(BC_REG_B, reg, REGISTER_SIZE, false, true);
                        builder.emit_push(BC_REG_B);
                        continue;
                    }
                }

                if(is_slice) {
                    auto& mem = linfo->structImpl->members[0];
                    Assert(linfo->astStruct->members[0].name == "ptr");

                    endType = mem.typeId;
                    endType.setPointerLevel(endType.getPointerLevel()-1);
                    u32 typesize = info.ast->getTypeSize(endType);
                    u32 rsize = info.ast->getTypeSize(tempTypes.last());
                    BCRegister reg = BC_REG_D;

                    builder.emit_pop(reg); // integer

                    builder.emit_pop(BC_REG_B); // reference to slice
                    
                    builder.emit_mov_rm_disp(BC_REG_C, BC_REG_B, REGISTER_SIZE, mem.offset); // dereference slice to pointer
                    // builder.emit_mov_rm(BC_REG_C, BC_REG_B, REGISTER_SIZE, ); // dereference len
                    
                    // TODO: BOUNDS CHECK
                    if(typesize>1){
                        builder.emit_li32(BC_REG_A, typesize);
                        builder.emit_mul(reg, BC_REG_A, REGISTER_SIZE, false, false);
                    }
                    builder.emit_add(BC_REG_C, reg, REGISTER_SIZE, false);

                    builder.emit_push(BC_REG_C);
                } else {
                    endType.setPointerLevel(endType.getPointerLevel()-1);

                    u32 typesize = info.ast->getTypeSize(endType);
                    u32 rsize = info.ast->getTypeSize(tempTypes.last());
                    BCRegister reg = BC_REG_D;
                    builder.emit_pop(reg); // integer
                    builder.emit_pop(BC_REG_B); // reference
                    builder.emit_mov_rm(BC_REG_C, BC_REG_B, REGISTER_SIZE); // dereference pointer to pointer
                    
                    if(typesize>1){
                        builder.emit_li32(BC_REG_A, typesize);
                        builder.emit_mul(reg, BC_REG_A, REGISTER_SIZE, false, false);
                    }
                    builder.emit_add(BC_REG_C, reg, REGISTER_SIZE, false);

                    builder.emit_push(BC_REG_C);
                }
            }
        }
    }
    if(pointerType && !wasNonReference){
        ERR_SECTION(
            ERR_HEAD2(_expression->location)
            ERR_MSG("'"<<compiler->lexer.tostring(_expression->location)<<
            "' must be a reference to some memory. "
            "A variable, member or expression resulting in a dereferenced pointer would work.")
            ERR_LINE2(_expression->location, "must be a reference")
        )
        return SIGNAL_FAILURE;
    }
    if(array_length)
        *array_length = arrayLength;
    if(wasNonReference)
        *wasNonReference = pointerType;
    *outTypeId = endType;
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateSpecialFncall(ASTExpressionCall* expression){
    if(expression->args.size() != 1){
        Assert(hasForeignErrors()); // type checker checks error
        // ERR_SECTION(
        //     ERR_HEAD2(expression->location)
        //     ERR_MSG("'"<<expression->name<<"' takes one argument, not "<<expression->args.size()<<".")
        //     ERR_LINE2(expression->location, "here")
        // )
        return SIGNAL_FAILURE;
    }

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    TEMP_ARRAY_N(TypeId, types, 5);
    auto signal = generateExpression(expression->args[0], &types, currentScopeId);

    if(signal != SIGNAL_SUCCESS)
        return SIGNAL_FAILURE;

    if(types.size() != 1) {
        Assert(hasForeignErrors()); // type checker checks error
        // ERR_SECTION(
        //     ERR_HEAD2(expression->location)
        //     ERR_MSG("Expression returns "<<types.size()<<" types while 1 is expected.")
        //     ERR_LINE2(expression->location, "here")
        // )
        return SIGNAL_FAILURE;
    }

    TypeId arg_type = types[0];

    bool is_destruct = expression->name == "destruct";
    if(expression->name == "construct" || expression->name == "destruct") { 
        // NOTE: construct and destruct are very similar, they can share code, search for is_destruct to find the differences
        if(arg_type.getPointerLevel() == 0) {
            Assert(hasForeignErrors()); // type checker checks error
            // ERR_SECTION(
            //     ERR_HEAD2(expression->location)
            //     ERR_MSG("'"<<expression->name<<"' expects a pointer to a \"data object\" where initialization should occur.")
            //     ERR_LINE2(expression->location, "here")
            // )
            return SIGNAL_FAILURE;
        }

        arg_type.setPointerLevel(arg_type.getPointerLevel()-1);

        TypeInfo* typeinfo = ast->getTypeInfo(arg_type.baseType());
        Assert(typeinfo); // shouldn't be possible

        if(arg_type.getPointerLevel() == 0 && typeinfo->astStruct) {
            builder.emit_pop(BC_REG_B); // pop pointer to data object

            OverloadGroup::Overload* overload = &expression->versions_overload[currentPolyVersion];

            if(!overload || !overload->astFunc || !overload->funcImpl) {
                generateDefaultValue(BC_REG_B, 0, arg_type, &expression->location);
            } else {
                if(!is_destruct) {
                    // In 'init' functions, the user will expect the struct to be initialized to default values or at least zeroes. So  we do that. It's not performant but if the user performance they'll probable not construct or initialize things this way anyway.
                    // construct on non-zero memory such as memory from heap would have been problematic.
                    generateDefaultValue(BC_REG_B, 0, arg_type, &expression->location);
                    // Code is partly copied from generateFncall
                }
                // Code is partly copied from generateFncall
                int allocated_stack_space = 0;
                auto signature = &overload->funcImpl->signature;
                CallConvention call_convention = signature->convention;

                switch(call_convention){
                    case BETCALL: {
                        int stackSpace = signature->argSize;
                        allocated_stack_space = stackSpace;
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        for(int i=0;i<signature->argumentTypes.size();i++){
                            int size = info.ast->getTypeSize(signature->argumentTypes[i].typeId);
                            if(size>REGISTER_SIZE){
                                // TODO: This should be moved to the type checker.
                                ERR_SECTION(
                                    ERR_HEAD2(expression->location)
                                    ERR_MSG("Argument types cannot be larger than "<<REGISTER_SIZE<<" bytes when using stdcall.")
                                    ERR_LINE2(expression->location, "bad")
                                )
                                return SIGNAL_FAILURE;
                            }
                        }

                        if(call_convention == STDCALL) {
                            int stackSpace = signature->argumentTypes.size() * REGISTER_SIZE;
                            if(stackSpace<32)
                                stackSpace = 32;
                            
                            allocated_stack_space = stackSpace;
                        } else {
                            // How does unixcall deal with stack?
                            // I guess we just allocate some for now.
                            int stackSpace = signature->argumentTypes.size()* REGISTER_SIZE;
                            allocated_stack_space = stackSpace;
                        }
                    } break;
                    // case CDECL_CONVENTION: {
                    //     Assert(false); // INCOMPLETE
                    // } break;
                    case INTRINSIC: {
                        // do nothing
                    } break;
                    default: Assert(false);
                }
                
                if(call_convention != INTRINSIC) {
                    currentFuncImpl->update_max_arguments(allocated_stack_space);
                    builder.emit_alloc_args(BC_REG_INVALID, allocated_stack_space);
                }

                builder.emit_set_arg(BC_REG_B, signature->argumentTypes[0].offset, REGISTER_SIZE, false, true);

                TEMP_ARRAY_N(TypeId, tempTypes, 5);
                // Code copied from generateFncall
                for(int i=1;i<overload->astFunc->arguments.size();i++) {
                    auto arg = overload->astFunc->arguments[i].defaultValue;
                    Assert(arg);
                    tempTypes.resize(0);
                    auto result = generateExpression(arg, &tempTypes);
                        if(tempTypes.size() == 0) {
                    if(result == SIGNAL_SUCCESS) {
                            Assert(info.hasForeignErrors());
                        } else {
                            TypeId argType = tempTypes[0];
                            // log::out << "PUSH ARG "<<info.ast->typeToString(argType)<<"\n";
                            bool wasSafelyCasted = performSafeCast(argType, signature->argumentTypes[i].typeId);

                            // NOTE: We don't allow cast operators here because i'd rather have less and simpler code.
                            if(!wasSafelyCasted && !info.hasAnyErrors()){
                                ERR_SECTION(
                                    ERR_HEAD2(arg->location)
                                    ERR_MSG("Cannot cast argument of type " << info.ast->typeToString(argType) << " to " << info.ast->typeToString(signature->argumentTypes[i].typeId) << ". The function in question was called by destruct/construct.")
                                    ERR_LINE2(arg->location,"bad argument")
                                    ERR_LINE2(expression->location,"the caller expression")
                                )
                            }
                        }
                    }
                }

                for(int i=overload->astFunc->arguments.size()-1;i >= 1;i--) {
                    auto arg = overload->astFunc->arguments[i].defaultValue;
                    generatePop_set_arg(signature->argumentTypes[i].offset, signature->argumentTypes[i].typeId);
                }

                int reloc = 0;
                builder.emit_call(overload->astFunc->linkConvention, overload->astFunc->callConvention, &reloc);
                info.addCallToResolve(reloc, overload->funcImpl);
    
                builder.emit_free_args(allocated_stack_space);
            }
        } else if(arg_type != TYPE_VOID) {
            builder.emit_pop(BC_REG_B); // pop pointer to data object

            int size = ast->getTypeSize(arg_type);
            Assert(size <= REGISTER_SIZE); // only structs can be bigger than 4/8
            genMemzero(BC_REG_B, BC_REG_A, size, 0);
        } else {
            auto loc = expression->args[0]->location;
            ERR_SECTION(
                ERR_HEAD2(loc)
                ERR_MSG("Expressions evaluates to void*. Compiler cannot construct/destruct void.")
                ERR_LINE2(loc, "here")
            )
            return SIGNAL_FAILURE;
        }
    } else {
        Assert(false);
    }

    return SIGNAL_SUCCESS;
}
// SignalIO GenContext::generateFunctionCallFromArgs(FunctionSignature* signature, OverloadGroup::Overload overload, const BaseArray<ASTExpression*> args, QuickArray<TypeId>* outTypeIds, bool isOperator) {
    // this won't work, what if caller wants arguments to be generated with generateReference and not generateExpression, i'd like to avoid function callbacks and lambdas.
// }
SignalIO GenContext::generateFncall(ASTExpression* base_expression, QuickArray<TypeId>* outTypeIds, bool isOperator){
    using namespace engone;

    if(base_expression->type == EXPR_CALL && (base_expression->as<ASTExpressionCall>()->name == "construct" || base_expression->as<ASTExpressionCall>()->name == "destruct")) {
        return generateSpecialFncall(base_expression->as<ASTExpressionCall>());
    }
    
    ASTFunction* astFunc = nullptr;
    FuncImpl* funcImpl = nullptr;
    FunctionSignature* signature = base_expression->type == EXPR_CALL ? base_expression->as<ASTExpressionCall>()->versions_func_type[currentPolyVersion] : base_expression->as<ASTExpressionOperation>()->versions_func_type[currentPolyVersion];
    bool is_function_pointer = signature;
    if (!is_function_pointer) {
        OverloadGroup::Overload overload = base_expression->type == EXPR_CALL ? base_expression->as<ASTExpressionCall>()->versions_overload[info.currentPolyVersion] : base_expression->as<ASTExpressionOperation>()->versions_overload[info.currentPolyVersion];
        astFunc = overload.astFunc;
        funcImpl = overload.funcImpl;
        if(funcImpl)
            signature = &overload.funcImpl->signature;
    } else {
        int hi = 0;
    }
    if(!info.hasForeignErrors()) {
        // not ok, type checker should have generated the right overload.
        // May happen if  we put TEST_ERROR before statement
        // Assert((astFunc && funcImpl) || signature);
    }
    if((!astFunc || !funcImpl) && !signature) {
        return SIGNAL_FAILURE;
    }

    // overload comes from type checker
    if(isOperator) {
        _GLOG(log::out << "Operator overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)
    } else {
        _GLOG(log::out << "Overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)
    }
    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    TEMP_ARRAY(ASTExpression*,all_arguments);
    all_arguments.resize(signature->argumentTypes.size());
    
    TEMP_ARRAY_N(TypeId,tempTypes, 5);

    if(isOperator) {
        auto expression = base_expression->as<ASTExpressionOperation>();
        all_arguments[0] = expression->left;
        all_arguments[1] = expression->right;
    } else {
        auto expression = base_expression->as<ASTExpressionCall>();
        int extra = expression->isMemberCall() && is_function_pointer ? 1 : 0;

        for (int i = 0; i < (int)expression->args.size() - extra;i++) {
            ASTExpression* arg = expression->args.get(i + extra);
            Assert(arg);
            if(!arg->namedValue.len){
                if(expression->hasImplicitThis()) {
                    all_arguments[i+1] = arg;
                } else {
                    all_arguments[i] = arg;
                }
            }else{
                Assert(!is_function_pointer);
                int argIndex = -1;
                for(int j=0;j<(int)astFunc->arguments.size();j++){
                    if(astFunc->arguments[j].name==arg->namedValue){
                        argIndex=j;
                        break;
                    }
                }
                if(argIndex!=-1) {
                    if (all_arguments[argIndex] != nullptr) {
                        ERR_SECTION(
                            ERR_HEAD2(arg->location)
                            ERR_MSG( "A named argument cannot specify an occupied parameter.")
                            ERR_LINE2(astFunc->location,"this overload")
                            ERR_LINE2(all_arguments[argIndex]->location,"occupied")
                            ERR_LINE2(arg->location,"bad coder")
                        )
                    } else {
                        all_arguments[argIndex] = arg;
                    }
                } else{
                    ERR_SECTION(
                        ERR_HEAD2(arg->location)
                        ERR_MSG_COLORED("Named argument is not a parameter of '"; funcImpl->print(info.ast,astFunc); engone::log::out << "'.")
                        ERR_LINE2(arg->location,"not valid")
                    )
                    // continue like nothing happened
                }
            }
        }
        if(!is_function_pointer)
            for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
                auto &arg = astFunc->arguments[i];
                if (!all_arguments[i])
                    all_arguments[i] = arg.defaultValue;
            }
    }

    int allocated_stack_space = 0;

    CallConvention call_convention = signature->convention;

    switch(call_convention){
        case BETCALL: {
            int stackSpace = signature->argSize;
            allocated_stack_space = stackSpace;
        } break;
        case STDCALL:
        case UNIXCALL: {
            for(int i=0;i<signature->argumentTypes.size();i++){
                int size = info.ast->getTypeSize(signature->argumentTypes[i].typeId);
                if(size>REGISTER_SIZE){
                    // TODO: This should be moved to the type checker.
                    ERR_SECTION(
                        ERR_HEAD2(base_expression->location)
                        ERR_MSG("Argument types cannot be larger than "<<REGISTER_SIZE<<" bytes when using stdcall.")
                        ERR_LINE2(base_expression->location, "bad")
                    )
                    return SIGNAL_FAILURE;
                }
            }

            if(call_convention == STDCALL) {
                int stackSpace = signature->argumentTypes.size() * REGISTER_SIZE;
                if(stackSpace<32)
                    stackSpace = 32;
                
                allocated_stack_space = stackSpace;
            } else {
                // How does unixcall deal with stack?
                // I guess we just allocate some for now.
                int stackSpace = signature->argumentTypes.size() * REGISTER_SIZE;
                allocated_stack_space = stackSpace;
            }
        } break;
        // case CDECL_CONVENTION: {
        //     Assert(false); // INCOMPLETE
        // } break;
        case INTRINSIC: {
            // do nothing
        } break;
        default: Assert(false);
    }
    
    if(call_convention != INTRINSIC) {
        // we should always emit alloc_args even if we don't have any arguments to ensure 16-byte alignment
        // The x64 generator (and virtual machine) ensures 16-byte alignment on alloc_args instructions
        builder.emit_alloc_args(BC_REG_INVALID, allocated_stack_space);
        // We don't have current func impl when evaluatin at compile time
        if(currentFuncImpl)
            currentFuncImpl->update_max_arguments(allocated_stack_space);
    }

    // Evaluate arguments and push the values to stack
    for(int i=0;i<(int)all_arguments.size();i++){
        auto arg = all_arguments[i];
        if(base_expression->type == EXPR_CALL && base_expression->as<ASTExpressionCall>()->hasImplicitThis() && i == 0) {
            // Make sure we actually have this stored before at the base pointer of
            // the current function.
            Assert(info.currentFunction && info.currentFunction->parentStruct);
            // Assert(info.currentFunction->callConvention == BETCALL);
            // NOTE: Is member variable/argument always at this offset with all calling conventions?
            // builder.emit_mov_rm_disp(BC_REG_B, BC_REG_BP, REGISTER_SIZE, GenContext::FRAME_SIZE);
            builder.emit_get_param(BC_REG_B, 0, REGISTER_SIZE, false);
            builder.emit_push(BC_REG_B);
            continue;
        } else {
            Assert(arg);
        }
        
        TypeId argType = {};
        SignalIO result = SIGNAL_FAILURE;
        if(base_expression->type == EXPR_CALL && base_expression->as<ASTExpressionCall>()->isMemberCall() && !is_function_pointer && i == 0) {
            // method call and first argument which is 'this'
            bool nonReference = false;
            result = generateReference(arg, &argType, -1, &nonReference);
            if(result==SIGNAL_SUCCESS){
                if(!nonReference) {
                    if(!argType.isPointer()){
                        argType.setPointerLevel(1);
                    } else {
                        Assert(argType.getPointerLevel()==1);
                        builder.emit_pop(BC_REG_B);
                        builder.emit_mov_rm(BC_REG_A, BC_REG_B, REGISTER_SIZE);
                        builder.emit_push(BC_REG_A);
                    }
                } else {
                    // TODO: Improve error message
                    ERR_SECTION(
                        ERR_HEAD2(arg->location)
                        ERR_MSG("Methods are only allowed on expressions that evaluate to some kind of reference or pointer. The marked expression was not such a type.")
                        ERR_LINE2(arg->location,ast->typeToString(argType))
                    )
                }
            } else {
                Assert(hasAnyErrors());
            }
        } else {
            // TODO: if expr should be casted, then emit space for arguments
            // we need to know the type in before hand
            // should we generate emit_alloc_args anyway and then
            // if it's not needed, delete a bunch of instructions?
            // builder.emit_alloc_args(0);
            int off_alloc_args = 0;
            if(base_expression->uses_cast_operator) {
                // only emit if we need to
                // unecessary bytecode instructions otherwise.
                // x64_gen does optimize them away though.
                builder.emit_empty_alloc_args(&off_alloc_args);
            }
            // TODO: if alloc args isn't used, we may have problems.
            tempTypes.resize(0);
            result = generateExpression(arg, &tempTypes);
            if(result == SIGNAL_SUCCESS) {
                if(tempTypes.size() == 0) {
                    Assert(info.hasForeignErrors());
                } else {
                    TypeId argType = tempTypes[0];
                    // log::out << "PUSH ARG "<<info.ast->typeToString(argType)<<"\n";
                    bool wasSafelyCasted = performSafeCast(argType, signature->argumentTypes[i].typeId);
                    if(!wasSafelyCasted && i < astFunc->nonDefaults) {
                        // cast operator isn't supported with non named arguments at the moment
                        Assert(base_expression->uses_cast_operator);
                        OverloadGroup::Overload cast_overload;
                        wasSafelyCasted = ast->findCastOperator(currentScopeId, argType, signature->argumentTypes[i].typeId, &cast_overload);
                        Assert(cast_overload.astFunc && cast_overload.funcImpl);

                        int signature=0; // prevent mistakes
                        auto cast_sig = &cast_overload.funcImpl->signature;

                        int arg_space = cast_overload.funcImpl->signature.argSize;
                        // we must emit alloc args before we generate pushed values!
                        // builder.emit_alloc_args(BC_REG_INVALID, arg_space);
                        builder.fix_alloc_args(off_alloc_args, arg_space);
                        if(currentFuncImpl)
                            currentFuncImpl->update_max_arguments(arg_space);
                        Assert(cast_sig->argumentTypes.size() == 1);
                        generatePop_set_arg(cast_sig->argumentTypes[0].offset, cast_sig->argumentTypes[0].typeId);

                        int reloc;
                        builder.emit_call(cast_overload.astFunc->linkConvention, cast_overload.astFunc->callConvention, &reloc);
                        info.addCallToResolve(reloc, cast_overload.funcImpl);
                    
                        builder.emit_free_args(arg_space);

                        Assert(cast_sig->returnTypes.size() == 1);

                        auto &ret = cast_sig->returnTypes[0];
                        TypeId typeId = ret.typeId;
                        generatePush_get_val(ret.offset - cast_sig->returnSize, typeId);
                    }
                    if(!wasSafelyCasted && !info.hasAnyErrors()){
                        if(!is_function_pointer) {
                            ERR_SECTION(
                                ERR_HEAD2(arg->location)
                                ERR_MSG_COLORED("Cannot cast expression of type '" << log::LIME << info.ast->typeToString(argType) << log::NO_COLOR << "' to argument of type '" <<log::LIME<< info.ast->typeToString(signature->argumentTypes[i].typeId) << log::NO_COLOR<<"'. This is the function: '" << log::LIME<<astFunc->name<<log::NO_COLOR<<"'. (function may be an overloaded operator)")
                                ERR_LINE2(arg->location,"mismatching types")
                            )
                        } else {
                            ERR_SECTION(
                                ERR_HEAD2(arg->location)
                                ERR_MSG_COLORED("Cannot cast argument of type '" << log::LIME << info.ast->typeToString(argType) << log::NO_COLOR << "' to '" << log::LIME <<  info.ast->typeToString(signature->argumentTypes[i].typeId) << log::NO_COLOR << "'.")
                                ERR_LINE2(arg->location,"mismatching types")
                            )
                        }
                    }
                }
            }
        }
    }

    // operator overloads should always use BETCALL
    Assert(!isOperator || call_convention == BETCALL);

    if (call_convention == INTRINSIC) {
        // TODO: You could do some special optimisations when using intrinsics.
        //  If the arguments are strictly variables or constants then you can use a mov instruction 
        //  instead messing with push and pop.
        auto& name = funcImpl->astFunction->name;
        if(name == "prints"){
            builder.emit_pop(BC_REG_A);
            builder.emit_pop(BC_REG_B);
            
            builder.emit_opcode(BC_PRINTS);
            builder.emit_operand(BC_REG_A);
            builder.emit_operand(BC_REG_B);
        } else if(name == "printc"){
            builder.emit_pop(BC_REG_A);
            
            builder.emit_opcode(BC_PRINTC);
            builder.emit_operand(BC_REG_A);
        } else if(name == "memcpy"){
            builder.emit_pop(BC_REG_C);
            builder.emit_pop(BC_REG_B);
            builder.emit_pop(BC_REG_A);
            builder.emit_memcpy(BC_REG_A, BC_REG_B, BC_REG_C);
        } else if(name == "strlen"){
            builder.emit_pop(BC_REG_B);
            builder.emit_strlen(BC_REG_A, BC_REG_B);
            builder.emit_push(BC_REG_A); // len
            outTypeIds->add(TYPE_INT32);
        } else if(name == "memzero"){
            builder.emit_pop(BC_REG_B); // ptr
            builder.emit_pop(BC_REG_A);
            builder.emit_memzero(BC_REG_A, BC_REG_B, 0);
        } else if(name == "rdtsc"){
            builder.emit_rdtsc(BC_REG_A);
            builder.emit_push(BC_REG_A); // timestamp counter
            outTypeIds->add(TYPE_INT64);
        // } else if(funcImpl->name == "rdtscp"){
        //     builder.emit_({BC_RDTSC, BC_REG_A, BC_REG_ECX, BC_REG_RDX});
        //     builder.emit_push(BC_REG_A); // timestamp counter
        //     builder.emit_push(BC_REG_ECX); // processor thing?
            
        //     outTypeIds->add(TYPE_UINT64);
        //     outTypeIds->add(TYPE_UINT32);
        } else if(name == "atomic_compare_swap"){
            builder.emit_pop(BC_REG_C); // new
            builder.emit_pop(BC_REG_B); // old
            builder.emit_pop(BC_REG_A); // ptr
            builder.emit_atomic_cmp_swap(BC_REG_A, BC_REG_B, BC_REG_C);
            builder.emit_push(BC_REG_A);
            outTypeIds->add(TYPE_INT32);
        } else if(name == "atomic_add"){
            builder.emit_pop(BC_REG_B);
            builder.emit_pop(BC_REG_A);
            builder.emit_atomic_add(BC_REG_A, BC_REG_B, CONTROL_32B);
            builder.emit_push(BC_REG_A);
            outTypeIds->add(TYPE_INT32);
        } else if(name == "sqrt"){
            builder.emit_pop(BC_REG_A);
            builder.emit_sqrt(BC_REG_A);
            builder.emit_push(BC_REG_A);
            outTypeIds->add(TYPE_FLOAT32);
        } else if(name == "round"){
            builder.emit_pop(BC_REG_A);
            builder.emit_round(BC_REG_A);
            builder.emit_push(BC_REG_A);
            outTypeIds->add(TYPE_FLOAT32);
        } 
        // else if(funcImpl->name == "sin"){
        //     builder.emit_pop(BC_REG_A);
        //     builder.emit_({BC_SIN, BC_REG_A});
        //     builder.emit_push(BC_REG_A);
        //     outTypeIds->add(TYPE_FLOAT32);
        // } else if(funcImpl->name == "cos"){
        //     builder.emit_pop(BC_REG_A);
        //     builder.emit_({BC_COS, BC_REG_A});
        //     builder.emit_push(BC_REG_A);
        //     outTypeIds->add(TYPE_FLOAT32);
        // } else if(funcImpl->name == "tan"){
        //     builder.emit_pop(BC_REG_A);
        //     builder.emit_({BC_TAN, BC_REG_A});
        //     builder.emit_push(BC_REG_A);
        //     outTypeIds->add(TYPE_FLOAT32);
        // }
        else {
            ERR_SECTION(
                ERR_HEAD2(base_expression->location)
                ERR_MSG("'"<<name<<"' is not an intrinsic function.")
                ERR_LINE2(base_expression->location,"not an intrinsic")
            )
        }
        return SIGNAL_SUCCESS;
    }
    // TODO: It would be more efficient to do GeneratePop right after the argument expressions are generated instead
    //  of pushing them to the stack and then popping them here. You would save some pushing and popping
    //  The only difficulty is handling all the calling conventions. stdcall requires some more thought
    //  it's arguments should be put in registers and those are probably used when generating expressions. 
    for(int i=all_arguments.size()-1;i>=0;i--){
        auto arg = all_arguments[i];
        generatePop_set_arg(signature->argumentTypes[i].offset, signature->argumentTypes[i].typeId);
    }
    
    i32 reloc = 0;
    if(is_function_pointer) {
        BCRegister reg = BC_REG_B;
        auto expression = base_expression->as<ASTExpressionCall>();
        if (expression->isMemberCall()) {
            TypeId type{};
            bool was_reference = false;
            auto result = generateReference(expression->args[0], &type, currentScopeId, &was_reference);
            if(result == SIGNAL_SUCCESS) {
            } else {
                Assert(info.hasForeignErrors());
            }

            auto typeinfo = ast->getTypeInfo(type.baseType());
            auto mem = typeinfo->getMember(expression->name);

            builder.emit_pop(reg);
            if(type.getPointerLevel() > 0) {
                builder.emit_mov_rm(reg, reg, REGISTER_SIZE);
            }
            builder.emit_mov_rm_disp(reg, reg, REGISTER_SIZE, mem.offset);

            // log::out << log::CYAN << "PC: " << builder.get_pc() << "\n";
            // tinycode->print(0,-1, bytecode);
        } else {
            auto varinfo = expression->identifier->cast_var();

            auto version = currentPolyVersion;
            if(varinfo->is_import_global)
                version = 0;
            
            auto type_id = varinfo->versions_typeId[version];
            auto typeinfo = ast->getTypeInfo(type_id);
            Assert(typeinfo->funcType);
            
            switch(varinfo->type) {
                case Identifier::GLOBAL_VARIABLE: {
                    if(varinfo->declaration && varinfo->declaration->isImported()) {
                        generate_ext_dataptr(reg, varinfo);
                    } else {
                        emit_abstract_dataptr(reg, varinfo->versions_dataOffset[version], varinfo);
                    }
                    builder.emit_mov_rm(reg, reg, REGISTER_SIZE);
                } break; 
                case Identifier::LOCAL_VARIABLE: {
                    builder.emit_mov_rm_disp(reg, BC_REG_LOCALS, REGISTER_SIZE, varinfo->versions_dataOffset[info.currentPolyVersion]);
                } break;
                case Identifier::ARGUMENT_VARIABLE: {
                    builder.emit_get_param(reg, varinfo->versions_dataOffset[info.currentPolyVersion], REGISTER_SIZE, false);
                } break;
                case Identifier::MEMBER_VARIABLE: {
                    // NOTE: Is member variable/argument always at this offset with all calling conventions?
                    auto type = varinfo->versions_typeId[info.currentPolyVersion];
                
                    builder.emit_get_param(reg, 0, REGISTER_SIZE, false);

                    auto& mem = currentFunction->parentStruct->members[varinfo->memberIndex];
                    if (mem.array_length) {
                        ERR_SECTION(
                            ERR_HEAD2(expression->location)
                            ERR_MSG_COLORED("The identifier '"<<log::LIME << varinfo->name<<log::NO_COLOR<<"' is a member of the parent struct of a non-function pointer type. While it is an array of function pointers, it still an array and you must dereference or index into the array.")
                            ERR_LINE2(expression->location, "here")
                        )
                        // builder.emit_li32(BC_REG_A, varinfo->versions_dataOffset[info.currentPolyVersion]);
                        // builder.emit_add(BC_REG_B, BC_REG_A, false, REGISTER_SIZE);
                    } else {
                        builder.emit_mov_rm_disp(reg, reg, REGISTER_SIZE, varinfo->versions_dataOffset[info.currentPolyVersion]);
                    }
                    // builder.emit_get_param(BC_REG_B, 0, REGISTER_SIZE, false);
                    // generatePush(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion],
                        // varinfo->versions_typeId[info.currentPolyVersion]);
                } break;
                default: {
                    Assert(false);
                }
            }
        }
        
        // TODO: There should be no link convention
        builder.emit_call_reg(reg, LinkConvention::NONE, call_convention);
    } else if(astFunc->linkConvention == LinkConvention::NONE) {
            builder.emit_call(astFunc->linkConvention, astFunc->callConvention, &reloc);
            info.addCallToResolve(reloc, funcImpl);
    } else {
        // determine link convention
        LinkConvention link_convention = astFunc->linkConvention;

        std::string alias = astFunc->name;
        if (astFunc->linked_alias.size() != 0)
            alias = astFunc->linked_alias;

        std::string lib_path = "";
        auto func_imp = info.compiler->getImport(astFunc->getImportId(&info.compiler->lexer));
        Assert(func_imp);

        for(auto& lib : func_imp->libraries) {
            if(astFunc->linked_library == lib.named_as) {
                lib_path = lib.path;
                break;
            }
        }
        if (link_convention == LinkConvention::IMPORT && lib_path.size() != 0) {
            link_convention = DetermineLinkConvention(lib_path);
        }
        // log::out << "Try " << lib_path << " -> " << link_convention<<"\n";
        if (lib_path.size() == 0) {
            if(astFunc->linked_library.size() != 0) {
                // TODO: If many functions complain about GLAD then only display the first 5 or so.
                int errs = reporter->get_lib_errors(astFunc->linked_library);
                reporter->add_lib_error(astFunc->linked_library);
                if (errs >= Reporter::LIB_ERROR_LIMIT) {
                    // log::out << "skip\n";
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(astFunc->location)
                        ERR_MSG_COLORED("'"<<log::LIME<<astFunc->linked_library<<log::NO_COLOR<<"' is not a library. Did you misspell it?")
                        ERR_LINE2(astFunc->location, "this definition")
                        ERR_LINE2(base_expression->location, "this call")

                        log::out << "These are the available libraries: ";
                        for(int i=0;i<func_imp->libraries.size();i++){
                            if(i!=0) log::out << ", ";
                            log::out << log::LIME << func_imp->libraries[i].named_as << log::NO_COLOR;
                        }
                        log::out << "\n";
                    )
                }
            } else {
                // TOOD: Improve error message by showing how to specify import.
                //   Also link to the guide.md.
                // NOTE: The error message points to the function definition,
                //   not the function call because you need to fix the definition
                //   and not the call.
                ERR_SECTION(
                    ERR_HEAD2(astFunc->location)
                    ERR_MSG("You are trying to call an external function that isn't linked to a library. At the function definition, specify which library to link with.")
                    ERR_LINE2(astFunc->location, "this definition")
                    ERR_LINE2(base_expression->location, "this call")
                    ERR_EXAMPLE(1, "#load \"stuff.lib\" as Stuff\nfn @import(Stuff) DoStuff()")
                )
            }
        }
        if(link_convention == STATIC_IMPORT) {
            builder.emit_call(link_convention, astFunc->callConvention, &reloc, bytecode->externalRelocations.size());
            addExternalRelocation(alias, lib_path, reloc, BC_REL_FUNCTION);
        } else if(link_convention == DYNAMIC_IMPORT){
            builder.emit_call(link_convention, astFunc->callConvention, &reloc, bytecode->externalRelocations.size());
            if(compiler->options->target == TARGET_WINDOWS_x64) {
                // Windows has an import table of pointers and we
                // prefix with __imp_ to refer to that table.
                // Linux does not.
                alias = "__imp_" + alias;
            }
            addExternalRelocation(alias, lib_path, reloc, BC_REL_FUNCTION);
        } else {
            // NOTE: We emit call to prevent asserts and keep compiling since this link problem
            //   is a problem in x64.
            builder.emit_call(LinkConvention::NONE, astFunc->callConvention, &reloc);
            ERR_SECTION(
                ERR_HEAD2(astFunc->location)
                ERR_MSG_COLORED("Link convention (@import) for function could not be determined to @importdll or @importlib. This was the library name and path: '"<<log::LIME << astFunc->linked_library <<log::NO_COLOR<<"', '"<<log::LIME<<lib_path<<log::NO_COLOR<<"'. You can always specify @importdll or @importlib manually.")
                ERR_LINE2(astFunc->location,"function")
                ERR_LINE2(base_expression->location,"call")
            )
        }
    }
    
    builder.emit_free_args(allocated_stack_space);

    if (signature->returnTypes.size()==0) {
        outTypeIds->add(TYPE_VOID);
        return SIGNAL_SUCCESS;
    }

    if(call_convention != BETCALL) {
        // return type must fit in RAX for stdcall and unixcall
        if(signature->returnTypes.size() > 1) {
            // NOTE: This message isn't great because the default convention may change in the future and when it does I will forget to change this message.
            ERR_SECTION(
                ERR_HEAD2(base_expression->location)
                ERR_MSG_LOG(call_convention<<" cannot have more than 1 return type. Use @betcall for that (the default unless function has @import @export or you are targeting ARM).")
                ERR_LINE2(base_expression->location, "function call here")
            )
            return SIGNAL_FAILURE;
        }
        
        int ret_type_size = info.ast->getTypeSize(signature->returnTypes[0].typeId);
        if(ret_type_size > REGISTER_SIZE) {
            ERR_SECTION(
                ERR_HEAD2(base_expression->location)
                ERR_MSG_LOG(call_convention<<" cannot return a type larger than the register size (currently "<<REGISTER_SIZE<<" when targeting "<<compiler->options->target<<").")
                ERR_LINE2(base_expression->location, "function call here")
            )
            return SIGNAL_FAILURE;
        }
    }

    for (int i = 0; i< (int)signature->returnTypes.size(); i++) {
        auto &ret = signature->returnTypes[i];
        TypeId typeId = ret.typeId;

        // log::out << "ret "<<i<<" off: " << (ret.offset - signature->returnSize) << "\n";
        generatePush_get_val(ret.offset - signature->returnSize, typeId);
        outTypeIds->add(ret.typeId);
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePushedLiterals(TypeId type, char* stack, ASTExpression* expression, TypeInfo* structType, int memberIndex) {
    using namespace engone;
    TypeInfo* typeinfo = ast->getTypeInfo(type.baseType());
    if(AST::IsInteger(type) || AST::IsDecimal(type) || type == TYPE_BOOL || type == TYPE_CHAR || typeinfo->astEnum) {
        i64 value = 0;
        memcpy(&value, stack, REGISTER_SIZE);
        if(typeinfo->getSize() == 8) {
            builder.emit_li64(BC_REG_A, value);
            builder.emit_push(BC_REG_A);
        } else {
            builder.emit_li32(BC_REG_A, value);
            builder.emit_push(BC_REG_A);
        }
    } else if(type.getPointerLevel() == 1) {
        Assert(("TODO: handle 32 bit pointer in comp time on ARM", REGISTER_SIZE == 8));
        i64 value = 0;
        memcpy(&value, stack, REGISTER_SIZE);
        i64 data_start = (i64)bytecode->dataSegment.data();
        i64 data_end = (i64)bytecode->dataSegment.data() + bytecode->dataSegment.size();
        
        if(value == 0) {
            // null pointer
            builder.emit_li64(BC_REG_A, 0);
            builder.emit_push(BC_REG_A);
        } else if(value >= data_start && value < data_end) {
            // if pointer points to a data block
            int data_offset = value - (i64)bytecode->dataSegment.data();
            builder.emit_dataptr(BC_REG_A, data_offset);
            builder.emit_push(BC_REG_A);
        } else {
            if(structType) {
                // What about external data pointer? can we handle it?
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("Local run directive can only return pointers to the global data section. Type from this struct member: '"<<structType->name<<"."<<structType->astStruct->members[memberIndex].name<<"' . This was the resulting address: "<<log::LIME << NumberToHex(value, true) <<log::NO_COLOR<<", data section exists within this range: " <<log::LIME << NumberToHex(data_start, true) << " - " << NumberToHex(data_end, true) <<log::NO_COLOR <<" (exclusive). pointer - data_start = " << log::LIME << (value - data_start) <<log::NO_COLOR << " .")
                    ERR_LINE2(expression->location, ast->typeToString(type))
                )
            } else {
                // What about external data pointer? can we handle it?
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("Local run directive can only return pointers to the global data section. This was the resulting address: "<<log::LIME << NumberToHex(value, true) <<log::NO_COLOR<<", data section exists within this range: " <<log::LIME << NumberToHex(data_start, true) << " - " << NumberToHex(data_end, true) <<log::NO_COLOR <<" (exclusive). pointer - data_start = " << log::LIME << (value - data_start) <<log::NO_COLOR << " .")
                    ERR_LINE2(expression->location, ast->typeToString(type))
                )
            }
            return SIGNAL_FAILURE;
        }
    } else if(typeinfo->funcType) {
        Assert(("TODO: handle 32 bit function pointer in comp time on ARM", REGISTER_SIZE == 8));
        i64 value = 0;
        memcpy(&value, stack, REGISTER_SIZE);
        int tinycode_id = value; // see how virtual machine handles BC_CODEPTR and BC_CALL_REG
        int func_start = 1;
        int func_end = bytecode->tinyBytecodes.size() + 1;
        if(tinycode_id >= func_start && tinycode_id < func_end) {
            int reloc = builder.get_pc() + 2;
            builder.emit_codeptr(BC_REG_A, tinycode_id);
            builder.emit_push(BC_REG_A);
            if(compiler->options->target != TARGET_ARM) {
                info.addCallToResolve(reloc, bytecode->tinyBytecodes[tinycode_id-1]->funcImpl);
            }
        } else {
            if(structType) {
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("Local run directive returned a function pointer that didn't refer to a function within the program. "<<log::LIME << tinycode_id << log::NO_COLOR <<" was returned which wasn't within this range of function ids: " << log::LIME << func_start << log::NO_COLOR << " - " << log::LIME << func_end << log::NO_COLOR<<". Type from this struct member: '"<<structType->name<<"."<<structType->astStruct->members[memberIndex].name<<"'.")
                    ERR_LINE2(expression->location, ast->typeToString(type))
                )
            } else {
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("Local run directive returned a function pointer that didn't refer to a function within the program. "<<log::LIME << tinycode_id << log::NO_COLOR <<" was returned which wasn't within this range of function ids: " << log::LIME << func_start << log::NO_COLOR << " - " << log::LIME << func_end << log::NO_COLOR<<".")
                    ERR_LINE2(expression->location, ast->typeToString(type))
                )
            }
            return SIGNAL_FAILURE;
        }
    } else if(typeinfo->structImpl) {
        for(int i=typeinfo->structImpl->members.size()-1;i>=0;i--) {
            SignalIO result = generatePushedLiterals(typeinfo->structImpl->members[i].typeId, stack + i * REGISTER_SIZE, expression, typeinfo, i);
            if(result != SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;
        }
    } else {
        generateDefaultValue(BC_REG_INVALID, 0, type, &expression->location);
        ERR_SECTION(
            ERR_HEAD2(expression->location)
            ERR_MSG_COLORED("Local run directive can only return primitive types and structs. '"<<log::LIME << ast->typeToString(type) <<log::NO_COLOR<<"' is not allowed.")
            ERR_LINE2(expression->location, "here")
        )
        return SIGNAL_FAILURE;
    }
    return SIGNAL_SUCCESS;
};
SignalIO GenContext::generateExpression(ASTExpression *expression, TypeId *outTypeIds, ScopeId idScope) {
    *outTypeIds = TYPE_VOID;
    TEMP_ARRAY_N(TypeId, tempTypes, 5)
    auto signal = generateExpression(expression, &tempTypes, idScope);
    if(tempTypes.size()) *outTypeIds = tempTypes.last();
    return signal;
}
SignalIO GenContext::generateExpression(ASTExpression *base_expression, QuickArray<TypeId> *outTypeIds, ScopeId idScope) {
    using namespace engone;
    TRACE_FUNC()
    
    ZoneScopedC(tracy::Color::Blue2);
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;
    _GLOG(FUNC_ENTER)
    
    MAKE_NODE_SCOPE(base_expression);
    Assert(base_expression);

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    TEMP_ARRAY_N(TypeId, tempTypes, 5)

    SOURCE_TRACE(base_expression->location)
    
    // IMPORTANT: This DOES NOT work duudeeee. you must use poly versions
    // TypeId castType = expression->castType;
    // if(castType.isString()){
    //     castType = info.ast->convertToTypeId(castType,idScope);
    //     if(!castType.isValid()){
    //         ERR_SECTION(
// ERR_HEAD2(expression->location,"Type "<<info.ast->getTokenFromTypeString(expression->castType) << " does not exist.\n";)
    //     }
    // }
    // castType = info.ast->ensureNonVirtualId(castType);

    if(base_expression->computeWhenPossible && !inside_compile_time_execution) {
        // @nocheckin TODO: Polymorphic scope is not considered
        VirtualMachine vm{};
    
        ScopeId scopeId = currentScopeId;
        ASTExpression* expression = base_expression;

        CALLBACK_ON_ASSERT(
            ERR_SECTION(
                ERR_HEAD2(expression->location)
                ERR_MSG_LOG("Virtual machine failed when executing run directive. Call stack:\n")
                for(int i=0;i<vm.call_stack.size();i++) {
                    log::out << " " << vm.call_stack[i].func->name << "\n";
                }
                // TODO: Call stack
                ERR_LINE2(expression->location, "here")
            )
        )

        // TODO: Code below should be the same as the one in generateFunction.
        //   If we change the code in generateFunction but forget to here then
        //   we will be in trouble. So, how do we combine the code?

        BytecodeBuilder prev_builder = std::move(builder);

        auto temp_tinycode = compiler->get_temp_tinycode();

        auto prev_tinycode = tinycode;
        auto prev_currentScopeId = currentScopeId;
        auto prev_currentFrameOffset = currentFrameOffset;
        auto prev_currentScopeDepth = currentScopeDepth;
        auto prev_currentPolyVersion = currentPolyVersion;
        tinycode = temp_tinycode;
        new(&builder)BytecodeBuilder();
        builder.init(bytecode, tinycode, compiler);
        
        currentScopeId = ast->globalScopeId;
        currentFrameOffset = 0;
        currentScopeDepth = -1;
        currentPolyVersion = 0; // @TODO Should we reset poly version? Shouldn't we keep using the curreny one?

        TEMP_ARRAY_N(TypeId, tempTypes, 5)
        expression->computeWhenPossible = false; // temporarily disable to preven infinite loop
        Assert(!inside_compile_time_execution);
        inside_compile_time_execution = true;
        auto result = generateExpression(expression, &tempTypes, 0);
        inside_compile_time_execution = false;
        expression->computeWhenPossible = true;
        
        builder.~BytecodeBuilder();
        builder = std::move(prev_builder);
        
        tinycode = prev_tinycode;
        currentScopeId = prev_currentScopeId;
        currentFrameOffset = prev_currentFrameOffset;
        currentScopeDepth = prev_currentScopeDepth;
        currentPolyVersion = prev_currentPolyVersion;
        
        if(outTypeIds && tempTypes.size())
            outTypeIds->add(tempTypes[0]);
        
        // TODO: We generate expression with from global scope so that we can't access local variables but what about constant functions? There may be more issues?
        if (result != SIGNAL_SUCCESS) {
            // We should have printed an error
            Assert(info.hasAnyErrors());
            return SIGNAL_FAILURE;
        }
        
        if (tempTypes.size() > 1) {
            ERR_SECTION(
                ERR_HEAD2(expression->location)
                ERR_MSG("Expression for run directives cannot produce more than one value.")
                ERR_LINE2(expression->location, "here")
            )
            return SIGNAL_FAILURE;
        }

        // log::out << log::GOLD <<"global: " <<stmt->varnames[0].name << "\n";
        // tinycode->print(0,-1,bytecode);

        vm.silent = true;
        vm.init_stack();
        if(REGISTER_SIZE == 4) {
            void* ptr_to_global_data = bytecode->dataSegment.data();
            u32 mem = 0x1000'0000;
            u32 mem_size = bytecode->dataSegment.size();
            vm.add_memory_mapping(mem, (u64)ptr_to_global_data, mem_size);
        }
        // let VM evaluate expression and put into global data
        vm.execute(bytecode, temp_tinycode->name, true);
        
        POP_LAST_CALLBACK()
        
        if(vm.error.type != VM_ERROR_NONE) {
            printVMFailedMessage(vm, expression->location);
            return SIGNAL_FAILURE;
        }
        
        if(tempTypes.size() != 0 && tempTypes[0] != TYPE_VOID) {
            TypeId type = tempTypes[0];
            SignalIO result = generatePushedLiterals(type, (char*)vm.stack_pointer, expression);
            return result;
        }
        return SIGNAL_SUCCESS;
    }

    if (base_expression->type == EXPR_VALUE) {
        // data type
        auto expression = base_expression->as<ASTExpressionValue>();
        if (AST::IsInteger(expression->typeId)) {
            i64 val = expression->i64Value;
            // Assert(!(val&0xFFFFFFFF00000000));

            // TODO: immediate only allows 32 bits. What about larger values?
            // bytecode->addDebugText("  expr push int");
            // TOKENINFO(expression->location)

            // TODO: Int types should come from global scope. Is it a correct assumption?
            // TypeInfo *typeInfo = info.ast->getTypeInfo(expression->typeId);
            u32 size = info.ast->getTypeSize(expression->typeId);
            BCRegister reg = BC_REG_A;
            if(size == 8) {
                builder.emit_li64(reg, val);
            } else {
                builder.emit_li32(reg, val);
            }
            builder.emit_push(reg);
        }
        else if (expression->typeId == TYPE_BOOL) {
            bool val = expression->boolValue;

            // TOKENINFO(expression->location)
            // bytecode->addDebugText("  expr push bool");
            builder.emit_li32(BC_REG_A, val);
            builder.emit_push(BC_REG_A);
        }
        else if (expression->typeId == TYPE_CHAR) {
            char val = expression->charValue;

            // TOKENINFO(expression->location)
            // bytecode->addDebugText("  expr push char");
            builder.emit_li32(BC_REG_A, val);
            builder.emit_push(BC_REG_A);
        }
        else if (expression->typeId == TYPE_FLOAT32) {
            float val = expression->f32Value;

            // TOKENINFO(expression->location)
            // bytecode->addDebugText("  expr push float");
            builder.emit_li32(BC_REG_A, *(u32*)&val);
            builder.emit_push(BC_REG_A);
        }
        else if (expression->typeId == TYPE_FLOAT64) {
            double val = expression->f64Value;

            // TOKENINFO(expression->location)
            // bytecode->addDebugText("  expr push float");
            builder.emit_li64(BC_REG_A, *(u64*)&val);
            builder.emit_push(BC_REG_A);
        }
        outTypeIds->add( expression->typeId);
        return SIGNAL_SUCCESS;
    } else if (base_expression->type == EXPR_IDENTIFIER) {
        auto expression = base_expression->as<ASTExpressionIdentifier>();
        // NOTE: HELLO! i commented out the code below because i thought it was strange and out of place.
        //   It might be important but I just don't know why.
        // NOTE: Yes it was important past me. AST_ID and variables have simular syntax.
        // NOTE: Hello again! I am commenting the enum code again because I don't think it's necessary with the new changes.
        
        // TypeInfo *typeInfo = info.ast->convertToTypeInfo(expression->name, idScope, true);
        // // A simple check to see if the identifier in the expr node is an enum type.
        // // no need to check for pointers or so.
        // if (typeInfo && typeInfo->astEnum) {
        //     Assert(false);
        //     outTypeIds->add(typeInfo->id);
        //     return SIGNAL_SUCCESS;
        // }

        if(expression->enum_ast) {
            auto& mem = expression->enum_ast->members[expression->enum_member];
            int size = info.ast->getTypeSize(expression->enum_ast->typeId);
            Assert(size <= REGISTER_SIZE);
            
            if(size > 4) {
                Assert(sizeof(mem.enumValue) == size); // bug in parser/AST
                builder.emit_li64(BC_REG_A, mem.enumValue);
                builder.emit_push(BC_REG_A);
            } else {
                builder.emit_li32(BC_REG_A, mem.enumValue);
                builder.emit_push(BC_REG_A);
            }

            outTypeIds->add(expression->enum_ast->typeId);
            return SIGNAL_SUCCESS;
        }

        // check data type and get it
        // auto id = info.ast->findIdentifier(idScope, , expression->name);
        auto id = expression->identifier;
        if (id) {
            if (id->is_var()) {
                auto varinfo = id->cast_var();
                auto version = currentPolyVersion;
                if (varinfo->is_import_global)
                    version = 0;
                auto type = varinfo->versions_typeId[version];
                if(!type.isValid()){
                    return SIGNAL_FAILURE;
                }
                // auto var = info.ast->getVariableByIdentifier(id);
                // TODO: check data type?
                // fp + offset
                // TODO: what about struct
                _GLOG(log::out << " expr var push " << expression->name << "\n";)

                // char buf[100];
                // int len = sprintf(buf,"  expr push %s",expression->name->c_str());
                // bytecode->addDebugText(buf,len);
                // log::out << "AST_VAR: "<<id->name<<", "<<id->varIndex<<", "<<var->frameOffset<<"\n";
                    
                switch(varinfo->type) {
                    case Identifier::GLOBAL_VARIABLE: {
                        if(varinfo->declaration && varinfo->declaration->isImported()) {
                            generate_ext_dataptr(BC_REG_B, varinfo);
                        } else {
                            emit_abstract_dataptr(BC_REG_B, varinfo->versions_dataOffset[version], varinfo);
                        }
                        generatePush(BC_REG_B, 0, type);
                    } break; 
                    case Identifier::LOCAL_VARIABLE: {
                        // generatePush(BC_REG_LP, varinfo->versions_dataOffset[info.currentPolyVersion],
                        //     varinfo->versions_typeId[info.currentPolyVersion]);
                        generatePush(BC_REG_LOCALS, varinfo->versions_dataOffset[info.currentPolyVersion],
                            varinfo->versions_typeId[info.currentPolyVersion]);
                    } break;
                    case Identifier::ARGUMENT_VARIABLE: {
                        generatePush_get_param(varinfo->versions_dataOffset[info.currentPolyVersion],
                            varinfo->versions_typeId[info.currentPolyVersion]);
                    } break;
                    case Identifier::MEMBER_VARIABLE: {
                        // NOTE: Is member variable/argument 'this' always at this offset with all calling conventions?
                        // type = varinfo->versions_typeId[info.currentPolyVersion];
                        builder.emit_get_param(BC_REG_B, 0, REGISTER_SIZE, false); // pointer
                        if(currentFunction->parentStruct) {
                            auto& mem = currentFunction->parentStruct->members[varinfo->memberIndex];
                            if (mem.array_length) {
                                type.setPointerLevel(type.getPointerLevel()+1);
                                builder.emit_li32(BC_REG_T0, varinfo->versions_dataOffset[info.currentPolyVersion]);
                                builder.emit_add(BC_REG_B, BC_REG_T0, REGISTER_SIZE, false);
                                builder.emit_push(BC_REG_B);
                            } else {
                                generatePush(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion],
                                type);
                            }
                        } else {
                            // This happend because of @TEST_ERROR in tests/funcs/stuff.btb at one point.
                        }
                    } break;
                    default: {
                        Assert(false);
                    }
                }

                outTypeIds->add(type);
                if(outTypeIds->last() == TYPE_VOID)
                    return SIGNAL_FAILURE;
                return SIGNAL_SUCCESS;
            } else if (id->type == Identifier::FUNCTION) {
                _GLOG(log::out << " expr func push " << expression->name << "\n";)

                auto fun = id->cast_fn();
                // TODO: Feature to take function pointers of imported functions
                if(fun->funcOverloads.overloads.size()==1){
                    
                    int reloc = builder.get_pc() + 2;
                    builder.emit_codeptr(BC_REG_A, fun->funcOverloads.overloads[0].funcImpl->tinycode_id);
                    // @TODO: We're being incosistent here where arm_gen adds it's own relocation from the BC_CODEPTR while x64_gen doesn't. arm_gen needs to because relocation for a function address on arm is different from a relocation for a normal function call. Clean this up.
                    if(compiler->options->target != TARGET_ARM) {
                        info.addCallToResolve(reloc, fun->funcOverloads.overloads[0].funcImpl);
                    }
                    // export function when debugging, nice to see if objdump refer to the function we expect
                    // TODO: only add if unique
                    // TODO: Can't add if function wasn't generated, how do we fix that?
                    if(fun->funcOverloads.overloads[0].funcImpl->tinycode_id-1 != -1) {
                        bytecode->addExportedFunction(expression->name, fun->funcOverloads.overloads[0].funcImpl->tinycode_id-1);
                    }
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(expression->location)
                        ERR_MSG("You can only refer to functions with one overload. '"<<expression->name<<"' has "<<fun->funcOverloads.overloads.size()<<".")
                        ERR_LINE2(expression->location,"cannot refer to function")
                    )
                }

                builder.emit_push(BC_REG_A);

                DynamicArray<TypeId> args{};
                DynamicArray<TypeId> rets{};
                auto f = fun->funcOverloads.overloads[0].funcImpl;
                args.reserve(f->signature.argumentTypes.size());
                rets.reserve(f->signature.returnTypes.size());
                for(auto& t : f->signature.argumentTypes)
                    args.add(t.typeId);
                for(auto& t : f->signature.returnTypes)
                    rets.add(t.typeId);
                auto type = ast->findOrAddFunctionSignature(args, rets, f->signature.convention);
                if(!type) {
                    Assert(("can this crash?",false));
                } else {
                    outTypeIds->add(type->id);
                }

                return SIGNAL_SUCCESS;
            } else {
                INCOMPLETE
            }
        } else {
            if(!info.hasForeignErrors()) {
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("Compiler bug! '"<<expression->name<<"' is not declared (expression->identifier was null). See trace.")
                    // ERR_LINE2(expression->location,"undeclared")
                    for(auto& loc : source_trace) {
                        ERR_LINE2(loc,"")
                    }
                )
            }
            return SIGNAL_FAILURE;
        }
    } else if (base_expression->type == EXPR_CALL) {
        auto expression = base_expression->as<ASTExpressionCall>();
        return generateFncall(expression, outTypeIds, false);
    } else if(base_expression->type == EXPR_STRING){
        auto expression = base_expression->as<ASTExpressionString>();
        // Assert(expression->constStrIndex!=-1);
        int& constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
        auto& constString = info.ast->getConstString(constIndex);
        
        TypeInfo* typeInfo = nullptr;
        if (expression->flags & ASTNode::NULL_TERMINATED) {
            typeInfo = info.ast->convertToTypeInfo("char*", info.ast->globalScopeId, true);
            Assert(typeInfo);
            // NOTE: Literal strings are accessed from global data section, not the stable global data if we have it, meaning, we don't use emit_abstract_dataptr here!
            builder.emit_dataptr(BC_REG_B, constString.address);
            builder.emit_push(BC_REG_B);
            TypeId ti = TYPE_CHAR;
            ti.setPointerLevel(1);
            outTypeIds->add(ti);
        } else {
            typeInfo = info.ast->convertToTypeInfo("Slice<char>", info.ast->globalScopeId, true);
            if(!typeInfo){
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG(""<<compiler->lexer.tostring(expression->location)<<" cannot be converted to Slice<char> because Slice doesn't exist. Slice is defined in <preload> and this is probably a bug in the compiler. Unless you disabled preload?")
                )
                return SIGNAL_FAILURE;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].offset == 0);
            // Assert(typeInfo->structImpl->members[1].offset == 8);// len: u64
            // Assert(typeInfo->structImpl->members[1].offset == 12); // len: u32
            // last member in slice is pushed first
            if(typeInfo->structImpl->members[1].offset == 8){
                builder.emit_li32(BC_REG_A,constString.length);
                builder.emit_push(BC_REG_A);
            } else {
                builder.emit_li32(BC_REG_A,constString.length);
                builder.emit_push(BC_REG_A);
            }

            // first member is pushed last
            // NOTE: Literal strings don't use emit_abstract_dataptr
            builder.emit_dataptr(BC_REG_B, constString.address);
            builder.emit_push(BC_REG_B);
            outTypeIds->add(typeInfo->id);
        }
        return SIGNAL_SUCCESS;
    } else if(base_expression->type == EXPR_BUILTIN) {
        auto expression = base_expression->as<ASTExpressionBuiltin>();
        if(expression->builtin_type == AST_SIZEOF){

            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            TypeId typeId = expression->versions_outTypeSizeof[info.currentPolyVersion];
            int size = info.ast->getTypeSize(typeId);
            if (typeId == TYPE_VOID)
                size = -1;
            if (size < 0) {
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("Type '"<<log::LIME<<ast->typeToString(typeId)<<log::NO_COLOR<<"' is not valid.")
                    ERR_LINE2(expression->location, "here")
                )   
            }

            builder.emit_li32(BC_REG_A, size);
            builder.emit_push(BC_REG_A);

            outTypeIds->add(TYPE_INT32);
            return SIGNAL_SUCCESS;
        } else if(expression->builtin_type == AST_TYPEOF){

            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            TypeId result_typeId = expression->versions_outTypeTypeid[info.currentPolyVersion];

            const char* tname = "lang_TypeId";
            // TypeInfo* typeInfo = info.ast->convertToTypeInfo(tname, info.ast->globalScopeId, true);
            TypeInfo* typeInfo = info.ast->convertToTypeInfo(tname, info.currentScopeId, true);
            if(!typeInfo){
                // ERR_SECTION(
                //     ERR_HEAD2(expr->location)
                //     ERR_MSG("'"<<tname << "' was not a valid type. Did you forget to #import \"Lang\".")
                //     ERR_LINE2(expr->location, "bad")
                // )

                // type checker should have complained
                Assert(hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            Assert(typeInfo->getSize() == 4);

            // NOTE: This is scuffed, could be a bug in the future
            // lang::TypeId tmp={};
            // tmp.index0 = result_typeId._infoIndex0;
            // tmp.index1 = result_typeId._infoIndex1;
            // tmp.ptr_level = result_typeId.pointer_level;

            // builder.emit_li32(BC_REG_A, *(u32*)&tmp);
            // builder.emit_push(BC_REG_A);
            
            // NOTE: Struct members are pushed in the opposite order
            builder.emit_li32(BC_REG_A, result_typeId.pointer_level);
            builder.emit_push(BC_REG_A);

            builder.emit_li32(BC_REG_A, result_typeId._infoIndex1);
            builder.emit_push(BC_REG_A);

            builder.emit_li32(BC_REG_A, result_typeId._infoIndex0);
            builder.emit_push(BC_REG_A);

            outTypeIds->add(typeInfo->id);
            return SIGNAL_SUCCESS;
        } else if(expression->builtin_type == AST_NAMEOF){
            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid());

            // std::string name = info.ast->typeToString(typeId);

            // Assert(expression->constStrIndex!=-1);
            int constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
            auto& constString = info.ast->getConstString(constIndex);

            TypeInfo* typeInfo = info.ast->convertToTypeInfo("Slice<char>", info.ast->globalScopeId, true);
            if(!typeInfo){
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("'"<<compiler->lexer.tostring(expression->location)<<"' cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"")
                )
                return SIGNAL_FAILURE;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].offset == 0);
            // Assert(typeInfo->structImpl->members[1].offset == 8);// len: u64
            // Assert(typeInfo->structImpl->members[1].offset == 12); // len: u32

            // last member in slice is pushed first
            if(typeInfo->structImpl->members[1].offset == 8){
                builder.emit_li32(BC_REG_A, constString.length);
                builder.emit_push(BC_REG_A);
            } else {
                builder.emit_li32(BC_REG_A, constString.length);
                builder.emit_push(BC_REG_A);
            }

            // first member is pushed last
            // NOTE: Literal strings don't use emit_abstract_dataptr
            builder.emit_dataptr(BC_REG_B, constString.address);
            builder.emit_push(BC_REG_B);

            outTypeIds->add(typeInfo->id);
            return SIGNAL_SUCCESS;
        }
    }  else if(base_expression->type == EXPR_ASSEMBLY){
        auto expression = base_expression->as<ASTExpressionAssembly>();
        auto type = expression->versions_asmType[info.currentPolyVersion];
        
        int off = builder.get_virtual_sp();
        for(auto a : expression->args) {
            tempTypes.resize(0);
            auto signal = generateExpression(a, &tempTypes);
            if(signal != SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;
        }
        int input_count = -(builder.get_virtual_sp() - off) / REGISTER_SIZE;
        
        if(info.disableCodeGeneration) {
            for(int i=0;i<input_count;i++) {
                builder.emit_fake_pop();
            }
            if(type.isValid()) {
                SignalIO result = generateArtificialPush(expression->versions_asmType[info.currentPolyVersion]);
            }
            outTypeIds->add(type);
            return SIGNAL_SUCCESS;   
        }
        
        if(info.currentPolyVersion != 0) {
            // TODO: Don't add the same assembly to bytecode unless they are different.
            // log::out << log::GOLD << "Inline assembly in polymorphic functions will be stored twice which is unnecessary.\n";
        }

        auto imp = compiler->lexer.getImport_unsafe(expression->asm_range.importId);
        auto iterator = compiler->lexer.createFeedIterator(imp, expression->asm_range.token_index_start, expression->asm_range.token_index_end);
        bool yes = compiler->lexer.feed(iterator);
        if(!yes) {
            // TODO: When does this happen and is it an error?
        }
        // TODO: Optimize
        // ############################
        //   Find referenced variables
        // ############################
        std::string assembly = std::string(iterator.data(), iterator.len());
        std::string new_assembly = "";
        std::string temp = "";
        int ln = 1;
        int head = 0;
        int bracket_depth = 0;
        int name_start = 0;
        int name_end = 0;
        enum State {
            PARSE_NONE,
            PARSE_NAME,
            PARSE_REST,
        };
        State state = PARSE_NONE;
        
        // #define IS_WHITESPACE(C) (C == ' ' || C == '\n' || C == '\r' || C == '\t')
        #define IS_WHITESPACE(C) (C == ' ' || C == '\t')
        
        new_assembly = assembly;
        // I think I overcomplicated this parsing. We need to find "[some_name]" and if any characters doesn't match
        // that then we find the next opening bracket.
    #ifdef gone
        while(head < assembly.size()) {
            char chr = assembly[head];
            head++;
            
            if(chr == '\n') {
                ln++;
            }
            
            if(chr == '[') {
                bracket_depth++;
            } else if(chr == ']') {
                bracket_depth--;
            }
            
            switch(state) {
                case PARSE_NONE: {
                    if(chr == '[') {
                        state = PARSE_NAME;
                        name_start = 0;
                        name_end = 0;
                        temp.clear();
                    }
                    new_assembly += chr;
                } break;
                case PARSE_NAME: {
                    if(chr == ']') {
                        if(bracket_depth == 0){
                            if(name_end == 0) {
                                name_end = head-1;
                            }
                            const char* reg_names[] {
                                "rsp",
                                "rbp",
                                "rax",
                                "rbx",
                                "rcx",
                                "rdx",
                                "rdi",
                                "rsi",
                                "r8",
                                "r9",
                                "r10",
                                "r11",
                                "r12",
                                "r13",
                                "r14",
                                "r15",
                            };
                            bool is_reg = false;
                            for(int i=0;i<sizeof(reg_names)/sizeof(*reg_names);i++) {
                                if(temp == reg_names[i]) {
                                    is_reg = true;
                                    break;   
                                }
                            }
                            if(!is_reg) {
                                // ############################
                                //   We found a variable name
                                // ############################
                                std::string name = std::string(assembly.data() + name_start, name_end-name_start);
                                // log::out << "Var " << name<<"\n";
                                
                                ContentOrder order = CONTENT_ORDER_MAX; // TODO: Fix content order
                                bool crossed_function = false;
                                auto iden = ast->findIdentifier(idScope, order, name, &crossed_function);
                                if(iden){
                                    if(iden->is_var()){
                                        auto varinfo = iden->cast_var();

                                        if(crossed_function && varinfo->type != Identifier::GLOBAL_VARIABLE) {
                                            ERR_SECTION(
                                                ERR_HEAD2(expression->location)
                                                ERR_MSG("Undefined variable '"<<name<<"' in inline assembly.")
                                                ERR_LINE2(expression->location, "somewhere in here")
                                            )
                                        }
                                        
                                        switch(varinfo->type) {
                                            case Identifier::MEMBER_VARIABLE: {
                                                    ERR_SECTION(
                                                    ERR_HEAD2(expression->location)
                                                    ERR_MSG("Inline assembly cannot access members of methods (not implemented yet).")
                                                    ERR_LINE2(expression->location, "somewhere in here")
                                                )
                                                // The problem is that we need a double dereference
                                                new_assembly += "rbp+16]\n"; // access argument 'this'
                                                // then something like this to access the member
                                                new_assembly += "mov eax, [rax+" + std::to_string(varinfo->versions_dataOffset[currentPolyVersion]);
                                                // We have to parse the register that was just before the [var] part.
                                                // This assumes that [var] was just with mov.
                                                // Also, a user is writing assembly because they want control,
                                                // and now were adding extra instructions.
                                                // Referencing variables like this is optional so I guess it's fine.
                                            } break;
                                            case Identifier::ARGUMENT_VARIABLE: {
                                                // TODO: User may write [num + 8] which we want to allow.
                                                new_assembly += "rbp+"+std::to_string(varinfo->versions_dataOffset[currentPolyVersion]);
                                            } break;
                                            case Identifier::GLOBAL_VARIABLE: {
                                                ERR_SECTION(
                                                    ERR_HEAD2(expression->location)
                                                    ERR_MSG("Inline assembly cannot access global variables (not implemented yet).")
                                                    ERR_LINE2(expression->location, "somewhere in here")
                                                )
                                            } break; 
                                            case Identifier::LOCAL_VARIABLE: {
                                                    ERR_SECTION(
                                                    ERR_HEAD2(expression->location)
                                                    ERR_MSG("Inline assembly cannot access local variables (not implemented yet).")
                                                    ERR_LINE2(expression->location, "somewhere in here")
                                                )
                                                // new_assembly += "______";
                                            } break;
                                            default: Assert(false);
                                        }
                                    } else {
                                        ERR_SECTION(
                                            ERR_HEAD2(expression->location)
                                            ERR_MSG("Inline assembly cannot only refer to variables: mov eax [var_name] not functions: mov rax, [func_name].")
                                            ERR_LINE2(expression->location, "somewhere in here")
                                        )
                                    }
                                } else {

                                    ERR_SECTION(
                                        ERR_HEAD2(expression->location)
                                        ERR_MSG("Undefined variable '"<<name<<"' in inline assembly.")
                                        ERR_LINE2(expression->location, "somewhere in here")
                                    )
                                }
                            } else {
                                new_assembly += temp;
                            }
                            new_assembly += chr;
                            state = PARSE_NONE;
                        }
                    } else if(chr == '[') {
                        temp += chr;
                        state = PARSE_REST;
                    } else {
                        if(name_start == name_end) {
                            if(((chr|32) >= 'a' && (chr|32) <= 'z') || chr == '_') {
                                name_start = head - 1;
                                name_end = 0;
                                temp += chr;
                            } else if(IS_WHITESPACE(chr)) {
                                temp += chr;
                            } else {
                                state = PARSE_REST;
                                temp += chr;
                            }
                        } else {
                            if(IS_WHITESPACE(chr)) {
                                if(name_end == 0) {
                                    name_end = head-1;
                                }
                                temp += chr;
                            } else if(name_end == 0) {
                                if(((chr|32) >= 'a' && (chr|32) <= 'z') || (chr >= '0' && chr <= '9') || chr == '_') {
                                    temp += chr;
                                } else {
                                    state = PARSE_REST;
                                    temp += chr;
                                }
                            } else {
                                state = PARSE_REST;
                                temp += chr;
                            }
                        }
                    }
                } break;
                case PARSE_REST: {
                    temp += chr;
                    if(chr == ']') {
                        if(bracket_depth == 0){
                            new_assembly += temp;
                            state = PARSE_NONE;
                        }
                    }
                } break;
                default: Assert(false);
            }
        }
    #endif

        lexer::TokenSource* src_start, *src_end;
        compiler->lexer.getTokenInfoFromImport(imp->file_id, expression->asm_range.token_index_start, &src_start);
        compiler->lexer.getTokenInfoFromImport(imp->file_id, expression->asm_range.token_index_end-1, &src_end);
        iterator.append('\n'); // if the last token didn't have a newline then we must add one here
        int asm_index = bytecode->add_assembly(new_assembly.data(), new_assembly.size(), imp->path, src_start->line, src_end->line);
        
        for(int i=0;i<input_count;i++) {
            builder.emit_fake_pop();
        }
        
        builder.emit_asm(asm_index, input_count, type != TYPE_VOID);
        
        if(type.isValid()) {
            SignalIO result = generateArtificialPush(expression->versions_asmType[info.currentPolyVersion]);
        }

        outTypeIds->add(type);
        return SIGNAL_SUCCESS;
    }  else if (base_expression->type == EXPR_NULL) {
        // TODO: Move into the type checker?
        // bytecode->addDebugText("  expr push null");
        builder.emit_li64(BC_REG_A, 0);
        builder.emit_push(BC_REG_A);

        TypeInfo *typeInfo = info.ast->convertToTypeInfo("void", info.ast->globalScopeId, true);
        TypeId newId = typeInfo->id;
        newId.setPointerLevel(1);
        outTypeIds->add(newId);
        return SIGNAL_SUCCESS;
        // } else {
        //     auto typeName = info.ast->typeToString(expression->typeId);
        //     // builder.emit_({BC_PUSH,BC_REG_A}); // push something so the stack stays synchronized, or maybe not?
        //     ERR_SECTION(
        //         ERR_HEAD2(expression->location)
        //         ERR_MSG("'" <<typeName << "' is an unknown data type.")
        //         ERR_LINE2(expression->location, typeName)
        //     )
        //     // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
        //     outTypeIds->add( TYPE_VOID);
        //     return SIGNAL_FAILURE;
        // }
    } else if (base_expression->type == EXPR_CAST) {
        auto expression = base_expression->as<ASTExpressionCast>();
        tempTypes.clear();
        TypeId ltype{};
        SignalIO result = generateExpression(expression->left, &tempTypes);
        if(tempTypes.size())
            ltype = tempTypes.last();
        if (result != SIGNAL_SUCCESS)
            return result;

        // TypeInfo *ti = info.ast->getTypeInfo(ltype);
        // TypeInfo *tic = info.ast->getTypeInfo(castType);
        u32 typeSize = info.ast->getTypeSize(ltype);
        TypeId castType = expression->versions_castType[info.currentPolyVersion];
        u32 castSize = info.ast->getTypeSize(castType);
        BCRegister lreg = BC_REG_A;
        BCRegister creg = BC_REG_A;
        if(expression->isUnsafeCast()) {
            // Is there a reason we should pop and push?
            // builder.emit_pop(lreg);
            // builder.emit_push(creg);
            outTypeIds->add(castType);
        } else {
            // if ((castType.isPointer() && ltype.isPointer()) || (castType.isPointer() && (ltype == (TypeId)TYPE_INT64 ||
            //     ltype == (TypeId)TYPE_UINT64 || ltype == (TypeId)TYPE_INT32)) || ((castType == (TypeId)TYPE_INT64 ||
            //     castType == (TypeId)TYPE_UINT64 || ltype == (TypeId)TYPE_INT32) && ltype.isPointer())) {
            //     builder.emit_pop(lreg);
            //     builder.emit_push(creg);
            //     outTypeIds->add(castType);
            //     // data is fine as it is, just change the data type
            // } else { 
            bool yes = (ltype.getPointerLevel() > 0 && castType.getPointerLevel() > 0) || performSafeCast(ltype, castType, true);
            if (yes) {
                outTypeIds->add(castType);
            } else {
                Assert(info.hasForeignErrors());
                
                outTypeIds->add(ltype); // ltype since cast failed
                return SIGNAL_FAILURE;
            }
            // }
        }
        return SIGNAL_SUCCESS;
    } else if (base_expression->type == EXPR_MEMBER) {
        auto expression = base_expression->as<ASTExpressionMember>();
        // TODO: if propfrom is a variable we can directly take the member from it
        //  instead of pushing struct to the stack and then getting it.

        // bytecode->addDebugText("ast-member expr\n");
        TypeId exprId;

        if(expression->left->type == EXPR_IDENTIFIER){
            auto id_expr = expression->left->as<ASTExpressionIdentifier>();
            TypeInfo *typeInfo = info.ast->convertToTypeInfo(id_expr->name, idScope, true);
            // A simple check to see if the identifier in the expr node is an enum type.
            // no need to check for pointers or so.
            if (typeInfo && typeInfo->astEnum) {
                // SAMECODE as when checking AST_ID further up
                i32 enumValue;
                bool found = typeInfo->astEnum->getMember(expression->name, &enumValue);
                if (!found) {
                    Assert(info.hasForeignErrors());
                    // ERR_SECTION(
                    // ERR_HEAD2(expression->location, expression->tokenRange.firstToken << " is not a member of enum " << typeInfo->astEnum->name << "\n";
                    // )
                    return SIGNAL_FAILURE;
                }

                builder.emit_li32(BC_REG_A, enumValue);
                builder.emit_push(BC_REG_A);

                outTypeIds->add(typeInfo->id);
                return SIGNAL_SUCCESS;
            }
        }
        
        SignalIO result = SIGNAL_NO_MATCH;
        
        bool nonReference = false;
        int array_length = 0;
        result = generateReference(expression->left, &exprId, idScope, &nonReference, &array_length);
        if(result != SIGNAL_SUCCESS) {
            return SIGNAL_FAILURE;
        }
        
        if(array_length > 0) {
            Assert(nonReference); // we expect a pure pointer out
            if(expression->name == "len") {
                builder.emit_pop(BC_REG_T0); // pop pointer
                builder.emit_li64(BC_REG_T0, array_length);
                builder.emit_push(BC_REG_T0);
                if(outTypeIds)
                    outTypeIds->add(TYPE_INT32);
            } else if(expression->name == "ptr") {
                if(outTypeIds)
                    outTypeIds->add(exprId);
            } else {
                // Assert(hasForeignErrors());
                // type checker handles this
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("'"<<info.ast->typeToString(exprId)<<"' is an array within a struct and does not have members. If the elements of the array are structs then index into the array first.")
                    ERR_LINE2(expression->left->location, info.ast->typeToString(exprId).c_str())
                )
                return SIGNAL_FAILURE;
            }
        } else {
            if(exprId.getPointerLevel() > 1){ // one level of pointer is okay.
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("'"<<info.ast->typeToString(exprId)<<"' is not a struct, cannot access members. A pointer level of one will be implicitly dereferenced. However, the pointer level was "<<exprId.pointer_level<<"had a level.")
                    ERR_LINE2(expression->left->location, info.ast->typeToString(exprId).c_str())
                )
                return SIGNAL_FAILURE;
            }
            auto typeInfo = info.ast->getTypeInfo(exprId.baseType());
            if(!typeInfo || !typeInfo->astStruct){
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("'"<<info.ast->typeToString(exprId)<<"' is not a struct. Cannot access member.")
                    ERR_LINE2(expression->left->location, info.ast->typeToString(exprId).c_str())
                )
                return SIGNAL_FAILURE;
            }
            auto memberData = typeInfo->getMember(expression->name);
            if(memberData.index==-1){
                Assert(info.hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            auto& mem = typeInfo->astStruct->members[memberData.index];
            if(mem.array_length > 0) {
                
                auto type = memberData.typeId;
                type.setPointerLevel(type.getPointerLevel() + 1);
                
                bool popped = false;
                BCRegister reg = BC_REG_B;
                if(exprId.getPointerLevel()>0){
                    if(!popped)
                        builder.emit_pop(reg);
                    popped = true;
                    builder.emit_mov_rm(BC_REG_C, reg, REGISTER_SIZE);
                    reg = BC_REG_C;
                }
                if(memberData.offset!=0){
                    if(!popped)
                        builder.emit_pop(reg);
                    popped = true;
                    
                    builder.emit_li32(BC_REG_A, memberData.offset);
                    builder.emit_add(reg, BC_REG_A, REGISTER_SIZE, false);
                }
                if(!popped)
                    builder.emit_pop(reg);
                
                // builder.emit_li64(BC_REG_T0, mem.array_length);
                // builder.emit_push(BC_REG_T0);
                
                builder.emit_push(reg);

                outTypeIds->add(type);
                
                // NOTE: Arrays in structs don't evaluate to slice anymore.
                // std::string slice_name = "Slice<" + ast->typeToString(memberData.typeId) + ">";
                // auto slice_info = info.ast->convertToTypeInfo(slice_name, info.ast->globalScopeId, true);
                // if(!slice_info) {
                //     Assert(info.hasForeignErrors());
                //     return SIGNAL_FAILURE;
                // }
                
                // bool popped = false;
                // BCRegister reg = BC_REG_B;
                // if(exprId.getPointerLevel()>0){
                //     if(!popped)
                //         builder.emit_pop(reg);
                //     popped = true;
                //     builder.emit_mov_rm(BC_REG_C, reg, REGISTER_SIZE);
                //     reg = BC_REG_C;
                // }
                // if(memberData.offset!=0){
                //     if(!popped)
                //         builder.emit_pop(reg);
                //     popped = true;
                    
                //     builder.emit_li32(BC_REG_A, memberData.offset);
                //     builder.emit_add(reg, BC_REG_A, false, REGISTER_SIZE);
                // }
                // if(!popped)
                //     builder.emit_pop(reg);
                    
                
                // builder.emit_li64(BC_REG_T0, mem.array_length);
                // builder.emit_push(BC_REG_T0);
                
                // builder.emit_push(reg);
                
                // exprId = slice_info->id;
                
                // outTypeIds->add(exprId);
            } else {
                if(!nonReference) {
                    // auto memberData = typeInfo->getMember(expression->name);
                    // if(memberData.index==-1){
                    //     Assert(info.hasForeignErrors());
                    //     return SIGNAL_FAILURE;
                    // }
                    // auto& mem = typeInfo->astStruct->members[memberData.index];
                    bool popped = false;
                    BCRegister reg = BC_REG_B;
                    if(exprId.getPointerLevel()>0){
                        if(!popped)
                            builder.emit_pop(reg);
                        popped = true;
                        builder.emit_mov_rm(BC_REG_C, reg, REGISTER_SIZE);
                        reg = BC_REG_C;
                    }
                    if(memberData.offset!=0){
                        if(!popped)
                            builder.emit_pop(reg);
                        popped = true;
                        
                        builder.emit_li32(BC_REG_A, memberData.offset);
                        builder.emit_add(reg, BC_REG_A, REGISTER_SIZE, false);
                    }
                    if(popped)
                        builder.emit_push(reg);

                    exprId = memberData.typeId;
                    
                    builder.emit_pop(BC_REG_B);
                    result = generatePush(BC_REG_B,0, exprId);
                    
                    outTypeIds->add(exprId);
                } else {
                    if(typeInfo->astStruct && typeInfo->astStruct->name == "Slice") {
                        // this is a hardcoded solution to popping members.
                        auto memberData = typeInfo->getMember(expression->name);
                        if(memberData.index==-1){
                            Assert(info.hasForeignErrors());
                            return SIGNAL_FAILURE;
                        }
                        if(memberData.index == 0) {
                            builder.emit_pop(BC_REG_A);
                            builder.emit_pop(BC_REG_T0);
                            builder.emit_push(BC_REG_A);  // one we care about
                        } else {
                            builder.emit_pop(BC_REG_T0);
                            // builder.emit_pop(BC_REG_A); // one we care about
                        }
                        exprId = memberData.typeId;
                        
                        outTypeIds->add(exprId);
                    } else {
                        if(exprId.getPointerLevel() == 0) {
                            ERR_SECTION(
                                ERR_HEAD2(expression->location)
                                ERR_MSG("Accessing a member of a returned struct or a struct that is considered a value and not a reference is not possible, for now. (with exception to slices)")
                                ERR_LINE2(expression->location,"here")
                            )
                        } else if(exprId.getPointerLevel() > 1) {
                            ERR_SECTION(
                                ERR_HEAD2(expression->location)
                                ERR_MSG("You cannot access members if the type is a 2-level pointer Accessing members is only allowed if the expression evaluates to a 1-level pointer or a reference to a variable.")
                                ERR_LINE2(expression->location,"here")
                            )
                        }
                        BCRegister reg = BC_REG_B;
                        builder.emit_pop(reg);
                        
                        builder.emit_mov_rm(BC_REG_C, reg, REGISTER_SIZE);
                        reg = BC_REG_C;
                        
                        if(memberData.offset!=0){
                            builder.emit_li32(BC_REG_A, memberData.offset);
                            builder.emit_add(reg, BC_REG_A, REGISTER_SIZE, false);
                        }

                        exprId = memberData.typeId;
                        
                        result = generatePush(BC_REG_B, 0, exprId);
                        
                        outTypeIds->add(exprId);
                        
                        // Note a great error
                        // ERR_SECTION(
                        //     ERR_HEAD2(expression->left->location)
                        //     ERR_MSG("'"<<compiler->lexer.tostring(expression->left->location)<<
                        //     "' must be a reference to some memory. "
                        //     "A variable, member or expression resulting in a dereferenced pointer would work.")
                        //     ERR_LINE2(expression->left->location, "must be a reference")
                        // )
                    }
                }
            }
        }
        
        // OLD code
        // result = generateReference(expression,&exprId);
        // if(result != SIGNAL_SUCCESS) {
        //     return SIGNAL_FAILURE;
        // }
        // // TODO: We don't allow Apple{"Green",9}.name because initializer is not
        // //  a reference. We should allow it though.
        // builder.emit_pop(BC_REG_B);
        // result = generatePush(BC_REG_B,0, exprId);
        
        // outTypeIds->add(exprId);
    } else if (base_expression->type == EXPR_INITIALIZER) {
        auto expression = base_expression->as<ASTExpressionInitializer>();
        TypeId castType = expression->versions_castType[info.currentPolyVersion];
        TypeInfo* base_typeInfo = info.ast->getTypeInfo(castType.baseType()); // TODO: castType should be renamed
        // TypeInfo *structInfo = info.ast->getTypeInfo(info.currentScopeId, Token(*expression->name));
        if (!base_typeInfo) {
            if(!hasAnyErrors()) {
                auto str = info.ast->typeToString(castType);
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("Cannot do initializer on type '" << log::YELLOW << str << log::NO_COLOR << "'.")
                    ERR_LINE2(expression->location, "bad")
                )
            }
            return SIGNAL_FAILURE;
        }
        bool is_struct = base_typeInfo->astStruct && castType.isNormalType();
        if(!is_struct && expression->args.size() > 1) {
            auto str = info.ast->typeToString(castType);
            ERR_SECTION(
                ERR_HEAD2(expression->location)
                ERR_MSG_COLORED("The type '" << log::YELLOW << str << log::NO_COLOR << "' is not a struct and cannot have more than one value/expression in the initializer.")
                ERR_LINE2(expression->args[1]->location, "not allowed")
            )
            return SIGNAL_FAILURE;
        }    
        
        ASTStruct *ast_struct = base_typeInfo->astStruct;
        
        if(is_struct) {
            // store expressions in a list so we can iterate in reverse
            // TODO: parser could arrange the expressions in reverse.
            //   it would probably be easier since it constructs the nodes
            //   and has a little more context and control over it.
            // std::vector<ASTExpression *> exprs;
            TEMP_ARRAY(ASTExpression*,exprs);
            exprs.resize(ast_struct->members.size());

            // Assert(expression->args);
            // for (int index = 0; index < (int)expression->args->size(); index++) {
            //     ASTExpression *expr = expression->args->get(index);
                
            for (int index = 0; index < (int)expression->args.size(); index++) {
                ASTExpression *expr = expression->args.get(index);

                if (!expr->namedValue.len) {
                    if ((int)exprs.size() <= index) {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("To many members for struct " << ast_struct->name << " (" << ast_struct->members.size() << " member(s) allowed).")
                            ERR_LINE2(expr->location, "here")
                        )
                        continue;
                    }
                    else
                        exprs[index] = expr;
                } else {
                    int memIndex = -1;
                    for (int i = 0; i < (int)ast_struct->members.size(); i++) {
                        if (ast_struct->members[i].name == expr->namedValue) {
                            memIndex = i;
                            break;
                        }
                    }
                    if (memIndex == -1) {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("'"<<expr->namedValue << "' is not an member in " << ast_struct->name << ".")
                        )
                        continue;
                    } else {
                        if (exprs[memIndex]) {
                            ERR_SECTION(
                                ERR_HEAD2(expr->location)
                                ERR_MSG("Argument for " << ast_struct->members[memIndex].name << " is already specified.")
                                ERR_LINE2(exprs[memIndex]->location, "here")
                            )
                        } else {
                            exprs[memIndex] = expr;
                        }
                    }
                }
            }

            for (int i = 0; i < (int)ast_struct->members.size(); i++) {
                auto &mem = ast_struct->members[i];
                if (!exprs[i])
                    exprs[i] = mem.defaultValue;
            }

            int index = (int)exprs.size();
            while (index > 0) {
                index--;
                ASTExpression *expr = exprs[index];
                TypeId exprId={};
                if (!expr) {
                    exprId = base_typeInfo->getMember(index).typeId;
                    SignalIO result = generateDefaultValue(BC_REG_INVALID, 0, exprId, nullptr);
                    if (result != SIGNAL_SUCCESS)
                        return result;
                    // ERR_SECTION(
                // ERR_HEAD2(expression->location, "Missing argument for " << astruct->members[index].name << " (call to " << astruct->name << ").\n";
                    // )
                    // continue;
                } else {
                    tempTypes.clear();
                    SignalIO result = generateExpression(expr, &tempTypes);
                    if(tempTypes.size()) exprId = tempTypes.last();
                    if (result != SIGNAL_SUCCESS)
                        return result;
                }

                if (index >= (int)base_typeInfo->astStruct->members.size()) {
                    // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                    continue;
                }
                TypeId tid = base_typeInfo->getMember(index).typeId;
                if (!performSafeCast(exprId, tid)) {   // implicit conversion
                    // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                    ERRTYPE1(expr->location, exprId, tid, "(initializer of '"<<log::LIME<<ast->typeToString(base_typeInfo->id)<<log::NO_COLOR<<"')");
                    
                    continue;
                }
            }
        } else {
            if(expression->args.size()>0) {
                ASTExpression* expr = expression->args[0];
                TypeId expr_type{};
                tempTypes.clear();
                SignalIO result = generateExpression(expr, &tempTypes);
                    if(tempTypes.size()) expr_type = tempTypes.last();
                if (result != SIGNAL_SUCCESS)
                    return result;
                    
                // if(expr_type!=castType){ // strict, no conversion
                if (!performSafeCast(expr_type, castType)) { // implicit conversion
                    ERRTYPE1(expr->location, expr_type, castType, "(initializer)");
                    return SIGNAL_FAILURE;
                }
            } else {
                SignalIO result = generateDefaultValue(BC_REG_INVALID, 0, castType, nullptr);
                if (result != SIGNAL_SUCCESS)
                    return result;
            }
        }
        // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
        //     // return SIGNAL_FAILURE;
        // }
        if(outTypeIds)
            outTypeIds->add(castType);
    } else if(base_expression->type == EXPR_OPERATION || base_expression->type == EXPR_ASSIGN) {
        FuncImpl* operatorImpl = nullptr;
        if(base_expression->type == EXPR_OPERATION) {
            auto expression = base_expression->as<ASTExpressionOperation>();
            if(expression->versions_overload.size()>0) {
                operatorImpl = expression->versions_overload[info.currentPolyVersion].funcImpl;
                if(operatorImpl){
                    return generateFncall(expression, outTypeIds, true);
                }
            }
        }
        TypeId ltype = TYPE_VOID;
        if(base_expression->type != EXPR_ASSIGN) {
            auto expression = base_expression->as<ASTExpressionOperation>();
            if (expression->op_type == AST_REFER) {
                bool wasNonReference = false;
                SignalIO result = generateReference(expression->left,&ltype,idScope, &wasNonReference);
                if(result!=SIGNAL_SUCCESS)
                    return SIGNAL_FAILURE;
                if(ltype.getPointerLevel()==3){
                    ERR_SECTION(
                        ERR_HEAD2(expression->location)
                        ERR_MSG("Cannot take a reference of a triple pointer (compiler has a limit of 0-3 depth of pointing). Cast to u64 if you need triple pointers.")
                    )
                    return SIGNAL_FAILURE;
                }
                if (wasNonReference) {
                    // non reference means that the pointer pushed to the stack (by generateReference) has the type that was returned (ltype).
                } else {
                    // if not non reference then generateReference returned the type that the pointer on the refers to.
                    ltype.setPointerLevel(ltype.getPointerLevel()+1);
                }
                outTypeIds->add(ltype); 
            } else if (expression->op_type == AST_DEREF) {
                
                tempTypes.resize(0);
                
                SignalIO result = generateExpression(expression->left, &tempTypes);
                if (result != SIGNAL_SUCCESS)
                    return result;
                
                if(tempTypes.size()!=1) {
                    StringBuilder values = {};
                    FORN(tempTypes) {
                        auto& it = tempTypes[nr];
                        if(nr != 0)
                            values += ", ";
                        values += info.ast->typeToString(it);
                    }
                    ERR_SECTION(
                        ERR_HEAD2(expression->left->location)
                        ERR_MSG("Cannot dereference more than one value.")
                        ERR_LINE2(expression->left->location, values.data())
                    )
                    return SIGNAL_FAILURE;
                }
                ltype = tempTypes[0];

                if (!ltype.isPointer()) {
                    ERR_SECTION(
                        ERR_HEAD2(expression->left->location)
                        ERR_MSG("Cannot dereference " << info.ast->typeToString(ltype) << ".")
                        ERR_LINE2(expression->left->location, "bad")
                    )
                    outTypeIds->add( TYPE_VOID);
                    return SIGNAL_FAILURE;
                }

                TypeId outId = ltype;
                outId.setPointerLevel(outId.getPointerLevel()-1);

                if (outId == TYPE_VOID) {
                    ERR_SECTION(
                        ERR_HEAD2(expression->left->location)
                        ERR_MSG("Cannot dereference " << info.ast->typeToString(ltype) << ".")
                        ERR_LINE2(expression->left->location, "bad")
                    )
                    return SIGNAL_FAILURE;
                }
                Assert(info.ast->getTypeSize(ltype) == REGISTER_SIZE); // left expression must return pointer
                builder.emit_pop(BC_REG_B);

                if(outId.isPointer()){
                    int size = info.ast->getTypeSize(outId);

                    BCRegister reg = BC_REG_A;

                    builder.emit_mov_rm(reg, BC_REG_B, size);
                    builder.emit_push(reg);
                } else {
                    generatePush(BC_REG_B, 0, outId);
                }
                outTypeIds->add(outId);
            }
            else if(expression->op_type == AST_NOT) {
                tempTypes.clear();
                SignalIO result = generateExpression(expression->left, &tempTypes);
                if(tempTypes.size())
                    ltype = tempTypes.last();
                if (result != SIGNAL_SUCCESS)
                    return result;
                // TypeInfo *ti = info.ast->getTypeInfo(ltype);
                u32 size = info.ast->getTypeSize(ltype);
                // using two registers instead of one because
                // it is easier to implement in x64 generator 
                // or not actually.
                BCRegister reg = BC_REG_A;
                // u8 reg2 = RegBySize(BC_BX, size);

                builder.emit_pop(reg);
                builder.emit_lnot(reg, reg, size);
                builder.emit_push(reg);

                outTypeIds->add(TYPE_BOOL);
            }
            else if(expression->op_type == AST_BNOT) {
                tempTypes.clear();
                SignalIO result = generateExpression(expression->left, &tempTypes);
                if(tempTypes.size())
                    ltype = tempTypes.last();
                if (result != SIGNAL_SUCCESS)
                    return result;
                // TypeInfo *ti = info.ast->getTypeInfo(ltype);
                u32 size = info.ast->getTypeSize(ltype);
                BCRegister reg = BC_REG_A;

                builder.emit_pop(reg);
                builder.emit_bnot(reg, reg, size);
                builder.emit_push(reg);

                outTypeIds->add(ltype);
            }
            else if(expression->op_type == AST_RANGE){
                TypeInfo* typeInfo = info.ast->convertToTypeInfo("Range", info.ast->globalScopeId, true);
                if(!typeInfo){
                    ERR_SECTION(
                        ERR_HEAD2(expression->location)
                        ERR_MSG("'"<<compiler->lexer.tostring(expression->location)<<"' cannot be converted to struct Range since it doesn't exist. Did you forget #import \"Basic\".")
                        ERR_LINE2(expression->location,"Where is Range?")
                    )
                    return SIGNAL_FAILURE;
                }
                Assert(typeInfo->astStruct);
                Assert(typeInfo->astStruct->members.size() == 2);
                Assert(typeInfo->structImpl->members[0].typeId == typeInfo->structImpl->members[1].typeId);

                TypeId inttype = typeInfo->structImpl->members[0].typeId;

                TypeId ltype={};
                SignalIO result = generateExpression(expression->left,&ltype,idScope);
                if(result!=SIGNAL_SUCCESS)
                    return result;
                TypeId rtype={};
                result = generateExpression(expression->right,&rtype,idScope);
                if(result!=SIGNAL_SUCCESS)
                    return result;
                
                BCRegister lreg = BC_REG_C;
                BCRegister rreg = BC_REG_D;
                builder.emit_pop(rreg);
                builder.emit_pop(lreg);

                builder.emit_push(rreg);
                if(!performSafeCast(rtype,inttype)){
                    std::string msg = info.ast->typeToString(rtype);
                    ERR_SECTION(
                        ERR_HEAD2(expression->right->location)
                        ERR_MSG("Cannot convert to "<<info.ast->typeToString(inttype)<<".")
                        ERR_LINE2(expression->right->location,msg.c_str())
                    )
                }

                builder.emit_push(lreg);
                if(!performSafeCast(ltype,inttype)){
                    std::string msg = info.ast->typeToString(ltype);
                    ERR_SECTION(
                        ERR_HEAD2(expression->right->location)
                        ERR_MSG("Cannot convert to "<<info.ast->typeToString(inttype)<<".")
                        ERR_LINE2(expression->right->location,msg.c_str());
                    )
                }

                outTypeIds->add(typeInfo->id);
                return SIGNAL_SUCCESS;
            } 
            else if(expression->op_type == AST_INDEX){
                FuncImpl* operatorImpl = nullptr;
                if(expression->versions_overload.size()>0)
                    operatorImpl = expression->versions_overload[info.currentPolyVersion].funcImpl;
                TypeId ltype = TYPE_VOID;
                if(operatorImpl){
                    return generateFncall(expression, outTypeIds, true);
                }
                tempTypes.clear();
                SignalIO err = generateExpression(expression->left, &tempTypes);
                if(tempTypes.size()) ltype = tempTypes.last();
                if (err != SIGNAL_SUCCESS)
                    return SIGNAL_FAILURE;

                tempTypes.clear();
                TypeId rtype;
                err = generateExpression(expression->right, &tempTypes);
                if(tempTypes.size()) rtype = tempTypes.last();
                if (err != SIGNAL_SUCCESS)
                    return SIGNAL_FAILURE;

                if(!performSafeCast(rtype, TYPE_INT32)){
                    std::string strtype = info.ast->typeToString(rtype);
                    ERR_SECTION(
                        ERR_HEAD2(expression->right->location)
                        ERR_MSG("Index operator ( array[23] ) requires integer type in the inner expression. '"<<strtype<<"' is not an integer.")
                        ERR_LINE2(expression->right->location,strtype.c_str())
                    )
                    return SIGNAL_FAILURE;
                }
                // NOTE: I had an idea about implicit dereference of Slice with index operator similar to 
                //   struct_ptr.member but this is ambiguous since you may have an array of slices you want to access.
                // bool implicit_deref = false;
                TypeInfo* linfo = nullptr;
                if(ltype.isNormalType()) {
                    linfo = ast->getTypeInfo(ltype);
                }
                //  else if (ltype.getPointerLevel() == 1){
                //     linfo = ast->getTypeInfo(ltype.baseType());
                //     implicit_deref = true;
                // }
                if(linfo && linfo->astStruct && linfo->astStruct->name == "Slice") {
                    auto& ptr_mem = linfo->structImpl->members[0];
                    Assert(linfo->astStruct->members[0].name == "ptr");

                    TypeId out_type = ptr_mem.typeId;
                    out_type.setPointerLevel(out_type.getPointerLevel()-1);

                    u32 lsize = info.ast->getTypeSize(out_type);
                    u32 rsize = info.ast->getTypeSize(out_type);

                    if(!out_type.isValid()) {
                        ERR_SECTION(
                            ERR_HEAD2(expression->left->location)
                            ERR_MSG("The type contained by "<<ast->typeToString(ltype)<<" is invalid (void). You cannot retrieve a value of void.")
                            ERR_LINE2(expression->left->location,"invalid type")
                        )
                        return SIGNAL_FAILURE;
                    }
                    if(lsize == 0) {
                        ERR_SECTION(
                            ERR_HEAD2(expression->left->location)
                            ERR_MSG("The type contained by "<<ast->typeToString(ltype)<<" is of size zero. You cannot retrieve a value of a type that is zero in size.")
                            ERR_LINE2(expression->left->location,"invalid type")
                        )
                        return SIGNAL_FAILURE;
                    }

                    BCRegister indexer_reg = BC_REG_D;
                    builder.emit_pop(indexer_reg); // integer

                    builder.emit_pop(BC_REG_B); // reference
                    builder.emit_pop(BC_REG_C); // len

                    // TODO: BOUNDS CHECK builder.emit_bounds_check

                    if(lsize>1){
                        builder.emit_li32(BC_REG_A, lsize);
                        builder.emit_mul(indexer_reg, BC_REG_A, 4, false, false);
                    }
                    builder.emit_add(BC_REG_B, indexer_reg, REGISTER_SIZE, false);

                    SignalIO result = generatePush(BC_REG_B, 0, out_type);

                    outTypeIds->add(out_type);
                } else if(ltype.isPointer()){
                    ltype.setPointerLevel(ltype.getPointerLevel()-1);

                    u32 lsize = info.ast->getTypeSize(ltype);
                    u32 rsize = info.ast->getTypeSize(rtype);
                    BCRegister reg = BC_REG_D;
                    builder.emit_pop(reg); // integer
                    builder.emit_pop(BC_REG_B); // reference
                    if(lsize>1){
                        builder.emit_li32(BC_REG_A, lsize);
                        builder.emit_mul(reg,BC_REG_A, 4, false, false);
                    }
                    builder.emit_add(BC_REG_B, reg, REGISTER_SIZE, false);

                    SignalIO result = generatePush(BC_REG_B, 0, ltype);

                    outTypeIds->add(ltype);
                } else {
                    if(!info.hasForeignErrors()){
                        std::string strtype = info.ast->typeToString(ltype);
                        ERR_SECTION(
                            ERR_HEAD2(expression->right->location)
                            ERR_MSG("Index operator ( array[23] ) requires pointer or Slice type in the outer expression. '"<<strtype<<"' is neither.")
                            ERR_LINE2(expression->right->location,strtype.c_str())
                        )
                    }
                    return SIGNAL_FAILURE;
                }
            }
            else if(expression->op_type == AST_PRE_INCREMENT || expression->op_type == AST_PRE_DECREMENT || expression->op_type == AST_POST_INCREMENT || expression->op_type == AST_POST_DECREMENT){
                SignalIO result = generateReference(expression->left, &ltype, idScope);
                if(result != SIGNAL_SUCCESS){
                    return SIGNAL_FAILURE;
                }
                
                if(!AST::IsInteger(ltype) && !ltype.isPointer()){
                    std::string strtype = info.ast->typeToString(ltype);
                    ERR_SECTION(
                        ERR_HEAD2(expression->left->location)
                        ERR_MSG("Increment/decrement only works on integer types unless overloaded.")
                        ERR_LINE2(expression->left->location, strtype.c_str())
                    )
                    return SIGNAL_FAILURE;
                }

                int size = info.ast->getTypeSize(ltype);
                BCRegister reg = BC_REG_A;

                builder.emit_pop(BC_REG_B); // reference

                builder.emit_mov_rm(reg, BC_REG_B, size);
                // post
                bool is_post = expression->op_type == AST_POST_INCREMENT || expression->op_type == AST_POST_DECREMENT;
                if(is_post)
                    builder.emit_push(reg);
                if(expression->op_type == AST_PRE_INCREMENT || expression->op_type == AST_POST_INCREMENT)
                    builder.emit_incr(reg, 1);
                else
                    builder.emit_incr(reg, -1);
                builder.emit_mov_mr(BC_REG_B, reg, size);
                // pre
                if(!is_post)
                    builder.emit_push(reg);
                    
                outTypeIds->add(ltype);
            }
            else if(expression->op_type == AST_UNARY_SUB){
                tempTypes.clear();
                SignalIO result = generateExpression(expression->left, &tempTypes, idScope);
                if(tempTypes.size()) ltype = tempTypes.last();
                if(result != SIGNAL_SUCCESS){
                    return SIGNAL_FAILURE;
                }
                
                if(AST::IsInteger(ltype)) {
                    int size = info.ast->getTypeSize(ltype);
                    BCRegister regFinal = BC_REG_A;
                    BCRegister regValue = BC_REG_D;

                    builder.emit_pop(regValue);
                    builder.emit_bxor(regFinal, regFinal, size);
                    builder.emit_sub(regFinal, regValue, size, false);
                    builder.emit_push(regFinal);
                    outTypeIds->add(ltype);
                } else if (AST::IsDecimal(ltype)) {
                    int size = info.ast->getTypeSize(ltype);
                    BCRegister regMask = BC_REG_A;
                    BCRegister regValue = BC_REG_D;

                    builder.emit_pop(regValue);
                    builder.emit_li32(regMask, 0);
                    builder.emit_sub(regMask, regValue, size, true, false);
                    builder.emit_push(regMask);

                    // if(size == 8)
                    //     builder.emit_li64(regMask, 0x8000000000000000);
                    // else
                    //     builder.emit_li32(regMask, 0x80000000);
                    // builder.emit_pop(regValue);
                    // builder.emit_bxor(regValue, regMask, size);
                    // builder.emit_push(regValue);

                    outTypeIds->add(ltype);
                } else {
                    std::string strtype = info.ast->typeToString(ltype);
                    ERR_SECTION(
                        ERR_HEAD2(expression->left->location)
                        ERR_MSG("Unary subtraction only works with integers and decimals.")
                        ERR_LINE2(expression->left->location, strtype.c_str())
                    )
                    return SIGNAL_FAILURE;
                }
            }
        }
        if(outTypeIds->size() == 0) {
            // I commented this out at some point for some reason. Not sure why?
            // Did I disable operator overloading? Should I uncomment?
            // const char* str = OpToStr((OperationType)expr->typeId.getId(), true);
            // if(str) {
            //     int yes = CheckFncall(info,scopeId,expr, outTypes, true);
                
            //     if(yes)
            //         return true;
            // }
            if(base_expression->type == EXPR_ASSIGN && base_expression->as<ASTExpressionAssign>()->assign_op_type == (OperationType)0){
                auto expression = base_expression->as<ASTExpressionAssign>();
                // THIS IS PURELY ASSIGN NOT +=, *=
                // WARN_HEAD3(expression->location,"Expression is generated first and then reference. Right to left instead of left to right. "
                // "This is important if you use assignments/increments/decrements on the same memory/reference/variable.\n\n";
                //     WARN_LINE(expression->right->location,"generated first");
                //     WARN_LINE(expression->left->location,"then this");
                // )
                tempTypes.clear();
                TypeId rtype{};
                SignalIO result = generateExpression(expression->right, &tempTypes, idScope);
                if(tempTypes.size())
                    rtype = tempTypes[0];

                for(int i=tempTypes.size()-1; i >= 1;i--) {
                    generatePop(BC_REG_INVALID, 0, tempTypes[i]);
                }

                if(result!=SIGNAL_SUCCESS)
                    return SIGNAL_FAILURE;

                TypeId assignType={};
                result = generateReference(expression->left,&assignType,idScope);
                if(result!=SIGNAL_SUCCESS)
                    return SIGNAL_FAILURE;

                builder.emit_pop(BC_REG_B);

                if (!performSafeCast(rtype, assignType)) { // CASTING RIGHT VALUE TO TYPE ON THE LEFT
                    // std::string leftstr = info.ast->typeToString(assignType);
                    // std::string rightstr = info.ast->typeToString(rtype);
                    ERRTYPE1(expression->location, rtype,assignType, ""
                        // ERR_LINE2(expression->left->location, leftstr.c_str());
                        // ERR_LINE2(expression->right->location,rightstr.c_str());
                    )
                    return SIGNAL_FAILURE;
                }
                // if(assignType != rtype){
                //     if(info.errors != 0 || info.compileInfo->typeErrors !=0){
                //         return SIGNAL_FAILURE;
                //     }
                //     log::out << info.ast->typeToString(assignType)<<" = "<<info.ast->typeToString(rtype)<<"\n";
                //     // TODO: Try to cast, you may need to rearragne some things and also add some poly_versions type stuff.
                //     //  You'll figure it out.
                //     Assert(assignType == rtype);
                // }

                 // note that right expression should be popped first

                // TODO: This code can be improved.
                //  Don't push if value is ignored.
                //  Don't pop, just copy if you need the pushed values.
                //  perhaps implement a function called GenerateCopy? nah, come up with a better name, rename GeneratePop/Push too.
                result = generatePop(BC_REG_B, 0, assignType);
                result = generatePush(BC_REG_B, 0, assignType);
                
                outTypeIds->add(rtype);
            } else {
                    
                // basic operations
                // BREAK(expression->nodeId == 1365)
                ASTExpression* expr_left = nullptr;
                ASTExpression* expr_right = nullptr;
                OperationType operationType = (OperationType)0;
                if(base_expression->type == EXPR_ASSIGN) {
                    auto expression = base_expression->as<ASTExpressionAssign>();
                    expr_left = expression->left;
                    expr_right = expression->right;
                    operationType = expression->assign_op_type;
                } else {
                    auto expression = base_expression->as<ASTExpressionOperation>();
                    expr_left = expression->left;
                    expr_right = expression->right;
                    operationType = expression->op_type;
                }
                
                tempTypes.clear();
                TypeId ltype{}, rtype{};
                SignalIO err = generateExpression(expr_left, &tempTypes);
                if(tempTypes.size()) ltype = tempTypes.last();
                if (err != SIGNAL_SUCCESS)
                    return err;
                tempTypes.clear();
                err = generateExpression(expr_right, &tempTypes);
                if(tempTypes.size()) rtype = tempTypes.last();
                if (err != SIGNAL_SUCCESS)
                    return err;
                if(!ltype.isValid() || !rtype.isValid())
                    return SIGNAL_FAILURE;
                TypeInfo* left_info = info.ast->getTypeInfo(ltype.baseType());
                TypeInfo* right_info = info.ast->getTypeInfo(rtype.baseType());
                Assert(left_info && right_info);
                
                TypeId voidp = TypeId::Create(TYPE_VOID);
                voidp.setPointerLevel(1);
                if((ltype.getId() < TYPE_PRIMITIVE_COUNT || ltype.getPointerLevel() > 0) && (rtype.getId() < TYPE_PRIMITIVE_COUNT || rtype.getPointerLevel())){
                    // okay
                } else if(left_info->astEnum && ltype == rtype) {
                    // okay
                } else if(left_info->funcType && ltype == rtype) {
                    // okay
                } else if((left_info->funcType || right_info->funcType) && (ltype == voidp || rtype == voidp)) {
                    // okay
                } else if((left_info->funcType || right_info->funcType) && (ltype == TYPE_BOOL || rtype == TYPE_BOOL)) {
                    // okay
                } else {
                    if(left_info->astStruct || right_info->astStruct) {
                        // log::out << "Fail, " << expression->nodeId<<"\n";
                        std::string lmsg = info.ast->typeToString(ltype);
                        std::string rmsg = info.ast->typeToString(rtype);
                        ERR_SECTION(
                            ERR_HEAD2(base_expression->location)
                            ERR_MSG("Cannot do operation on struct.")
                            ERR_LINE2(expr_left->location,lmsg.c_str());
                            ERR_LINE2(expr_right->location,rmsg.c_str());
                        )
                    } else if(left_info->astEnum || right_info->astEnum) {
                        std::string lmsg = info.ast->typeToString(ltype);
                        std::string rmsg = info.ast->typeToString(rtype);
                        ERR_SECTION(
                            ERR_HEAD2(base_expression->location)
                            ERR_MSG("Cannot do operation on enums.")
                            ERR_LINE2(expr_left->location,lmsg.c_str());
                            ERR_LINE2(expr_right->location,rmsg.c_str());
                        )
                    } else {
                        std::string lmsg = info.ast->typeToString(ltype);
                        std::string rmsg = info.ast->typeToString(rtype);
                        ERR_SECTION(
                            ERR_HEAD2(base_expression->location)
                            ERR_MSG("Cannot perform operation on the types.")
                            ERR_LINE2(expr_left->location,lmsg.c_str());
                            ERR_LINE2(expr_right->location,rmsg.c_str());
                        )
                    }
                    return SIGNAL_FAILURE;
                }
                
                TypeId outType = ltype;
                
                
                bool is_arithmetic = operationType == AST_ADD || operationType == AST_SUB || operationType == AST_MUL || operationType == AST_DIV || operationType == AST_MODULO;
                bool is_comparison = operationType == AST_LESS || operationType == AST_LESS_EQUAL || operationType == AST_GREATER || operationType == AST_GREATER_EQUAL;
                bool is_equality = operationType == AST_EQUAL || operationType == AST_NOT_EQUAL;
                bool is_float = AST::IsDecimal(ltype) || AST::IsDecimal(rtype);
                bool is_signed = !is_float && (AST::IsSigned(ltype) || AST::IsSigned(rtype));
                
                BCRegister left_reg = BC_REG_C; // also out register
                BCRegister right_reg = BC_REG_D;
                builder.emit_pop(right_reg);
                builder.emit_pop(left_reg);
                
                int outSize = 0;
                int operand_size = 0;
                
                /*###########################
                 Validation operations on types
                ################################*/
                if((ltype.isPointer() && AST::IsInteger(rtype)) || (AST::IsInteger(ltype) && rtype.isPointer()) || (ltype.isPointer() && rtype.isPointer())){
                    if(operationType == AST_ADD && ltype.isPointer() && rtype.isPointer()) {
                        ERR_SECTION(
                            ERR_HEAD2(base_expression->location)
                            ERR_MSG("You cannot add two pointers together. (is this a good or bad restriction?)")
                            ERR_LINE2(expr_left->location, info.ast->typeToString(ltype));
                            ERR_LINE2(expr_right->location, info.ast->typeToString(rtype));
                        )
                        return SIGNAL_FAILURE;
                    }
                    if((operationType == AST_EQUAL || operationType == AST_NOT_EQUAL)) {
                        if((AST::IsInteger(rtype) && info.ast->getTypeSize(rtype) != REGISTER_SIZE) || (AST::IsInteger(rtype) && info.ast->getTypeSize(rtype) != REGISTER_SIZE)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("When using equality operators with a pointer and integer they must be of the same size. The integer must be u64 or i64 (on 64-bit system).")
                                ERR_LINE2(expr_left->location, info.ast->typeToString(ltype));
                                ERR_LINE2(expr_right->location, info.ast->typeToString(rtype));
                            )
                            return SIGNAL_FAILURE;
                        }
                    }
                    if (operationType != AST_ADD && operationType != AST_SUB  && operationType != AST_EQUAL  && operationType != AST_NOT_EQUAL) {
                        ERR_SECTION(
                            ERR_HEAD2(base_expression->location)
                            ERR_MSG(OP_NAME(operationType) << " does not work with pointers. Only addition, subtraction and equality works.")
                            ERR_LINE2(expr_left->location, info.ast->typeToString(ltype));
                            ERR_LINE2(expr_right->location, info.ast->typeToString(rtype));
                        )
                        return SIGNAL_FAILURE;
                    }
                    if(rtype.isPointer()) {
                        outType = rtype; // Make sure '1 + ptr' results in a pointer type and not an int while 'ptr + 1' would be a pointer.
                        outSize = ast->getTypeSize(outType);
                        operand_size = outSize;
                    }
                } else if ((AST::IsInteger(ltype) || left_info->astEnum) && (AST::IsInteger(rtype) || right_info->astEnum)){
                    // TODO: WE DON'T CHECK SIGNEDNESS WITH ENUMS.
                    int lsize = info.ast->getTypeSize(ltype);
                    int rsize = info.ast->getTypeSize(rtype);
                    if(operationType == AST_DIV) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot divide signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    } else if(operationType == AST_MUL) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot multiply signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    }  else if(operationType == AST_MODULO) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot take remainder (modulus) of signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    } else if(is_comparison) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot compare signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    }
                    int outSize = lsize > rsize ? lsize : rsize;
                    if(is_comparison || is_equality) {
                        operand_size = outSize;
                    }
                    if(operationType == AST_BLSHIFT || operationType == AST_BRSHIFT) {
                        outSize = lsize;
                        operand_size = lsize;
                        outType = ltype;
                    } else {
                        if(lsize != outSize || (!AST::IsSigned(ltype) && AST::IsSigned(rtype))) {
                            if(!AST::IsSigned(ltype) && !AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_UINT_UINT, lsize, outSize);
                            if(!AST::IsSigned(ltype) && AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_UINT_SINT, lsize, outSize);
                            if(AST::IsSigned(ltype) && !AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_SINT_SINT, lsize, outSize);
                            if(AST::IsSigned(ltype) && AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_SINT_SINT, lsize, outSize);
                        }
                        if(rsize != outSize || (!AST::IsSigned(rtype) && AST::IsSigned(ltype))) {
                            if(!AST::IsSigned(rtype) && !AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_UINT_UINT, rsize, outSize);
                            if(!AST::IsSigned(rtype) && AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_UINT_SINT, rsize, outSize);
                            if(AST::IsSigned(rtype) && !AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_SINT_SINT, rsize, outSize);
                            if(AST::IsSigned(rtype) && AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_SINT_SINT, rsize, outSize);
                        }
                    }
                    if(operationType != AST_BLSHIFT && operationType != AST_BRSHIFT) {
                        if(lsize < rsize) {
                            outType = rtype;
                        } else {
                            outType = ltype;
                        }
                        if(AST::IsSigned(ltype) || AST::IsSigned(rtype)) {
                            if(!AST::IsSigned(outType))
                                outType._infoIndex0 += 4;
                            Assert(AST::IsSigned(outType));
                        }
                    }
                } else if ((AST::IsInteger(ltype) || ltype == TYPE_CHAR) && (AST::IsInteger(rtype) || rtype == TYPE_CHAR)){
                    // TODO: WE DON'T CHECK SIGNEDNESS WITH ENUMS.
                    int lsize = info.ast->getTypeSize(ltype);
                    int rsize = info.ast->getTypeSize(rtype);
                    if(operationType == AST_DIV) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot divide signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    } else if(operationType == AST_MUL) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot multiply signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    }  else if(operationType == AST_MODULO) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot take remainder (modulus) of signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    } else if(is_comparison) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(base_expression->location)
                                ERR_MSG("You cannot compare signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expr_left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expr_right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    }
                    int outSize = 1; // sizeof char
                    if(is_comparison || is_equality) {
                        operand_size = outSize;
                    }
                    if(operationType == AST_BLSHIFT || operationType == AST_BRSHIFT) {
                        
                    } else {
                        if(lsize != outSize || (!AST::IsSigned(ltype) && AST::IsSigned(rtype))) {
                            if(!AST::IsSigned(ltype) && !AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_UINT_UINT, lsize, outSize);
                            if(!AST::IsSigned(ltype) && AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_UINT_SINT, lsize, outSize);
                            if(AST::IsSigned(ltype) && !AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_SINT_UINT, lsize, outSize);
                            if(AST::IsSigned(ltype) && AST::IsSigned(rtype))
                                builder.emit_cast(left_reg,left_reg, CAST_SINT_SINT, lsize, outSize);
                        }
                        if(rsize != outSize || (!AST::IsSigned(rtype) && AST::IsSigned(ltype))) {
                            if(!AST::IsSigned(rtype) && !AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_UINT_UINT, rsize, outSize);
                            if(!AST::IsSigned(rtype) && AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_UINT_SINT, rsize, outSize);
                            if(AST::IsSigned(rtype) && !AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_SINT_UINT, rsize, outSize);
                            if(AST::IsSigned(rtype) && AST::IsSigned(ltype))
                                builder.emit_cast(right_reg,right_reg, CAST_SINT_SINT, rsize, outSize);
                        }
                    }
                    outType = TYPE_CHAR;
                } else if ((AST::IsDecimal(ltype) || AST::IsInteger(ltype)) && (AST::IsDecimal(rtype) || AST::IsInteger(rtype))){
                    if(!is_arithmetic && !is_comparison && !is_equality) {
                        // We may allow bitwise operation like 'num & 0x8000_0000' to check if a decimal is decimal or not.
                        // Or 'num & 0x7F_FFFF, num & > 23' to extract the exponent and mantissa.
                        ERR_SECTION(
                            ERR_HEAD2(base_expression->location)
                            ERR_MSG("Floats are limited to arithmetic and comparison operations.")
                            ERR_LINE2(base_expression->location, "here")
                        )
                        return SIGNAL_FAILURE;
                    }
                    
                    /*
                    integers should be converted to floats
                    floats should be converted to the biggest float
                    */

                    InstructionOpcode bytecodeOp = ASTOpToBytecode(operationType,true);
                    int lsize = info.ast->getTypeSize(ltype);
                    int rsize = info.ast->getTypeSize(rtype);
                    int finalSize = 0;
                    if(AST::IsDecimal(ltype)) {
                        finalSize = lsize;
                        outType = ltype;
                    }
                    if(AST::IsDecimal(rtype)) {
                        if(rsize > finalSize) {
                            finalSize = rsize;
                            outType = rtype;
                        }
                    }
                    if(is_comparison || is_equality) {
                        operand_size = finalSize;
                    }

                    if(AST::IsInteger(rtype)){
                        if(AST::IsSigned(rtype)){
                            builder.emit_cast(right_reg,right_reg, CAST_SINT_FLOAT, rsize, finalSize);
                        } else {
                            builder.emit_cast(right_reg,right_reg, CAST_UINT_FLOAT, rsize, finalSize);
                        }
                    } else if(rsize != finalSize) {
                        builder.emit_cast(right_reg,right_reg, CAST_FLOAT_FLOAT, rsize, finalSize);
                    }
                    if(AST::IsInteger(ltype)){
                        if(AST::IsSigned(ltype)){
                            builder.emit_cast(left_reg,left_reg, CAST_SINT_FLOAT, lsize, finalSize);
                        } else {
                            builder.emit_cast(left_reg,left_reg, CAST_UINT_FLOAT, lsize, finalSize);
                        }
                    } else if(lsize != finalSize) {
                        builder.emit_cast(left_reg,left_reg, CAST_FLOAT_FLOAT, lsize, finalSize);
                    }
                } else if((ltype.isPointer() || ltype == TYPE_BOOL) && (rtype == TYPE_BOOL || rtype.isPointer())) {
                    outSize = 1;
                    operand_size = 1;
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(base_expression->location)
                        ERR_MSG("You cannot perform an operation on these types.")
                        ERR_LINE2(expr_left->location, ast->typeToString(ltype))
                        ERR_LINE2(expr_right->location, ast->typeToString(rtype))
                    )
                    return SIGNAL_FAILURE;
                }
                
                if(is_comparison || is_equality) {
                    outType = TYPE_BOOL;
                }

                if(operationType == AST_AND || operationType == AST_OR) {
                    // Is this okay, assumming operand size is REGISTER_SIZE bytes?
                    outType = TYPE_BOOL;
                    operand_size = REGISTER_SIZE;
                }
                
                outSize = ast->getTypeSize(outType);

                /*#########################
                        Code generation
                ###########################*/
                
                switch(operationType) {
                case AST_ADD:           builder.emit_add(    left_reg, right_reg, outSize, is_float, is_signed); break;
                case AST_SUB:           builder.emit_sub(    left_reg, right_reg, outSize, is_float, is_signed); break;
                case AST_MUL:           builder.emit_mul(    left_reg, right_reg, outSize, is_float, is_signed); break;
                case AST_DIV:           builder.emit_div(    left_reg, right_reg, outSize, is_float, is_signed); break;
                case AST_MODULO:        builder.emit_mod(    left_reg, right_reg, outSize, is_float, is_signed); break;
                case AST_EQUAL:         builder.emit_eq(     left_reg, right_reg, operand_size, is_float); break;
                case AST_NOT_EQUAL:     builder.emit_neq(    left_reg, right_reg, operand_size, is_float); break;
                case AST_LESS:          builder.emit_lt(     left_reg, right_reg, operand_size, is_float, is_signed); break;
                case AST_LESS_EQUAL:    builder.emit_lte(    left_reg, right_reg, operand_size, is_float, is_signed); break;
                case AST_GREATER:       builder.emit_gt(     left_reg, right_reg, operand_size, is_float, is_signed); break;
                case AST_GREATER_EQUAL: builder.emit_gte(    left_reg, right_reg, operand_size, is_float, is_signed); break;
                case AST_AND:           builder.emit_land(   left_reg, right_reg, operand_size); break;
                case AST_OR:            builder.emit_lor(    left_reg, right_reg, operand_size); break;
                case AST_BAND:          builder.emit_band(   left_reg, right_reg, outSize); break;
                case AST_BOR:           builder.emit_bor(    left_reg, right_reg, outSize); break;
                case AST_BXOR:          builder.emit_bxor(   left_reg, right_reg, outSize); break;
                case AST_BLSHIFT:       builder.emit_blshift(left_reg, right_reg, outSize); break;
                case AST_BRSHIFT:       builder.emit_brshift(left_reg, right_reg, outSize); break;
                default: Assert(("Operation not implemented",false));
                }
                
                if(base_expression->type==EXPR_ASSIGN){
                    builder.emit_push(left_reg);
                    TypeId assignType={};
                    SignalIO result = generateReference(expr_left,&assignType,idScope);
                    if(result!=SIGNAL_SUCCESS)
                        return SIGNAL_FAILURE;
                    Assert(assignType == ltype);
                    Assert(BC_REG_B != left_reg);
                    builder.emit_pop(BC_REG_B);
                    // builder.emit_pop(left_reg); // performSafeCast pops value

                    if(!performSafeCast(outType, assignType)) {
                        ERR_SECTION(
                            ERR_HEAD2(base_expression->location)
                            ERR_MSG("Cannot cast '" << ast->typeToString(outType) << "' to '"<< ast->typeToString(assignType)<<"'.")
                            ERR_LINE2(base_expression->location, "here")
                        )
                    }
                    builder.emit_pop(left_reg); // performSafeCast, popped and pushed value so we pop it again

                    int assignedSize = info.ast->getTypeSize(ltype);
                    builder.emit_mov_mr(BC_REG_B, left_reg, assignedSize);
                }
                
                builder.emit_push(left_reg);
                
                if(outTypeIds)
                    outTypeIds->add(outType);
            }
        }
    }
    for(auto& typ : *outTypeIds){
        TypeInfo* ti = info.ast->getTypeInfo(typ.baseType());
        if(ti){
            Assert(("Expression should not evaluate to raw virtual type!",ti->id == typ.baseType()));
        }
    }
    // To avoid ensuring non virtual type everywhere all the entry point where virtual type can enter
    // the expression system is checked. The rest of the system won't have to check it.
    // An entry point has been missed if the assert above fire.
    return SIGNAL_SUCCESS;
}

SignalIO GenContext::generateFunction(ASTFunction* function, ASTStruct* astStruct){
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);

    // TODO: THIS IS TEMPORARY CODE!
    WHILE_TRUE_N(1000) {
        // In processImports we use 'lock_imports' mutex when checking is_being_checked.
        // This would mean that all threads must use the same mutex 'lock_imports' when checking 'is_being_checked', HOWEVER, we are not in a different phase and no thread will check is_being_checked with a different mutex except for the code right here. SOOO, we can use whichever mutex we'd like.
        ast->lock_globalScope.lock(); // TODO: use a different mutex named more appropriately
        bool is_being_checked = function->is_being_checked || (astStruct && astStruct->is_being_checked);
        if(!is_being_checked) {
            function->is_being_checked = true;
            if(astStruct)
                astStruct->is_being_checked = true;
            ast->lock_globalScope.unlock();
            break;
        }
        ast->lock_globalScope.unlock();

        engone::Sleep(0.001); // TODO: Don't sleep, try generating another function instead. Move responsibility to compiler instead of generator, currently the generator goes through all functions and generates stuff, perhaps the compiler loop should instead. We can add "generate function" tasks.
    }

    defer {
        ast->lock_globalScope.lock(); // TODO: use a different mutex named more appropriately
        function->is_being_checked = false;
        if(astStruct)
            astStruct->is_being_checked = false;
        ast->lock_globalScope.unlock();
    };
    
    _GLOG(FUNC_ENTER_IF(function->linkConvention == LinkConvention::NONE)) // no need to log
    
    MAKE_NODE_SCOPE(function);

    // log::out << "may gen "<<function->name << "\n";

    if(!hasForeignErrors()) {
        Assert(!function->body == !function->needsBody());
    }
    // Assert(function->body || function->external || function->native ||info.compileInfo->errors!=0);
    // if(!function->body && !function->external && !function->native) return PARSE_ERROR;
    
    // NOTE: This is the only difference between how methods and functions
    //  are generated.
    IdentifierFunction* identifier = nullptr;
    if(!astStruct){
        identifier = function->identifier;
        // // NOTE: We used this code when identifier wasn't stored in AST, but now it is so delete code below.
        // identifier = (IdentifierFunction*)info.ast->findIdentifier(info.currentScopeId, CONTENT_ORDER_MAX, function->name, nullptr);
        // if (!identifier) {
        //     // NOTE: function may not have been added in the type checker stage for some reason.
        //     // THANK YOU, past me for writing this note. I was wondering what I broke and reading the
        //     // note made instantly realise that I broke something in the type checker.
        //     ERR_SECTION(
        //         ERR_HEAD2(function->location)
        //         ERR_MSG("'"<<function->name << "' was null (compiler bug).")
        //         ERR_LINE2(function->location, "bad")
        //     )
        //     // if (function->tokenRange.firstToken.str) {
        //     //     ERRTOKENS(function->location)
        //     // }
        //     // 
        //     return SIGNAL_FAILURE;
        // }
        // if (identifier->type != Identifier::FUNCTION) {
        //     // I have a feeling some error has been printed if we end up here.
        //     // Double check that though.
        //     return SIGNAL_FAILURE;
        // }
        // ASTFunction* tempFunc = info.ast->getFunction(fid);
        // // Assert(tempFunc==function);
        // if(tempFunc!=function){
        //     // error already printed?
        //     return SIGNAL_FAILURE;
        // }
    }
    if(function->callConvention == CallConvention::INTRINSIC) {
        // is this okay?
        return SIGNAL_SUCCESS;
    }
    if(function->linkConvention != LinkConvention::NONE){
        if(function->polyArgs.size()!=0 || (astStruct && astStruct->polyArgs.size()!=0)){
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("Imported/native functions cannot be polymorphic.")
                ERR_LINE2(function->location, "remove polymorphism")
            )
            return SIGNAL_FAILURE;
        }
        if(identifier->funcOverloads.overloads.size() != 1){
            // TODO: This error prints multiple times for each duplicate definition of native function.
            //   With this, you know the location of the colliding functions but the error message is printed multiple times.
            //  It is better if the message is shown once and then show all locations at once.
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("Imported/native functions can only have one overload.")
                ERR_LINE2(function->location, "bad")
            )
            return SIGNAL_FAILURE;
        }
        return SIGNAL_SUCCESS;
    }

    // When export is implemented I need to come back here and fix it.
    // The assert will notify me when that happens.
    Assert(function->linkConvention == LinkConvention::NONE);

    for(auto& funcImpl : function->getImpls()) {
        // IMPORTANT: If you uncomment this then you have to make sure
        //  that the type checker checked this function body. It won't if
        //  the implementation isn't used.
        if(!funcImpl->isUsed())
            continue; // Skips implementation if it isn't used
        Assert(("func has already been generated!",funcImpl->tinycode_id == 0));

        if(funcImpl->astFunction->name == compiler->entry_point) {
            switch(info.compiler->options->target) { // this is really cheeky
            case TARGET_WINDOWS_x64:
                function->callConvention = STDCALL;
                break;
            case TARGET_LINUX_x64:
                function->callConvention = UNIXCALL;
                break;
            case TARGET_BYTECODE:
                function->callConvention = BETCALL;
                break;
            default: Assert(false);
            }
        }

        // log::out << "do gen "<<function->name << "\n";

        tinycode = bytecode->createTiny(function->name, function->callConvention);
        tinycode->funcImpl = funcImpl;
        out_codes->add(tinycode);
        builder.init(bytecode, tinycode, compiler);
        
        funcImpl->tinycode_id = tinycode->index + 1;
        if(tinycode->name == compiler->entry_point) {
            bytecode->index_of_main = tinycode->index;
            int prev_tinyindex;
            bool yes = info.bytecode->addExportedFunction(funcImpl->astFunction->name, tinycode->index, &prev_tinyindex);
            if(!yes) {
                auto& prev_tiny = bytecode->tinyBytecodes[prev_tinyindex];
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG_LOG("You have two functions named '"<<funcImpl->astFunction->name<<"' which is the entry point. ")
                    if(prev_tiny->debugFunction && !prev_tiny->debugFunction->funcAst) {
                        auto& file = bytecode->debugInformation->files[prev_tiny->debugFunction->fileIndex];
                        log::out << "The other 'main' is implicitly created in '" << file<<"'. The initial file you import should call upon a function in the other file.";
                    }
                    log::out << "\n\n";
                    
                    ERR_LINE2(function->location,"second '"<<funcImpl->astFunction->name<<"'")
                    if(prev_tiny->debugFunction) {
                        if(prev_tiny->debugFunction->funcAst) {
                            auto func = prev_tiny->debugFunction->funcAst;
                            ERR_LINE2(func->location,"first '"<<funcImpl->astFunction->name<<"'")
                        } else {
                            // auto& file = bytecode->debugInformation->files[prev_tiny->debugFunction->fileIndex];
                            // log::out << "First 'main' comes from global scope of " << file<<"\n";
                        }
                    }
                )
            }
        }
        if(function->export_alias.size() != 0) {
            int prev_tinyindex;
            bool yes = info.bytecode->addExportedFunction(funcImpl->astFunction->export_alias, tinycode->index, &prev_tinyindex);
            if (!yes) {
                auto& prev_tiny = bytecode->tinyBytecodes[prev_tinyindex];
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG("You cannot export functions with the same name. Use alias in annotation to export with a different name than the one used inside the language.")
                    ERR_LINE2(function->location,"rename or alias")
                    if(prev_tiny->debugFunction && prev_tiny->debugFunction->funcAst) {
                        auto func = prev_tiny->debugFunction->funcAst;
                        ERR_LINE2(func->location,"first '"<<funcImpl->astFunction->name<<"'")
                    }
                )
            }
        }

        DebugInformation* di = bytecode->debugInformation;
        Assert(di);
        auto src = compiler->lexer.getTokenSource_unsafe(function->location);
        auto dfun = di->addFunction(funcImpl, tinycode, imp->path, src->line);
        debugFunction = dfun;
        // debugFunctionIndex = di->functions.size();
        
        currentScopeDepth = -1; // incremented to 0 in GenerateBody
        currentPolyVersion = funcImpl->polyVersion;

        _GLOG(log::out << "Function " << function->name << "\n";)

        // TODO: Export function symbol if annotated with @export
        //  Perhaps force functions named main to be annotated with @export in parser
        //  instead of handling the special case for main here?

        auto prevFunc = info.currentFunction;
        auto prevFuncImpl = info.currentFuncImpl;
        auto prevScopeId = info.currentScopeId;
        info.currentFunction = function;
        info.currentFuncImpl = funcImpl;
        info.currentScopeId = function->scopeId;
        defer { info.currentFunction = prevFunc;
            info.currentFuncImpl = prevFuncImpl;
            info.currentScopeId = prevScopeId; };

        // reset frame offset at beginning of function
        currentFrameOffset = 0;

        #define DFUN_ADD_VAR(NAME, OFFSET, TYPE) dfun->localVariables.add({});\
                            dfun->localVariables.last().name = NAME;\
                            dfun->localVariables.last().frameOffset = OFFSET;\
                            dfun->localVariables.last().typeId = TYPE;

        // int allocated_stack_space = 0;

        if(function->callConvention != BETCALL) {
            // Assert(!function->parentStruct);

            if (funcImpl->signature.returnTypes.size() > 1) {
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG(ToString(function->callConvention) << " only allows one return value. BETCALL (the default calling convention in this language) supports multiple return values.")
                    ERR_LINE2(function->location, "bad")
                )
            }
        };

        // Why to we calculate var offsets here? Can't we do it in 
        if (function->arguments.size() != 0) {
            _GLOG(log::out << "set " << function->arguments.size() << " args\n");
            for (int i = 0; i < (int)function->arguments.size(); i++) {
                // for(int i = function->arguments.size()-1;i>=0;i--){
                auto &arg = function->arguments[i];
                auto &argImpl = funcImpl->signature.argumentTypes[i];
                auto varinfo = arg.identifier;
                if (!varinfo) {
                    ERR_SECTION(
                        ERR_HEAD2(arg.location)
                        ERR_MSG(arg.name << " is already defined.")
                        ERR_LINE2(arg.location,"cannot use again")
                    )
                }
                varinfo->versions_dataOffset[info.currentPolyVersion] = argImpl.offset;
            }
            _GLOG(log::out << "\n";)
        }
        if(function->parentStruct){
            // This doesn't need to be done here. Data offset is known in type checker since it 
            // depends on the struct type. The argument data offset is predictable. But
            // since the data offset for normal arguments are done here we might as well
            // to members here to for consistency.
            // TypeInfo* ti = info.ast->getTypeInfo(varinfo->versions_typeId[info.currentPolyVersion]);
            // ti->ast
            Assert(function->memberIdentifiers.size() == funcImpl->structImpl->members.size());
            for(int i=0;i<(int)function->memberIdentifiers.size();i++){
                auto identifier  = function->memberIdentifiers[i];
                if(!identifier) continue; // see type checker as to why this may happen
                auto& memImpl = funcImpl->structImpl->members[i];

                identifier->versions_dataOffset[info.currentPolyVersion] = memImpl.offset;
            }
        }
        int index_of_frame_size = 0;
        
        if(function->blank_body) {
            // builder.emit_alloc_local(BC_REG_INVALID, (u16)0);
        } else {
            builder.emit_alloc_local(BC_REG_INVALID, &index_of_frame_size);
            add_frame_fix(index_of_frame_size);
        }

        if (funcImpl->signature.returnTypes.size() != 0) {
            // We don't need to zero initialize return value.
            // You cannot return without specifying what to return.
            // If we have a feature where return values can be set like
            // local variables then we may need to rethink things.
        // #ifndef DISABLE_ZERO_INITIALIZATION
            // builder.emit_alloc_local(BC_REG_B, funcImpl->returnSize);
            // genMemzero(BC_REG_B, BC_REG_C, funcImpl->returnSize);
        // #else
        // #endif

            if(!function->blank_body) {
                // builder.emit_alloc_local(BC_REG_INVALID, funcImpl->signature.returnSize);
                

                // allocated_stack_space += funcImpl->signature.returnSize;
                info.currentFrameOffset -= funcImpl->signature.returnSize;
                currentFuncImpl->alloc_frame_space(funcImpl->signature.returnSize);
            }

            _GLOG(log::out << "Return values:\n";)
            for(int i=0;i<(int)funcImpl->signature.returnTypes.size();i++){
                auto& ret = funcImpl->signature.returnTypes[i];
                _GLOG(log::out << " " <<"["<<ret.offset<<"] " << ": " << info.ast->typeToString(ret.typeId) << "\n";)
            }
        }
        if(astStruct){
            for(int i=0;i<(int)astStruct->polyArgs.size();i++){
                auto& arg = astStruct->polyArgs[i];
                arg.virtualType->id = funcImpl->structImpl->polyArgs[i];
            }
        }
        for(int i=0;i<(int)function->polyArgs.size();i++){
            auto& arg = function->polyArgs[i];
            arg.virtualType->id = funcImpl->signature.polyArgs[i];
        }

        if(function->blank_body && ((astStruct && astStruct->polyArgs.size()) || function->polyArgs.size())) {
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("Specifying @blank for functions is a bad idea for polymorphic functions. @blank is meant to be used as a 'clean slate' for inline assembly.")
                if(function->polyArgs.size()) {
                    ERR_LINE2(function->location,"this function is polymorphic")
                    if(astStruct && astStruct->polyArgs.size()) {
                        ERR_LINE2(astStruct->location,"this parent struct is polymorphic")
                    }
                } else {
                    ERR_LINE2(astStruct->location,"this parent struct is polymorphic...")
                    ERR_LINE2(function->location,"...to this function")
                }
            )
        }

        // dfun->codeStart = info.bytecode->length();

        if(funcImpl->astFunction->name == compiler->entry_point) {
            if(function->blank_body) {
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG("The entry point function cannot be @blank because it's the entry point which initializes type information and global data.")
                    ERR_LINE2(function->location, "here")
                )
            }
            generatePreload();
        }
        if(function->callConvention != BETCALL) {
            if(funcImpl->signature.returnTypes.size() > 1) {
                // NOTE: This message isn't great because the default convention may change in the future and when it does I will forget to change this message.
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG_LOG(function->callConvention<<" cannot have more than 1 return type. Use @betcall for that (the default unless function has @import @export or you are targeting ARM).")
                    ERR_LINE2(function->location, "function call here")
                )
                return SIGNAL_FAILURE;
            }
            if(funcImpl->signature.returnTypes.size() > 0){
                int ret_type_size = info.ast->getTypeSize(funcImpl->signature.returnTypes[0].typeId);
                if(ret_type_size > REGISTER_SIZE) {
                    ERR_SECTION(
                        ERR_HEAD2(function->location)
                        ERR_MSG_LOG(function->callConvention<<" cannot return a type larger than the register size (currently "<<REGISTER_SIZE<<" when targeting "<<compiler->options->target<<").")
                        ERR_LINE2(function->location, "function call here")
                    )
                    return SIGNAL_FAILURE;
                }
            }
        }

        if (function->is_compiler_func) {
            if (function->name == "init_preload") {
                generatePreload();
            } else if (function->name == "global_slice") {
                BCRegister reg = BC_REG_A;
                builder.emit_dataptr(reg, 0);
                // -16 is hardcode, use sizeof slice.
                builder.emit_set_ret(reg, -16, REGISTER_SIZE, false);

                // NOTE: We return preallocated data which is the globals defined by the user instead
                //   of dataSegment.size() which also contains string literals and type information.
                //   We don't want users to modify those (of course they can but we make it harder
                //   by giving them less information).
                // builder.emit_li32(reg, bytecode->dataSegment.size());
                builder.emit_li32(reg, ast->preallocatedGlobalSpace());
                // NOTE: We don't need a fixup of the global data size because once we're in the generator phase
                //   all global data has already been reserved by the type checker.
                //   This may change in the future but for now this will work fine.
                // Assert(!compiler->global_size_relocation.valid()); // Setting this twice would be an error
                // compiler->global_size_relocation = builder.get_relocation(-4);

                builder.emit_set_ret(reg, -REGISTER_SIZE, REGISTER_SIZE, false);
            } else {
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG_COLORED("'"<<log::LIME<<function->name<<log::NO_COLOR<<"' is not a function from the compiler.")
                    ERR_LINE2(function->location,"here")
                )
            }
        } else {
            SignalIO result = generateBody(function->body);
        }

        // dfun->codeEnd = info.bytecode->length();

        for(int i=0;i<(int)function->polyArgs.size();i++){
            auto& arg = function->polyArgs[i];
            arg.virtualType->id = {}; // disable types
        }
        if(astStruct){
            for(int i=0;i<(int)astStruct->polyArgs.size();i++){
                auto& arg = astStruct->polyArgs[i];
                arg.virtualType->id = {}; // disable types
            }
        }

        if (funcImpl->signature.returnTypes.size() != 0 && !function->blank_body) { // blank body requires the user to manually return arguments through assembly. It's the user's fault if they did it incorrectly.
            // @compiler functions do not have bodies.
            if(function->body) {
                // check last statement for a return and "exit" early
                bool foundReturn = function->body->statements.size()>0 && 
                    function->body->statements.get(function->body->statements.size()-1) ->type == ASTStatement::RETURN;
                // TODO: A return statement might be okay in an inner scope and not necessarily the
                //  top scope.
                if(!foundReturn){
                    for(auto it : function->body->statements){
                        if (it->type == ASTStatement::RETURN) {
                            foundReturn = true;
                            break;
                        }
                    }
                    if (!foundReturn) {
                        ERR_SECTION(
                            ERR_HEAD2(function->location)
                            ERR_MSG("Missing return statement in '" << function->name << "'.")
                            ERR_LINE2(function->location,"put a return in the body")
                        )
                    }
                }
            }
        }
        // We can't skip ret like this the last ret may exist in a
        // if-block. That if block will jump to the instruction after the ret 
        // so WE MUST emit an extra ret.
        // TODO: We could optimize a bit if the last instruction doesn't come from
        //   an if, while, for, switch or whatever else we might have. This makes it hard.
        // if(builder.get_last_opcode() != BC_RET) { }
            
        // add return with no return values if it doesn't exist
        // this is only fine if the function doesn't return values
        if(function->body->statements.size() == 0 || function->body->statements.last()->type != ASTStatement::RETURN) {
            if(!function->blank_body) {
                int index;
                builder.emit_free_local(&index);
                add_frame_fix(index);
            }
            builder.emit_ret();
        }
        fix_frame_values(funcImpl, tinycode);
        // TODO: Assert that we haven't allocated more stack space than we freed!
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateFunctions(ASTScope* body){
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
    _GLOG(FUNC_ENTER)
    Assert(body || info.hasForeignErrors());
    if(!body) return SIGNAL_FAILURE;
    // MAKE_NODE_SCOPE(body); // we don't generate bytecode here so no need for this

    ScopeId savedScope = info.currentScopeId;
    defer { info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    for(auto it : body->namespaces) {
        generateFunctions(it);
    }
    for(auto it : body->functions) {
        if(it->body) {
            // External/native does not have body.
            // Not calling function reducess log messages
            generateFunctions(it->body);
        }
        if(it->contains_run_directive == gen_func_with_run_directives)
            generateFunction(it);
    }
    for(auto it : body->statements) {
        if(it->firstBody) {
            generateFunctions(it->firstBody);
        }
        if(it->secondBody) {
            generateFunctions(it->secondBody);
        }
    }
    for(auto it : body->structs) {
        for (auto function : it->functions) {
            Assert(function->body);
            generateFunctions(function->body);
            if(function->contains_run_directive == gen_func_with_run_directives)
                generateFunction(function, it);
        }
    }
    return SIGNAL_SUCCESS;
}

SignalIO GenContext::generateStatement(ASTStatement *statement) {
    using namespace engone;
    if (statement->type == ASTStatement::DECLARATION) {
        _GLOG(SCOPE_LOG("ASSIGN"))            

        if (statement->globalDeclaration) {
            if(currentPolyVersion == 0 && debugFunction){
                auto& varname = statement->varnames[0];
                debugFunction->addVar(varname.name, varname.identifier->versions_dataOffset[currentPolyVersion], varname.versions_assignType[currentPolyVersion], info.currentScopeDepth, varname.identifier->scopeId, true);
            }
            // global variables should not generate any local code.
            // expressions in globals are generated and evaluated once.
            return SIGNAL_SUCCESS;
        }
        
        auto& typesFromExpr = statement->versions_expressionTypes[info.currentPolyVersion];
        if(statement->firstExpression && typesFromExpr.size() < statement->varnames.size()) {
            if(!info.hasForeignErrors()){
                // char msg[100];
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("To many variables.")
                    // sprintf(msg,"%d variables",(int)statement->varnames.size());
                    ERR_LINE2(statement->location, statement->varnames.size() << " variables");
                    // sprintf(msg,"%d return values",(int)typesFromExpr.size());
                    ERR_LINE2(statement->firstExpression->location, typesFromExpr.size() << " return values");
                )
                Assert(false); // type checker should have handled this
            }
            return SIGNAL_FAILURE;
        }

        // This is not an error we want. You should be able to do this.
        // if(statement->globalDeclaration && statement->firstExpression && info.currentScopeId != info.ast->globalScopeId) {
        //     ERR_SECTION(
        //         ERR_HEAD2(statement->location)
        //         ERR_MSG("Assigning a value to a global variable inside a local scope is not allowed. Either move the variable to the global scope or separate the declaration and assignment with expression.")
        //         ERR_LINE2(statement->location,"bad")
        //         ERR_EXAMPLE(1,"global someName;")
        //         ERR_EXAMPLE(2,"someName = 3;")
        //     )
        //     return result;
        // }

        // for(int i = 0;i<(int)typesFromExpr.size();i++){
        //     TypeId typeFromExpr = typesFromExpr[i];
        //     if((int)statement->varnames.size() <= i){
        //         // _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(rightType)<<"\n";)
        //         continue;
        //     }

        int frameOffsetOfLastVarname = 0;

        for(int i = 0;i<(int)statement->varnames.size();i++){
            auto& varname = statement->varnames[i];
            // _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(rightType)<<"\n";)
            
            // NOTE: Global variables refer to memory in data segment.
            //  Each declaration of a variable which is global will get it's own memory.
            //  This means that polymorphic implementations will have multiple global variables
            //  for the same name, referring to different data. This is fine, the variables don't
            //  collide since the variables exist within a scope. Global variables accessed within
            //  a polymorphic function is also fine since the variable's type will stay the same.

            IdentifierVariable* varIdentifier = varname.identifier;
            if(!varIdentifier){
                Assert(info.hasForeignErrors());
                // Assert(info.errors!=0); // there should have been errors
                continue;
            }
            auto varinfo = varIdentifier;
            if(!varinfo){
                Assert(info.hasForeignErrors());
                // Assert(info.errors!=0); // there should have been errors
                continue;
            }
            
            // is this casting handled already? type checker perhaps?
            // if(!info.ast->castable(typeFromExpr, declaredType)) {
            //     ERRTYPE(statement->location, declaredType, typeFromExpr, "(assign)\n";
            //         ERR_LINE2(statement->location,"bad");
            //     )
            //     continue;
            // }

            // i32 rightSize = info.ast->getTypeSize(typeFromExpr);
            // i32 leftSize = info.ast->getTypeSize(var->typeId);
            // i32 asize = info.ast->getTypeAlignedSize(var->typeId);

            int alignment = 0;
            if (varname.arrayLength>0){
                // TODO: Fix arrays with static data
                if(statement->firstExpression) {
                    ERR_SECTION(
                        ERR_HEAD2(statement->firstExpression->location)
                        ERR_MSG("An expression is not allowed when declaring an array on the stack. The array is zero-initialized by default.")
                        ERR_LINE2(statement->firstExpression->location, "bad")
                    )
                    continue;
                }
                // Assert(("Arrays disabled due to refactoring of assignments",false));
                // I have not refactored arrays. Do that. Probably not a lot of working. Mostly
                // Checking that it works as it should and handle any errors. I don't think arrays
                // were properly implemented before.

                // make sure type is a slice?
                // it will always be at the moment of writing since arrayLength is only set
                // when slice is used but this may not be true in the future.
                int arrayFrameOffset = 0;
                TypeInfo *typeInfo = info.ast->getTypeInfo(varinfo->versions_typeId[info.currentPolyVersion].baseType());
                TypeId elementType = typeInfo->structImpl->polyArgs[0];
                if(!elementType.isValid())
                    continue; // error message should have been printed in type checker
                i32 elementSize = info.ast->getTypeSize(elementType);
                // i32 asize2 = info.ast->getTypeAlignedSize(elementType);
                int arraySize = elementSize * varname.arrayLength;
                
                // Assert(size2 * varname.arrayLength <= pow(2,16)/2);
                if(arraySize > pow(2,16)/2) {
                    // std::string msg = std::to_string(size2) + " * "+ std::to_string(varname.arrayLength) +" = "+std::to_string(arraySize);
                    ERR_SECTION(
                        ERR_HEAD2(statement->location)
                        ERR_MSG((int)(pow(2,16)/2-1) << " is the maximum size of arrays on the stack. "<<(arraySize)<<" was used which exceeds that. The limit comes from the instruction BC_INCR which uses a signed 16-bit integer.")
                        ERR_LINE2(statement->location, elementSize << " * " << std::to_string(varname.arrayLength) << " = " << std::to_string(arraySize))
                    )
                    continue;
                }

                int diff = arraySize % REGISTER_SIZE;
                if(diff != 0){
                    arraySize += REGISTER_SIZE - diff;
                }
                Assert(info.currentFrameOffset%REGISTER_SIZE == 0);
                
                info.currentFrameOffset -= arraySize;
                arrayFrameOffset = info.currentFrameOffset;
                currentFuncImpl->alloc_frame_space(arraySize);
                // BCRegister reg_data = BC_REG_C;
                // builder.emit_alloc_local(reg_data, arraySize);
                
                if(i == (int)statement->varnames.size()-1){
                    frameOffsetOfLastVarname = arrayFrameOffset;
                }

                bool set_defaults = false;
                if(elementType.isNormalType()) {
                    TypeInfo* elementInfo = info.ast->getTypeInfo(elementType);
                    if(elementInfo->astStruct) {
                        set_defaults = true;
                        // TODO: Annotation to disable this
                        // TODO: Create a loop with cmp, je, jmp instructions instead of
                        //  "unrolling" the loop like this. We generate a lot of instructions from this.
                        for(int j = 0;j<varname.arrayLength;j++) {
                            SignalIO result = generateDefaultValue(BC_REG_LOCALS, arrayFrameOffset + elementSize * j, elementType);
                            // SignalIO result = generateDefaultValue(BC_REG_BP, arrayFrameOffset + elementSize * j, elementType);
                            if(result!=SIGNAL_SUCCESS)
                                return SIGNAL_FAILURE;
                        }
                    }
                }
                if(!set_defaults) {
                    #ifndef DISABLE_ZERO_INITIALIZATION
                    genMemzero(BC_REG_LOCALS, BC_REG_B, arraySize, arrayFrameOffset);
                    #endif // DISABLE_ZERO_INITIALIZATION
                }
                // data type may be zero if it wasn't specified during initial assignment
                // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                // if (diff != asize) {
                //     info.currentFrameOffset -= diff; // align
                // }
                // info.currentFrameOffset -= size;
                // var->frameOffset = info.currentFrameOffset;

                
                SignalIO result = framePush(varinfo->versions_typeId[info.currentPolyVersion],&varinfo->versions_dataOffset[info.currentPolyVersion],false, varinfo->isGlobal());

                // TODO: Don't hardcode this slice stuff, maybe I have to.
                // push length
                builder.emit_li32(BC_REG_D,varname.arrayLength);
                builder.emit_push(BC_REG_D);

                // push ptr
                // builder.emit_li32(BC_REG_B, arrayFrameOffset);
                // builder.emit_add(BC_REG_B, BC_REG_BP, false, REGISTER_SIZE);
                builder.emit_ptr_to_locals(BC_REG_B, arrayFrameOffset);
                builder.emit_push(BC_REG_B);

                generatePop(BC_REG_LOCALS, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                // generatePop(BC_REG_BP, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                
                if(debugFunction)
                    debugFunction->addVar(varname.name,
                        varinfo->versions_dataOffset[info.currentPolyVersion],
                        varinfo->versions_typeId[info.currentPolyVersion],
                        info.currentScopeDepth,
                        varname.identifier->scopeId);
            } else if(varname.declaration) {
                if(!varinfo->isGlobal()) {
                    // address of global variables is managed in type checker
                    SignalIO result = framePush(varinfo->versions_typeId[info.currentPolyVersion], &varinfo->versions_dataOffset[info.currentPolyVersion],
                        !statement->firstExpression, false);
                    // log::out << "decl " << varname.name << " " << varinfo->versions_dataOffset[info.currentPolyVersion] << " "<<currentFuncImpl->_max_size_of_locals << " "<<currentFuncImpl->_size_of_locals<<"\n";
                    if(debugFunction)
                        debugFunction->addVar(varname.name,
                            varinfo->versions_dataOffset[info.currentPolyVersion],
                            varinfo->versions_typeId[info.currentPolyVersion],
                            info.currentScopeDepth,
                            varname.identifier->scopeId);
                }
                _GLOG(log::out << "declare " << (varinfo->isGlobal()?"global ":"")<< varname.name << " at " << varinfo->versions_dataOffset[info.currentPolyVersion] << "\n";)
            } else {
                _GLOG(log::out << "assign " << (varinfo->isGlobal()?"global ":"")<< varname.name << " at " << varinfo->versions_dataOffset[info.currentPolyVersion] << "\n";)
            }
            if(varinfo){
                _GLOG(log::out << " " << varname.name << " : " << info.ast->typeToString(varinfo->versions_typeId[info.currentPolyVersion]) << "\n";)
            }
        }
        TEMP_ARRAY_N(TypeId, rightTypes, 5);
        if (statement->arrayValues.size()){
            auto& varname = statement->varnames.last();
            TypeInfo* sometypeInfo = info.ast->getTypeInfo(varname.versions_assignType[info.currentPolyVersion].baseType());
            TypeId elementType = sometypeInfo->structImpl->polyArgs[0];
            int elementSize = info.ast->getTypeSize(elementType);
            for(int j=0;j<(int)statement->arrayValues.size();j++){
                ASTExpression* value = statement->arrayValues[j];

                rightTypes.resize(0);
                SignalIO result = generateExpression(value, &rightTypes);
                if (result != SIGNAL_SUCCESS) {
                    continue;
                }

                if(rightTypes.size()!=1) {
                    Assert(info.hasForeignErrors());
                    continue; // error handled in type checker
                }

                // TypeId stateTypeId = varname.versions_assignType[info.currentPolyVersion];
                IdentifierVariable* varinfo = varname.identifier;
                if(!varinfo){
                    Assert(info.errors!=0); // there should have been errors
                    continue;
                }

                if(!performSafeCast(rightTypes[0], elementType)){
                    Assert(info.hasForeignErrors());
                    continue;
                }
                switch(varinfo->type) {
                    case Identifier::GLOBAL_VARIABLE: {
                        Assert(false); // broken with arrays
                        // builder.emit_dataptr(BC_REG_B, );
                        // info.addImm(varinfo->versions_dataOffset[info.currentPolyVersion]);
                        // GeneratePop(info, BC_REG_B, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                        break; 
                    }
                    case Identifier::LOCAL_VARIABLE: {
                        // builder.emit_li32(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                        // builder.emit_({BC_ADDI, BC_REG_BP, BC_REG_B, BC_REG_B});
                        generatePop(BC_REG_LOCALS, frameOffsetOfLastVarname + j * elementSize, elementType);
                        // generatePop(BC_REG_BP, frameOffsetOfLastVarname + j * elementSize, elementType);
                        break;
                    }
                    case Identifier::MEMBER_VARIABLE: {
                        Assert(false); // broken with arrays, this should probably not be allowed
                        // Assert(info.currentFunction && info.currentFunction->parentStruct);
                        // // TODO: Verify that  you
                        // // NOTE: Is member variable/argument always at this offset with all calling conventions?
                        // builder.emit_({BC_MOV_MR_DISP32, BC_REG_BP, BC_REG_B, REGISTER_SIZE});
                        // info.addImm(GenContext::FRAME_SIZE);
                        
                        // // builder.emit_li32(BC_REG_A, varinfo->versions_dataOffset[info.currentPolyVersion]);
                        // // builder.emit_({BC_ADDI, BC_REG_B, BC_REG_A, BC_REG_B});
                        // GeneratePop(info, BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                        break;
                    }
                    default: {
                        Assert(false);
                    }
                }
            }
        }
        if(statement->firstExpression){
            SignalIO result = generateExpression(statement->firstExpression, &rightTypes);
            if (result != SIGNAL_SUCCESS) {
                // assign fails and variable will not be declared potentially causing
                // undeclared errors in later code. This is probably better than
                // declaring the variable and using void type. Then you get type mismatch
                // errors.
                return result;
            }

            // Make sure Type checker and generator produce the same types, otherwise there's a bug
            Assert(typesFromExpr.size()==rightTypes.size());
            bool cont = false;
            for(int i=0;i<(int)typesFromExpr.size();i++){
                std::string a0 = info.ast->typeToString(typesFromExpr[i]);
                std::string a1 = info.ast->typeToString(rightTypes[i]);
                // Assert(typesFromExpr[i] == rightTypes[i]);
                if(typesFromExpr[i] != rightTypes[i]) {
                    Assert(info.hasForeignErrors());
                    if(!info.hasForeignErrors()) {
                        ERR_SECTION(
                            ERR_HEAD2(statement->firstExpression->location)
                            ERR_MSG("Compiler bug sorry! Type checker and generator produced different types '"<<a0<<"' != '"<<a1<<"' (type checker != generator).")
                            ERR_LINE2(statement->firstExpression->location, "here")
                        )
                    }
                    cont=true;
                    continue;
                }
            }
            // pop and cast the generates types to correct types and into the variable location
            if(cont) return SIGNAL_FAILURE;
            for(int i = (int)typesFromExpr.size()-1;i>=0;i--){
                TypeId typeFromExpr = typesFromExpr[i];
                if((int)statement->varnames.size() <= i){
                    // TODO: Sometimes we don't want to ignore values like this but sometimes it's convenient.
                    //   We need an annotation somewhere that throws an error if this happens.
                    //   Probably on the function definition. @handle_all_return_values
                    _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(typeFromExpr)<<"\n";)
                    generatePop(BC_REG_INVALID, 0, typeFromExpr);
                    continue;
                }
                auto& varname = statement->varnames[i];
                _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(typeFromExpr)<<"\n";)
                
                TypeId stateTypeId = varname.versions_assignType[info.currentPolyVersion];
                IdentifierVariable* varinfo = varname.identifier;

                if(!varinfo){
                    Assert(info.hasForeignErrors());
                    continue;
                }

                if(!performSafeCast(typeFromExpr, varinfo->versions_typeId[info.currentPolyVersion])){
                    if(!info.hasForeignErrors()){
                        ERRTYPE1(statement->location, typeFromExpr, varinfo->versions_typeId[info.currentPolyVersion], "(assign)."
                            // ERR_LINE2(statement->location,"bad");
                        )
                    }
                    continue;
                }
                // Assert(!var->globalData || info.currentScopeId == info.ast->globalScopeId);
                switch(varinfo->type) {
                    case Identifier::GLOBAL_VARIABLE: {
                        Assert(false); 
                        // possible bug with polyversions and globals?
                        if(varinfo->declaration && varinfo->declaration->isImported()) {

                        } else {
                            builder.emit_dataptr(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                        }
                        generatePop(BC_REG_B, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                    } break; 
                    case Identifier::LOCAL_VARIABLE: {
                        // builder.emit_li32(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                        // builder.emit_({BC_ADDI, BC_REG_BP, BC_REG_B, BC_REG_B});
                        generatePop(BC_REG_LOCALS, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                        // generatePop(BC_REG_BP, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                    } break;
                    case Identifier::MEMBER_VARIABLE: {
                        Assert(info.currentFunction && info.currentFunction->parentStruct);
                        // TODO: Verify that  you
                        // NOTE: Is member variable/argument always at this offset with all calling conventions?
                        
                    // type = varinfo->versions_typeId[info.currentPolyVersion];
                        builder.emit_get_param(BC_REG_B, 0, REGISTER_SIZE, false); // pointer

                        auto& mem = currentFunction->parentStruct->members[varinfo->memberIndex];
                        if (mem.array_length) {
                            ERR_SECTION(
                                ERR_HEAD2(statement->location)
                                ERR_MSG("You cannot assing values to a struct member that is an array.")
                                ERR_LINE2(statement->location,"here")
                            )
                        }
                        auto type = varinfo->versions_typeId[info.currentPolyVersion];
                        builder.emit_get_param(BC_REG_B, 0, REGISTER_SIZE, AST::IsDecimal(type));
                        // builder.emit_mov_rm_disp(BC_REG_B, BC_REG_BP, REGISTER_SIZE, GenContext::FRAME_SIZE);
                        
                        // builder.emit_li32(BC_REG_A,varinfo->versions_dataOffset[info.currentPolyVersion]);
                        // builder.emit_({BC_ADDI, BC_REG_B, BC_REG_A, BC_REG_B});
                        generatePop(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                    } break;
                    case Identifier::ARGUMENT_VARIABLE: {
                        Assert(false);
                    } break;
                    default: {
                        Assert(false);
                    }
                }
            }
        }
    }
    else if (statement->type == ASTStatement::IF) {
        _GLOG(SCOPE_LOG("IF"))

        // BREAK(statement->firstExpression->nodeId == 67)
        TypeId dtype = {};
        TEMP_ARRAY_N(TypeId, tempTypes, 5);
        
        SignalIO result = generateExpression(statement->firstExpression, &tempTypes);
        if(tempTypes.size()) dtype = tempTypes.last();
        if (result != SIGNAL_SUCCESS) {
            // generate body anyway or not?
            return result;
        }

        if(!performSafeCast(dtype, TYPE_BOOL)) {
            ERR_SECTION(
                ERR_HEAD2(statement->firstExpression->location)
                ERR_MSG_COLORED("The type '"<<log::GREEN<<info.ast->typeToString(dtype)<<log::NO_COLOR<<"' cannot be converted to a boolean which if-statements require.")
                ERR_LINE2(statement->firstExpression->location, "not a bool type")
            )
            generatePop(BC_REG_INVALID, 0, dtype);
            return SIGNAL_FAILURE;
        }

        BCRegister reg = BC_REG_A;

        builder.emit_pop(reg);
        int skipIfBodyIndex;
        // log::out << "beg pc " << builder.get_pc()<<"\n";
        builder.emit_jz(reg, &skipIfBodyIndex);

        result = generateBody(statement->firstBody);
        if (result != SIGNAL_SUCCESS)
            return result;

        int skipElseBodyIndex = 0;
        if (statement->secondBody) {
            builder.emit_jmp(&skipElseBodyIndex);
        }

        // fix address for jump instruction
        // log::out << "end pc " << builder.get_pc()<<"\n";
        builder.fix_jump_imm32_here(skipIfBodyIndex);

        if (statement->secondBody) {
            result = generateBody(statement->secondBody);
            if (result != SIGNAL_SUCCESS)
                return result;

            builder.fix_jump_imm32_here(skipElseBodyIndex);
        }
    } else if (statement->type == ASTStatement::SWITCH) {
        _GLOG(SCOPE_LOG("SWITCH"))

        // TODO: This is not a loop scope, continue is not allowed
        GenContext::LoopScope* loopScope = info.pushLoop();
        defer {
            _GLOG(log::out << "pop loop\n");
            bool yes = info.popLoop();
            if(!yes){
                log::out << log::RED << "popLoop failed (bug in compiler)\n";
            }
        };

        _GLOG(log::out << "push loop\n");
        loopScope->stackMoment = currentFrameOffset;

        TypeId exprType{};
        if(statement->versions_expressionTypes[info.currentPolyVersion].size()!=0) {
            exprType = statement->versions_expressionTypes[info.currentPolyVersion][0];
        } else {
            Assert(false);
            // return result; // error message printed already?
        }

        i32 switchValueOffset = 0; // frame offset
        framePush(exprType, &switchValueOffset,false, false);
        
        TypeId dtype = {};
        TEMP_ARRAY_N(TypeId, tempTypes, 5);
        SignalIO result = generateExpression(statement->firstExpression, &tempTypes);
        if(tempTypes.size()) dtype = tempTypes.last();
        if (result != SIGNAL_SUCCESS) {
            return result;
        }
        Assert(exprType == dtype);
        auto* typeInfo = info.ast->getTypeInfo(dtype);
        Assert(typeInfo);
        
        if(AST::IsInteger(dtype) || typeInfo->astEnum) {
            
        } else {
            // TODO: Allow Slice<char> at some point
            ERR_SECTION(
                ERR_HEAD2(statement->location)
                ERR_MSG_COLORED("You can only switch on integers and enums. Not '"<<log::LIME << ast->typeToString(dtype)<<log::NO_COLOR<<"'.")
                ERR_LINE2(statement->location, "invalid switch type")
            )
            return SIGNAL_FAILURE;
        }
        u32 switchExprSize = info.ast->getTypeSize(dtype);
        if (!switchExprSize) {
            // TODO: Print error? or does generate expression do that since it gives us dtype?
            return SIGNAL_FAILURE;
        }
        Assert(switchExprSize <= REGISTER_SIZE);
        
        BCRegister switchValueReg = BC_REG_D;
        builder.emit_pop(switchValueReg);
        builder.emit_mov_mr_disp(BC_REG_LOCALS, switchValueReg, switchExprSize, switchValueOffset);
        
        struct RelocData {
            int caseJumpAddress = 0;
            int caseBreakAddress = 0;
        };
        DynamicArray<RelocData> caseData{};
        caseData.resize(statement->switchCases.size());

        // DynamicArray<int> members_not_covered{}; // covered members (missing members) is handled in type checker.
        
        for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
            auto it = &statement->switchCases[nr];
            caseData[nr] = {};
            
            TypeId dtype = {};
            u32 size = 0;
            BCRegister caseValueReg = BC_REG_INVALID;
            bool wasMember = false;
            if(typeInfo->astEnum && it->caseExpr->type == EXPR_IDENTIFIER) {
                auto id_expr = it->caseExpr->as<ASTExpressionIdentifier>();
                int index = -1;
                bool yes = typeInfo->astEnum->getMember(id_expr->name, &index);
                if(yes) {
                    // members_not_covered.add(index);
                    wasMember = true;
                    dtype = typeInfo->id;
                    
                    size = info.ast->getTypeSize(dtype);
                    caseValueReg = BC_REG_A;

                    // TODO: Test if enum of different sizes work as expected. Does the x64-generator handle the register sizes properly?                        
                    builder.emit_li(caseValueReg, typeInfo->astEnum->members[index].enumValue, size);
                }
            }
            bool touched_switch_reg = false;
            if(!wasMember) {
                touched_switch_reg = true; // generateExpression might have modified the switch register
                TEMP_ARRAY_N(TypeId, tempTypes, 5)
                SignalIO result = generateExpression(it->caseExpr, &tempTypes);
                if(tempTypes.size()) dtype = tempTypes.last();
                if (result != SIGNAL_SUCCESS) {
                    continue;
                }
                size = info.ast->getTypeSize(dtype);
                caseValueReg = BC_REG_A;
                
                builder.emit_pop(caseValueReg);
            }
            if(touched_switch_reg)
                builder.emit_mov_rm_disp(switchValueReg, BC_REG_LOCALS, (int)size, switchValueOffset);
            
            builder.emit_eq(caseValueReg, switchValueReg, size, false);
            builder.emit_jnz(caseValueReg, &caseData[nr].caseJumpAddress);
        }

        int pc_of_default_case = builder.get_pc();

        // Generate default cause if we have one
        if(statement->firstBody) {
            result = generateBody(statement->firstBody);
            // if (result != SIGNAL_SUCCESS)
            //     continue;
        }
        int noCaseJumpAddress;
        builder.emit_jmp(&noCaseJumpAddress);
        
        /* TODO: Cases without a body does not fall but perhaps they should.
        I can think of a scenario where implicit fall would be confusing.
        If you have a bunch of cases, none use @fall, and you comment out a body
        or remove it you may not realize that empty bodies will fall to next
        statements. You become irritated and put a nop statment. Alternative is 
        @fall after each case which isn't ideal either but it's consistent and it's
        fast to type. NO IMPLICIT FALLS FOR NOW. -Emarioo, 2024-01-10
        code.
            case 1:     <- implicit fall would be nifty
            case 2:
                say_hi()
            case 4:
                no()
        */
        // i32 address_prevFallJump = -1;
        
        auto& list = statement->switchCases;
        for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
            auto it = &statement->switchCases[nr];
            builder.fix_jump_imm32_here(caseData[nr].caseJumpAddress);
            
            result = generateBody(it->caseBody);
            if (result != SIGNAL_SUCCESS)
                continue;
            
            if(!it->fall) {
                // implicit break
                builder.emit_jmp(&caseData[nr].caseBreakAddress);
            } else {
                if (nr == statement->switchCases.size()-1) {
                    // last case should fall through to the default case
                    builder.emit_jmp(pc_of_default_case);
                    // caseData[nr].caseBreakAddress = pc_of_default_case;
                } else {
                    caseData[nr].caseBreakAddress = 0;
                }
            }
        }
        builder.fix_jump_imm32_here(noCaseJumpAddress);
            
        for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
            if(caseData[nr].caseBreakAddress != 0)
                builder.fix_jump_imm32_here(caseData[nr].caseBreakAddress);
        }

        if(loopScope->stackMoment != currentFrameOffset) {
            // builder.emit_free_local(loopScope->stackMoment - currentFrameOffset);
            currentFuncImpl->free_frame_space(loopScope->stackMoment - currentFrameOffset);
            currentFrameOffset = loopScope->stackMoment;
        }
        
        for (auto ind : loopScope->resolveBreaks) {
            builder.fix_jump_imm32_here(ind);
        }

        
    } else if (statement->type == ASTStatement::WHILE) {
        _GLOG(SCOPE_LOG("WHILE"))

        GenContext::LoopScope* loopScope = info.pushLoop();
        defer {
            _GLOG(log::out << "pop loop\n");
            bool yes = info.popLoop();
            if(!yes){
                log::out << log::RED << "popLoop failed (bug in compiler)\n";
            }
        };

        _GLOG(log::out << "push loop\n");
        loopScope->continueAddress = builder.get_pc();
        loopScope->stackMoment = currentFrameOffset;
        // loopScope->stackMoment = builder.saveStackMoment();

        SignalIO result{};
        if(statement->firstExpression) {
            TypeId dtype = {};
            TEMP_ARRAY_N(TypeId, tempTypes, 5)
            result = generateExpression(statement->firstExpression, &tempTypes);
            if(tempTypes.size()) dtype = tempTypes.last();
            if (result != SIGNAL_SUCCESS) {
                // generate body anyway or not?
                return result;
            }
            if(!performSafeCast(dtype, TYPE_BOOL)) {
                ERR_SECTION(
                    ERR_HEAD2(statement->firstExpression->location)
                    ERR_MSG_COLORED("The type '"<<log::GREEN<<ast->typeToString(dtype)<<log::NO_COLOR<<"' cannot be converted to a boolean which is the accepted type for conditions.")
                    ERR_LINE2(statement->firstExpression->location,"here")
                )
                return SIGNAL_FAILURE;
            }
            u32 size = info.ast->getTypeSize(dtype);
            BCRegister reg = BC_REG_A;

            builder.emit_pop(reg);
            loopScope->resolveBreaks.add(0);
            builder.emit_jz(reg, &loopScope->resolveBreaks.last());
        } else {
            // infinite loop
        }

        result = generateBody(statement->firstBody);
        if (result != SIGNAL_SUCCESS)
            return result;

        builder.emit_jmp(loopScope->continueAddress);

        for (auto ind : loopScope->resolveBreaks) {
            builder.fix_jump_imm32_here(ind);
        }
        
        // TODO: Should this error exist? We need to check for returns too.
        // if(!statement->firstExpression && loopScope->resolveBreaks.size() == 0) {
        //     ERR_SECTION(
        //         ERR_HEAD2(statement->location)
        //         ERR_MSG("A true infinite loop without any break statements is not allowed. If this was intended, write 'while true' or put 'if false break' in the body of the loop.")
        //         ERR_LINE2(statement->location,"true infinite loop")
        //     )
        // }

        // pop loop happens in defer
        // _GLOG(log::out << "pop loop\n");
        // bool yes = info.popLoop();
        // if(!yes){
        //     log::out << log::RED << "popLoop failed (bug in compiler)\n";
        // }
    } else if (statement->type == ASTStatement::FOR) {
        _GLOG(SCOPE_LOG("FOR"))

        GenContext::LoopScope* loopScope = info.pushLoop();
        defer {
            _GLOG(log::out << "pop loop\n");
            bool yes = info.popLoop();
            if(!yes){
                log::out << log::RED << "popLoop failed (bug in compiler)\n";
            }
        };

        int stackBeforeLoop = currentFrameOffset; // loopScope->stackMoment is used by break and continue to restore stack allocated variables in the scope of the loop. However, stack management has been changed where we allocate all stack space at start of function so break and continue doesn't clean it up anywhere. We could therefore use loopScope->stackMoment instead of a stackBeforeLoop but this might once again change in the future so we'll keep it as it is.

        // body scope is used since the for loop's variables
        // shouldn't collide with the variables in the current scope.
        // not sure how well this works, we shall see.
        ScopeId scopeForVariables = statement->firstBody->scopeId;

        Assert(statement->varnames.size()==2);

        // NOTE: We add debug information last because varinfo does not have the frame offsets yet.

        if(statement->forLoopType == RANGED_FOR_LOOP){
            auto& varnameNr = statement->varnames[0];
            if(!varnameNr.identifier) {
                Assert(hasAnyErrors());
                return SIGNAL_FAILURE;
            }
            auto varinfo_index = varnameNr.identifier;
            {
                TypeId typeId = TYPE_INT32; // you may want to use the type in varname, the reason i don't is because
                framePush(typeId, &varinfo_index->versions_dataOffset[info.currentPolyVersion],false, false);

                TypeId dtype = {};
                // Type should be checked in type checker and further down
                // when extracting ptr and len. No need to check here.
                SignalIO result = generateExpression(statement->firstExpression, &dtype);
                if (result != SIGNAL_SUCCESS) {
                    return result;
                }
                if(statement->isReverse()){
                    builder.emit_pop(BC_REG_D); // throw range.now
                    builder.emit_pop(BC_REG_A);
                }else{
                    builder.emit_pop(BC_REG_A);
                    builder.emit_pop(BC_REG_D); // throw range.end
                    builder.emit_incr(BC_REG_A, -1);
                    // decrement by one since for loop increments
                    // before going into the loop
                }
                builder.emit_push(BC_REG_A);
                generatePop(BC_REG_LOCALS, varinfo_index->versions_dataOffset[info.currentPolyVersion], typeId);
            }

            _GLOG(log::out << "push loop\n");
            // loopScope->stackMoment = currentFrameOffset;
            loopScope->continueAddress = builder.get_pc();

            TypeId dtype = {};
            TEMP_ARRAY_N(TypeId, tempTypes, 5)
            SignalIO result = generateExpression(statement->firstExpression, &tempTypes);
            if(tempTypes.size()) dtype = tempTypes.last();
            if (result != SIGNAL_SUCCESS) {
                return result;
            }
            BCRegister index_reg = BC_REG_C;
            BCRegister length_reg = BC_REG_D;
            int int_size = 4;

            if(statement->isReverse()){
                builder.emit_pop(length_reg); // range.now we care about
                builder.emit_pop(BC_REG_A);
            } else {
                builder.emit_pop(BC_REG_A); // throw range.now
                builder.emit_pop(length_reg); // range.end we care about
            }
    
            builder.emit_mov_rm_disp(index_reg, BC_REG_LOCALS, int_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

            if(statement->isReverse()){
                builder.emit_incr(index_reg, -1);
            }else{
                builder.emit_incr(index_reg, 1);
            }
            
            builder.emit_mov_mr_disp(BC_REG_LOCALS, index_reg, int_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

            if(statement->isReverse()){
                builder.emit_gte(index_reg,length_reg, int_size, false, true);
            } else {
                builder.emit_lt(index_reg,length_reg, int_size, false, true);
            }
            loopScope->resolveBreaks.add(0);
            builder.emit_jz(index_reg, &loopScope->resolveBreaks.last());

            result = generateBody(statement->firstBody);
            if (result != SIGNAL_SUCCESS)
                return result;

            builder.emit_jmp(loopScope->continueAddress);

            for (auto ind : loopScope->resolveBreaks) {
                builder.fix_jump_imm32_here(ind);
            }
            if(debugFunction)
                debugFunction->addVar(varnameNr.name,
                    varinfo_index->versions_dataOffset[info.currentPolyVersion],
                    varinfo_index->versions_typeId[info.currentPolyVersion],
                    info.currentScopeDepth + 1,
                    varnameNr.identifier->scopeId);
        } else if (statement->forLoopType == SLICED_FOR_LOOP) {
            auto& varnameIt = statement->varnames[0];
            auto& varnameNr = statement->varnames[1];
            if(!varnameIt.identifier || !varnameNr.identifier) {
                Assert(hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            auto varinfo_item  = varnameIt.identifier;
            auto varinfo_index = varnameNr.identifier;

            {
                SignalIO result = framePush(varinfo_index->versions_typeId[info.currentPolyVersion], &varinfo_index->versions_dataOffset[info.currentPolyVersion], false, false);

                if(statement->isReverse()){
                    TypeId dtype = {};
                    // Type should be checked in type checker and further down
                    // when extracting ptr and len. No need to check here.
                    SignalIO result = generateExpression(statement->firstExpression, &dtype);
                    if (result != SIGNAL_SUCCESS) {
                        return result;
                    }
                    int size = ast->getTypeSize(dtype);
                    builder.emit_pop(BC_REG_D); // throw ptr
                    builder.emit_pop(BC_REG_T0); // keep len
                    builder.emit_cast(BC_REG_A, BC_REG_T0, CAST_UINT_SINT, size, size);
                }else{
                    // it is important that we load li64 to fill the CPU register so that we get a negative number and not a
                    // large 32-bit number in 64-bit register
                    builder.emit_li64(BC_REG_A,-1);
                }
                builder.emit_push(BC_REG_A);

                Assert(varinfo_index->versions_typeId[info.currentPolyVersion] == TYPE_INT64);

                generatePop(BC_REG_LOCALS,varinfo_index->versions_dataOffset[info.currentPolyVersion],varinfo_index->versions_typeId[info.currentPolyVersion]);
            }
            i32 itemsize = info.ast->getTypeSize(varnameIt.versions_assignType[info.currentPolyVersion]);

            {
                i32 asize = info.ast->getTypeAlignedSize(varinfo_item->versions_typeId[info.currentPolyVersion]);
                if(asize==0)
                    return SIGNAL_FAILURE;

                SignalIO result = framePush(varinfo_item->versions_typeId[info.currentPolyVersion], &varinfo_item->versions_dataOffset[info.currentPolyVersion], true, false);
            }

            _GLOG(log::out << "push loop\n");
            // loopScope->stackMoment = currentFrameOffset;
            loopScope->continueAddress = builder.get_pc();

            // TODO: don't generate ptr and length everytime.
            // put them in a temporary variable or something.
            TypeId dtype = {};
            TEMP_ARRAY_N(TypeId, tempTypes, 5)
            SignalIO result = generateExpression(statement->firstExpression, &tempTypes);
            if(tempTypes.size()) dtype = tempTypes.last();
            if (result != SIGNAL_SUCCESS) {
                return result;
            }
            TypeInfo* ti = info.ast->getTypeInfo(dtype);
            Assert(ti && ti->astStruct && ti->astStruct->name == "Slice");
            auto memdata_ptr = ti->getMember("ptr");
            auto memdata_len = ti->getMember("len");

            // NOTE: careful when using registers since you might use 
            //  one for multiple things. 
            BCRegister ptr_reg = BC_REG_F;
            BCRegister length_reg = BC_REG_B;
            BCRegister index_reg = BC_REG_C;

            builder.emit_pop(ptr_reg);
            builder.emit_pop(length_reg);

            int ptr_size = ast->getTypeSize(memdata_ptr.typeId);
            // int index_size = ast->getTypeSize(memdata_len.typeId);
            int operand_size = ptr_size;

            builder.emit_mov_rm_disp(index_reg, BC_REG_LOCALS, operand_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);
            if(statement->isReverse()){
                builder.emit_incr(index_reg, -1);
            }else{
                builder.emit_incr(index_reg, 1);
            }
            builder.emit_mov_mr_disp(BC_REG_LOCALS, index_reg, operand_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

            // NOTE: length_reg is modified here because it's not needed.
            if(statement->isReverse()){
                builder.emit_li(length_reg, 0, operand_size); // length reg is not used with reversed
                builder.emit_lte(length_reg,index_reg, operand_size, false, true);
            } else {
                // index < length, we do length > index because index_reg is used later, length_reg is free to use though
                builder.emit_gt(length_reg,index_reg, operand_size, false, true);
            }
            loopScope->resolveBreaks.add(0);
            builder.emit_jz(length_reg, &loopScope->resolveBreaks.last());

            // BE VERY CAREFUL SO YOU DON'T USE BUSY REGISTERS AND OVERWRITE
            // VALUES. UNEXPECTED VALUES WILL HAPPEN AND IT WILL BE PAINFUL.
            
            // NOTE: index_reg is needed here and should not be modified before.
            if(statement->isPointer()){
                if(itemsize>1){
                    builder.emit_li32(BC_REG_A, itemsize);
                    builder.emit_mul(BC_REG_A, index_reg, operand_size, false, true);
                } else {
                    builder.emit_mov_rr(BC_REG_A, index_reg);
                }
                builder.emit_add(ptr_reg, BC_REG_A, operand_size, false);

                builder.emit_mov_mr_disp(BC_REG_LOCALS, ptr_reg, operand_size, varinfo_item->versions_dataOffset[info.currentPolyVersion]);
            } else {
                if(itemsize>1){
                    builder.emit_li(BC_REG_A,itemsize, operand_size);
                    builder.emit_mul(BC_REG_A, index_reg, operand_size, false, true);
                } else {
                    builder.emit_mov_rr(BC_REG_A, index_reg);
                }
                builder.emit_add(ptr_reg, BC_REG_A, operand_size, false);

                builder.emit_ptr_to_locals(BC_REG_E, varinfo_item->versions_dataOffset[info.currentPolyVersion]);
                
                // builder.emit_li(BC_REG_B,itemsize,operand_size);
                // builder.emit_memcpy(BC_REG_E, ptr_reg, BC_REG_B);
                genMemcpy(BC_REG_E, ptr_reg, itemsize);
            }

            result = generateBody(statement->firstBody);
            if (result != SIGNAL_SUCCESS)
                return result;

            builder.emit_jmp(loopScope->continueAddress);

            for (auto ind : loopScope->resolveBreaks) {
                builder.fix_jump_imm32_here(ind);
            }
            
            // only add 'it' for Sliced loop (not ranged)
            if(debugFunction) {
                debugFunction->addVar(varnameIt.name,
                    varinfo_item->versions_dataOffset[info.currentPolyVersion],
                    varinfo_item->versions_typeId[info.currentPolyVersion],
                    info.currentScopeDepth + 1, // +1 because variables exist within stmt->firstBody, not the current scope
                    varnameIt.identifier->scopeId);
                debugFunction->addVar(varnameNr.name,
                    varinfo_index->versions_dataOffset[info.currentPolyVersion],
                    varinfo_index->versions_typeId[info.currentPolyVersion],
                    info.currentScopeDepth + 1,
                    varnameNr.identifier->scopeId);
            }
        } else if(statement->forLoopType == CUSTOM_FOR_LOOP) {
            auto& varnameIt = statement->varnames[0];
            auto& varnameNr = statement->varnames[1];
            if(!varnameIt.identifier || !varnameNr.identifier) {
                Assert(hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            auto varinfo_item  = varnameIt.identifier;
            auto varinfo_index = varnameNr.identifier;

            OverloadGroup::Overload create_overload = statement->versions_create_overload[currentPolyVersion];
            OverloadGroup::Overload iterate_overload = statement->versions_iterate_overload[currentPolyVersion];
            if(!create_overload.funcImpl || !iterate_overload.funcImpl) {
                Assert(hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            
            // TODO: Support other conventions and external functions.
            Assert(create_overload.funcImpl->signature.convention == BETCALL);
            Assert(iterate_overload.funcImpl->signature.convention == BETCALL);

            TypeId iterator_type = create_overload.funcImpl->signature.returnTypes[0].typeId;
            TypeInfo* iterator_typeinfo = ast->getTypeInfo(iterator_type);
                
            auto it_memdata = iterator_typeinfo->getMember(NAME_OF_CUSTOM_IT);
            auto nr_memdata = iterator_typeinfo->getMember(NAME_OF_CUSTOM_NR);
            if(it_memdata.index == -1) {
                Assert(hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            if(nr_memdata.index == -1) {
                Assert(hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            // Assert(it_memdata.index != -1);
            // Assert(nr_memdata.index != -1);
            
            // nocheckin auto dereference iterator
            SignalIO result = SIGNAL_NO_MATCH;
            
            int iterator_offset = 0;
            { // create_iterator
                result = framePush(iterator_type, &iterator_offset, false, false);
                
                auto signature = &create_overload.funcImpl->signature;
                int allocated_stack_space = signature->argSize;
                builder.emit_alloc_args(BC_REG_INVALID, allocated_stack_space);
                currentFuncImpl->update_max_arguments(allocated_stack_space);
                
                TypeId iter_type{};
                result = generateReference(statement->firstExpression, &iter_type, currentScopeId);
                if (result != SIGNAL_SUCCESS)
                    return result;
                iter_type.setPointerLevel(iter_type.getPointerLevel() + 1);
                
                // TODO: verify type is valid
                Assert(signature->argumentTypes.size() == 1);
                
                Assert(signature->argumentTypes[0].typeId == iter_type);
                    
                result = generatePop_set_arg(signature->argumentTypes[0].offset, signature->argumentTypes[0].typeId);
                if (result != SIGNAL_SUCCESS)
                    return result;
                
                int reloc;
                builder.emit_call(create_overload.astFunc->linkConvention, create_overload.astFunc->callConvention, &reloc);
                info.addCallToResolve(reloc, create_overload.funcImpl);
                
                builder.emit_free_args(allocated_stack_space);
                
                Assert(signature->returnTypes.size() == 1);
                auto &ret = signature->returnTypes[0];
                TypeId returned_type = ret.typeId;
                
                result = generatePush_get_val(ret.offset - signature->returnSize, returned_type);
                if (result != SIGNAL_SUCCESS)
                    return result;
                    
                result = generatePop(BC_REG_LOCALS, iterator_offset, returned_type);
                if (result != SIGNAL_SUCCESS)
                    return result;
                
                varinfo_index->versions_dataOffset[info.currentPolyVersion] = iterator_offset + nr_memdata.offset;
                varinfo_item->versions_dataOffset[info.currentPolyVersion] = iterator_offset + it_memdata.offset;
            }

            _GLOG(log::out << "push loop\n");
            loopScope->continueAddress = builder.get_pc();

            { // iterate(&iterator)
                auto signature = &iterate_overload.funcImpl->signature;
                int allocated_stack_space = signature->argSize;
                builder.emit_alloc_args(BC_REG_INVALID, allocated_stack_space);
                currentFuncImpl->update_max_arguments(allocated_stack_space);
                
                // First argument (this)
                TypeId iter_type{};
                result = generateReference(statement->firstExpression, &iter_type, currentScopeId);
                if (result != SIGNAL_SUCCESS)
                    return result;
                iter_type.setPointerLevel(iter_type.getPointerLevel() + 1);
                
                Assert(signature->argumentTypes.size() == 2);
                
                // Second argument (iterator)
                builder.emit_ptr_to_locals(BC_REG_B, iterator_offset);
                builder.emit_push(BC_REG_B);
                
                result = generatePop_set_arg(signature->argumentTypes[1].offset, signature->argumentTypes[1].typeId);
                if (result != SIGNAL_SUCCESS)
                    return result;
                
                result = generatePop_set_arg(signature->argumentTypes[0].offset, signature->argumentTypes[0].typeId);
                if (result != SIGNAL_SUCCESS)
                    return result;
                
                int reloc;
                builder.emit_call(iterate_overload.astFunc->linkConvention, iterate_overload.astFunc->callConvention, &reloc);
                info.addCallToResolve(reloc, iterate_overload.funcImpl);
                
                builder.emit_free_args(allocated_stack_space);
                
                Assert(signature->returnTypes.size() == 1);
                auto &ret = signature->returnTypes[0];
                TypeId returned_type = ret.typeId;
                
                result = generatePush_get_val(ret.offset - signature->returnSize, returned_type);
                if (result != SIGNAL_SUCCESS)
                    return result;
                    
                // TODO: Type check before generator
                Assert(returned_type == TYPE_BOOL);
            }
            
            // pop boolean from iterate() function
            builder.emit_pop(BC_REG_A);
            
            loopScope->resolveBreaks.add(0);
            builder.emit_jz(BC_REG_A, &loopScope->resolveBreaks.last());

            result = generateBody(statement->firstBody);
            if (result != SIGNAL_SUCCESS)
                return result;

            builder.emit_jmp(loopScope->continueAddress);

            for (auto ind : loopScope->resolveBreaks) {
                builder.fix_jump_imm32_here(ind);
            }
            
            // TODO: Call destruct(iterator) on the created iterator
            
            // only add 'it' for Sliced loop (not ranged)
            if(debugFunction) {
                debugFunction->addVar(varnameIt.name,
                    iterator_offset,
                    // varinfo_item->versions_dataOffset[info.currentPolyVersion],
                    varinfo_item->versions_typeId[info.currentPolyVersion],
                    info.currentScopeDepth + 1, // +1 because variables exist within stmt->firstBody, not the current scope
                    varnameIt.identifier->scopeId);
                debugFunction->addVar(varnameNr.name,
                    varinfo_index->versions_dataOffset[info.currentPolyVersion],
                    varinfo_index->versions_typeId[info.currentPolyVersion],
                    info.currentScopeDepth + 1,
                    varnameNr.identifier->scopeId);
                debugFunction->addVar("_for_iter_",
                    iterator_offset,
                    iterator_type,
                    info.currentScopeDepth + 1, // +1 because variables exist within stmt->firstBody, not the current scope
                    statement->firstBody->scopeId);
            }
        } else {
            Assert(("statement->forLoopType is invalid",false));
        }
        
        // builder.emit_free_local(stackBeforeLoop - info.currentFrameOffset);
        currentFuncImpl->free_frame_space(stackBeforeLoop - currentFrameOffset);
        currentFrameOffset = stackBeforeLoop;
        
        // pop loop happens in defer
    } else if(statement->type == ASTStatement::BREAK) {
        GenContext::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
        if(!loop) {
            ERR_SECTION(
                ERR_HEAD2(statement->location)
                ERR_MSG("Break is only allowed in loops.")
                ERR_LINE2(statement->location,"not in a loop")
            )
            return SIGNAL_FAILURE;
        }
        if(loop->stackMoment != currentFrameOffset) {
            // builder.emit_free_local(loop->stackMoment - currentFrameOffset); // freeing local variables without modifying currentFrameOffset
            // currentFuncImpl->free_frame_space(loop->stackMoment - currentFrameOffset);
        }
        
        loop->resolveBreaks.add(0);
        builder.emit_jmp(&loop->resolveBreaks.last());
    
        return SIGNAL_SUCCESS;
        // break; // don't continue, we'll break things
    } else if(statement->type == ASTStatement::CONTINUE) {
        GenContext::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
        if(!loop) {
            ERR_SECTION(
                ERR_HEAD2(statement->location)
                ERR_MSG("Continue is only allowed in loops.")
                ERR_LINE2(statement->location,"not in a loop")
            )
            return SIGNAL_FAILURE;
        }
        if(loop->stackMoment != currentFrameOffset) {
            // builder.emit_free_local(loop->stackMoment - currentFrameOffset);
            // currentFuncImpl->free_frame_space(loop->stackMoment - currentFrameOffset);
        }

        builder.emit_jmp(loop->continueAddress);
        
        return SIGNAL_SUCCESS;
    } else if (statement->type == ASTStatement::RETURN) {
        _GLOG(SCOPE_LOG("RETURN"))

        if (!info.currentFunction) {
            if(info.tinycode->name != compiler->entry_point) {
                // The global entry point can have return statement
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("Return only allowed in function.")
                    ERR_LINE2(statement->location, "bad")
                )
                return SIGNAL_FAILURE;
            }
            if ((int)statement->arrayValues.size() > 1) {
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("Found " << statement->arrayValues.size() << " return value(s) but only one is allowed when using the global scope as the entry point.")
                    ERR_LINE2(statement->location, "Y values")
                )
                return SIGNAL_FAILURE;
            }
        } else {
            if ((int)statement->arrayValues.size() != (int)info.currentFuncImpl->signature.returnTypes.size()) {
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("Found " << statement->arrayValues.size() << " return value(s) but should have " << info.currentFuncImpl->signature.returnTypes.size() << " for '" << info.currentFunction->name << "'.")
                    ERR_LINE2(info.currentFunction->location, "X return values")
                    ERR_LINE2(statement->location, "Y values")
                )
            }
        }

        if(info.currentFunction && (info.currentFunction->callConvention == STDCALL || info.currentFunction->callConvention == UNIXCALL)) {
            Assert(statement->arrayValues.size() <= 1);
            // stdcall and unixcall can only have one return value
            // error message should be handled in type checker
        }

        //-- Evaluate return values
        for (int argi = 0; argi < (int)statement->arrayValues.size(); argi++) {
            ASTExpression *expr = statement->arrayValues.get(argi);
            // nextExpr = nextExpr->next;
            // argi++;

            TypeId dtype = {};
            SignalIO result = generateExpression(expr, &dtype);
            if (result != SIGNAL_SUCCESS) {
                continue;
            }
            if(info.currentFuncImpl) {
                if (argi < (int)info.currentFuncImpl->signature.returnTypes.size()) {
                    // auto a = info.ast->typeToString(dtype);
                    // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
                    auto& retType = info.currentFuncImpl->signature.returnTypes[argi];
                    if (!performSafeCast(dtype, retType.typeId)) {
                        // if(info.currentFunction->returnTypes[argi]!=dtype){
                        ERRTYPE1(expr->location, dtype, info.currentFuncImpl->signature.returnTypes[argi].typeId, "(return values)");
                        
                        generatePop(BC_REG_INVALID, 0, dtype);
                    } else {
                        generatePop_set_ret(retType.offset - info.currentFuncImpl->signature.returnSize, retType.typeId);
                        // generatePop(BC_REG_BP, retType.offset - info.currentFuncImpl->returnSize, retType.typeId);
                    }
                } else {
                    // error here which has been printed somewhere
                    // but we should throw away values on stack so that
                    // we don't become desyncrhonized.
                    generatePop(BC_REG_INVALID, 0, dtype);
                }
            } else {
                TypeId retType = TYPE_VOID;
                if(compiler->options->target == TARGET_LINUX_x64) {
                    retType = TYPE_UINT8;
                } else {
                    retType = TYPE_INT32;
                }
                if (!performSafeCast(dtype, retType)) {
                    ERRTYPE1(expr->location, dtype, retType, ".");
                    generatePop(BC_REG_INVALID, 0, dtype);
                } else {
                    generatePop_set_ret(0-REGISTER_SIZE, retType);
                }
            }
        }
        // if(currentFrameOffset != 0) {
            // builder.emit_free_local(-currentFrameOffset);
            int index;
            builder.emit_free_local(&index);
            add_frame_fix(index);
        // }
        builder.emit_ret();
        // info.currentFrameOffset = lastOffset; // nocheckin TODO: Should we reset frame like this? If so, should we not break this loop and skip the rest of the statements too?
        // break; // Let's try?
    } else if (statement->type == ASTStatement::EXPRESSION) {
        _GLOG(SCOPE_LOG("EXPRESSION"))

        TEMP_ARRAY_N(TypeId, exprTypes, 5);
        // IMPORTANT: There is a bug here if expression returns more than 5 expressions, the scratch allocator would mess things up
        SignalIO result = generateExpression(statement->firstExpression, &exprTypes);
        if (result != SIGNAL_SUCCESS) {
            return result;
        }
        if(exprTypes.size() > 0 && exprTypes[0] != TYPE_VOID){
            for(int i = 0; i < (int) exprTypes.size();i++) {
                TypeId dtype = exprTypes[i];
                generatePop(BC_REG_INVALID, 0, dtype);
            }
        }
    }
    else if (statement->type == ASTStatement::USING) {
        _GLOG(SCOPE_LOG("USING"))
        Assert(false); // TODO: Fix this code and error messages
        Assert(statement->varnames.size()==1);

        // Token* name = &statement->varnames[0].name;

        // auto origin = info.ast->findIdentifier(info.currentScopeId, *name);
        // if(!origin){
        //     ERR_HEAD2(statement->location) << *name << " is not a variable (using)\n";
            
        //     return SIGNAL_FAILURE;
        // }
        // Fix something with content order? Is something fixed in type checker? what?
        // auto aliasId = info.ast->addIdentifier(info.currentScopeId, *statement->alias);
        // if(!aliasId){
        //     ERR_HEAD2(statement->location) << *statement->alias << " is already a variable or alias (using)\n";
            
        //     return SIGNAL_FAILURE;
        // }

        // aliasId->type = origin->type;
        // aliasId->varIndex = origin->varIndex;
    } else if (statement->type == ASTStatement::DEFER) {
        _GLOG(SCOPE_LOG("DEFER"))
        // defers are handled in the parser but we don't paste the body statement of the defers to end of scopes. We paste a new defer statement which
        // content is shared. !statement->sharedContents tells us that this
        // defer is the original from the end of the scope, not from continues/breaks/returns.
        SignalIO result = generateBody(statement->firstBody);
        // Is it okay to do nothing with result?
    } else if (statement->type == ASTStatement::BODY) {
        _GLOG(SCOPE_LOG("BODY (statement)"))

        SignalIO result = generateBody(statement->firstBody);
        // Is it okay to do nothing with result?
    } else if (statement->type == ASTStatement::TEST) {
        int moment = currentFrameOffset;
        // int moment = builder.saveStackMoment();
        TEMP_ARRAY_N(TypeId, tempTypes, 5)
        SignalIO result = generateExpression(statement->testValue, &tempTypes);
        if(tempTypes.size() != 1) {
            ERR_SECTION(
                ERR_HEAD2(statement->testValue->location)
                ERR_MSG("The expression in test statements must consist of 1 type.")
                ERR_LINE2(statement->testValue->location,tempTypes.size()<<" types")
            )
            return SIGNAL_FAILURE;
        }
        int size = info.ast->getTypeSize(tempTypes.last());
        TypeInfo* info_testvalue = nullptr;
        if(tempTypes.last().isNormalType())
            info_testvalue = info.ast->getTypeInfo(tempTypes.last());
        if(info_testvalue && info_testvalue->astStruct) {
            ERR_SECTION(
                ERR_HEAD2(statement->testValue->location)
                ERR_MSG("Types passed to _test cannot be a structs. They must be a primitive value like an integer.")
                ERR_LINE2(statement->testValue->location,"bad")
            )
            return SIGNAL_FAILURE;
        }
        if(size > REGISTER_SIZE) {
            ERR_SECTION(
                ERR_HEAD2(statement->testValue->location)
                ERR_MSG("Type cannot be larger than "<<REGISTER_SIZE<<" bytes. Test statement doesn't handle larger structs.")
                ERR_LINE2(statement->testValue->location,"bad")
            )
            return SIGNAL_FAILURE;
        }

        tempTypes.clear();
        result = generateExpression(statement->firstExpression, &tempTypes);
        if(tempTypes.size() != 1) {
            ERR_SECTION(
                ERR_HEAD2(statement->firstExpression->location)
                ERR_MSG("The expression in test statements must consist of 1 type.")
                ERR_LINE2(statement->firstExpression->location,tempTypes.size()<<" types")
            )
            return SIGNAL_FAILURE;
        }
        size = info.ast->getTypeSize(tempTypes.last());
        TypeInfo* info_expr = nullptr;
        if(tempTypes.last().isNormalType())
            info_expr = info.ast->getTypeInfo(tempTypes.last());
        if(info_expr && info_expr->astStruct) {
            ERR_SECTION(
                ERR_HEAD2(statement->firstExpression->location)
                ERR_MSG("Types passed to _test cannot be a structs. They must be a primitive value like an integer.")
                ERR_LINE2(statement->firstExpression->location,"bad")
            )
            return SIGNAL_FAILURE;
        }
        if(size > REGISTER_SIZE) {
            ERR_SECTION(
                ERR_HEAD2(statement->firstExpression->location)
                ERR_MSG("Type cannot be larger than "<<REGISTER_SIZE<<" bytes. Test statement doesn't handle larger structs.")
                ERR_LINE2(statement->firstExpression->location,"bad")
            )
            return SIGNAL_FAILURE;
        }
        // TODO: Should the types match? u16 - i32 might be fine but f32 - u16 shouldn't be okay, or?
        builder.emit_pop(BC_REG_A);
        builder.emit_pop(BC_REG_D);
        
        // NOTE: Alignment is handled in x64 generator.
        // TEST_VALUE calls stdcall functions which needs 16-byte alignment
        // int alignment = (16 - (currentFrameOffset % 16)) % 16;
        // if(alignment != 0)
        //     builder.emit_alloc_local(BC_REG_INVALID, alignment);
        // Assert(currentFrameOffset % 16 == 0);
        int loc = compiler->addTestLocation(statement->location, &compiler->lexer);
        builder.emit_test(BC_REG_D, BC_REG_A, REGISTER_SIZE, loc);
        // if(alignment != 0)
        //     builder.emit_free_local(alignment);
    } else if (statement->type == ASTStatement::TRY) {
        _GLOG(SCOPE_LOG("TRY-CATCH"))

        int stackBeforeLoop = currentFrameOffset;
        defer {
            currentFrameOffset = stackBeforeLoop;
        };

        compiler->code_has_exceptions = true;
        
        int data_offset = 0;
        TypeId vartype{};
        for(int i=0;i<statement->switchCases.size();i++) {
            if(statement->switchCases[i].variable) {
                vartype = statement->switchCases[i].variable->versions_typeId[currentPolyVersion];
                break;
            }
        }
        if(vartype.isValid()) {
            SignalIO result = framePush(vartype, &data_offset, false, false);
            Assert(result == SIGNAL_SUCCESS || hasAnyErrors());

            for(int i=0;i<statement->switchCases.size();i++) {
                if(statement->switchCases[i].variable) {
                    statement->switchCases[i].variable->versions_dataOffset[currentPolyVersion] = data_offset;
                }
            }
        }

        int bc_start = builder.get_pc();

        // Generate try
        generateBody(statement->firstBody);

        int bc_end = builder.get_pc();

        int offset_jmp;
        builder.emit_jmp(&offset_jmp); // jump over catch

        // Generate catch

        DynamicArray<int> catch_end_jumps{};

        TEMP_ARRAY_N(TypeId, tempTypes, 5);
        for(int i=0;i<statement->switchCases.size();i++) {
            auto& catch_part = statement->switchCases[i];
            auto catch_expr = catch_part.caseExpr;
            auto catch_body = catch_part.caseBody;
            SignalIO result = SIGNAL_NO_MATCH;

            // NOTE: One try-block for each catch. We can minimize size of the object file we we are a little smarter but I don't think we would gain much. We might as well keep it simple instead.
            TryBlock block{}; // we can't add try block to try_blocks yet because the body we generate may modifiy the array.
            block.bc_start = bc_start;
            block.bc_end = bc_end;
            if(catch_part.variable)
                block.offset_to_exception_info = data_offset;
            // block.frame_offset_before_try = ; // fixed later when we know the max size of the stack frame
            // TODO: We don't need to store the frame offset per TryBlock, just per function. It would save memory and I don't think it will change in the near future.

            block.bc_catch_start = builder.get_pc();
            result = generateBody(catch_body);

            auto temp_tinycode = compiler->get_temp_tinycode();
            
            // Calculate expression at compile time
            GenContext context{};
            context.init_context(compiler);
            context.builder.init(bytecode, temp_tinycode, compiler);

            context.currentScopeId = ast->globalScopeId;
            context.currentFrameOffset = 0;
            context.currentScopeDepth = -1;
            context.currentPolyVersion = 0;
            
            result = context.generateExpression(catch_expr, &tempTypes, 0);
            if (result != SIGNAL_SUCCESS) {
                if (!info.hasForeignErrors()) {
                    ERR_SECTION(
                        ERR_HEAD2(catch_expr->location)
                        ERR_MSG("Cannot evaluate expression for catch at compile time. TODO: Provide better error message.")
                        ERR_LINE2(catch_expr->location, "here")
                    )
                }
                continue;
            }
            auto expr_type = statement->versions_expressionTypes[currentPolyVersion][i];
            Assert(expr_type == tempTypes[0]);

            // log::out << log::GOLD <<"catch filter\n";
            // temp_tinycode->print(0,-1,bytecode);
            
            VirtualMachine vm{};
            vm.silent = true;
            vm.init_stack();
            vm.execute(bytecode, temp_tinycode->name, true);

            int* value = (int*)(vm.stack_pointer);
            block.filter_exception_code = *value;
            temp_tinycode->restore_to_empty();

            tinycode->try_blocks.add(block);

            if(i < statement->switchCases.size() - 1) {
                int off = 0;
                builder.emit_jmp(&off);
                catch_end_jumps.add(off);
            }

            // TODO: Add catch variable to debug info
            // debugFunction->addVar(varnameIt.name,
            //     varinfo_item->versions_dataOffset[info.currentPolyVersion],
            //     varinfo_item->versions_typeId[info.currentPolyVersion],
            //     info.currentScopeDepth + 1, // +1 because variables exist within stmt->firstBody, not the current scope
            //     varnameIt.identifier->scopeId);
        }

        for(auto off : catch_end_jumps) {
            builder.fix_jump_imm32_here(off);
        }

        builder.fix_jump_imm32_here(offset_jmp);

        // NOTE: I don't know how to implement 'finally' on Linux so we don't support it at all. Also, the implementation for it is harder than just generateBody. It's a termination handler, we need to add information about it in .pdata, .xdata and such.
        // if(statement->secondBody)
        //     generateBody(statement->secondBody);
        
        currentFuncImpl->free_frame_space(stackBeforeLoop - currentFrameOffset);
        currentFrameOffset = stackBeforeLoop;
    } else {
        Assert(("You forgot to implement statement type!",false));
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateBody(ASTScope *body) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
    PROFILE_SCOPE
    _GLOG(FUNC_ENTER)
    Assert(body||info.hasForeignErrors());
    if(!body) return SIGNAL_FAILURE;

    MAKE_NODE_SCOPE(body); // no need, the scope itself doesn't generate code

    Bytecode::Dump debugDump{};
    debugDump.tinycode_index = tinycode->index;
    if(body->flags & ASTNode::DUMP_ASM) {
        debugDump.dumpAsm = true;
    }
    if(body->flags & ASTNode::DUMP_BC) {
        debugDump.dumpBytecode = true;
    }
    debugDump.bc_startIndex = builder.get_pc();
    
    bool codeWasDisabled = info.disableCodeGeneration;
    bool errorsWasIgnored = info.ignoreErrors;
    ScopeId savedScope = info.currentScopeId;
    ScopeInfo* body_scope = info.ast->getScope(body->scopeId);
    body_scope->bc_start = builder.get_pc();
    info.currentScopeDepth++;

    info.currentScopeId = body->scopeId;

    int lastOffset = info.currentFrameOffset;

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    defer {
        info.disableCodeGeneration = codeWasDisabled;
        builder.disable_builder(info.disableCodeGeneration);

        if (lastOffset != info.currentFrameOffset) {
            _GLOG(log::out << "fix sp when exiting body\n";)
            // builder.emit_free_local(lastOffset - info.currentFrameOffset);
            currentFuncImpl->free_frame_space(lastOffset - info.currentFrameOffset);
            
            info.currentFrameOffset = lastOffset;
        } else {
            
        }

        info.currentScopeDepth--;
        body_scope->bc_end = builder.get_pc();
        info.currentScopeId = savedScope; 
        info.ignoreErrors = errorsWasIgnored;
        if(debugDump.dumpAsm || debugDump.dumpBytecode) {
            // debugDump.description = body->tokenRange.tokenStream()->streamName + ":"+std::to_string(body->tokenRange.firstToken.line);
            // debugDump.description = TrimDir(body->tokenRange.tokenStream()->streamName) + ":"+std::to_string(body->tokenRange.firstToken.line);
            debugDump.bc_endIndex = builder.get_pc();
            Assert(debugDump.bc_startIndex <= debugDump.bc_endIndex);
            bytecode->debugDumps.add(debugDump);
        }
    };

    const bool disabled_debug = false;

    for (auto statement : body->statements) {
        MAKE_NODE_SCOPE(statement);

        SOURCE_TRACE(statement->location)

        // TODO: Debug information is very slow.
        lexer::TokenSource* srcinfo = nullptr;
        if(compiler->options->useDebugInformation && !disabled_debug) {
            srcinfo = compiler->lexer.getTokenSource_unsafe(statement->location);
            if(debugFunction)
                debugFunction->addLine(srcinfo->line, builder.get_pc(), statement->location.tok.origin);
        }

        info.disableCodeGeneration = codeWasDisabled;
        builder.disable_builder(info.disableCodeGeneration);
        info.ignoreErrors = errorsWasIgnored;
        
        int prev_stackAlignment_size = 0;
        int prev_virtualStackPointer = 0;
        int prev_currentFrameOffset = 0;
        if(statement->isNoCode()) {
            info.disableCodeGeneration = true;
            builder.disable_builder(info.disableCodeGeneration);
            info.ignoreErrors = true;
        } else {
            if(compiler->options->useDebugInformation && !disabled_debug) {
                std::string line = compiler->lexer.getline(statement->location);
                builder.push_line(srcinfo->line, line);
            }
        }

        defer {
            if(statement->isNoCode()) {
                // Assert(false); // TODO, broken
                // Assert(prev_stackAlignment_size <= info.stackAlignment.size()); // We lost information, the no code remove stack elements which we can't get back. We would need to save the elements not just the size of stack alignment
                // info.stackAlignment.resize(prev_stackAlignment_size);
                // info.virtualStackPointer = prev_virtualStackPointer;
                // info.currentFrameOffset = prev_currentFrameOffset;
            }
        };
        
        // if(statement->computeWhenPossible && !inside_compile_time_execution) {
        if(statement->computeWhenPossible) {
            if(!at_top_level) { // if top level then it was already added in type checker
                GlobalRunDirective rundir{};
                rundir.statement = statement;
                rundir.scope = currentScopeId;
                compiler->global_run_directives.add(rundir);
            }
            continue;
        }

        SignalIO result = generateStatement(statement);
        if(statement->type == ASTStatement::RETURN || statement->type == ASTStatement::CONTINUE || statement->type == ASTStatement::BREAK) {
            break;   
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePreload() {
    BCRegister src_reg = BC_REG_F;
    BCRegister dst_reg = BC_REG_E;
    int polyVersion = 0;

    if(!compiler->varInfos[VAR_INFOS])
        return SIGNAL_SUCCESS;

    // Take pointer of type information arrays
    // and move into the global slices.
    builder.emit_dataptr(src_reg, compiler->dataOffset_types);
    builder.emit_dataptr(dst_reg, compiler->varInfos[VAR_INFOS]->versions_dataOffset[polyVersion]);
    builder.emit_mov_mr(dst_reg, src_reg, REGISTER_SIZE);
    
    builder.emit_dataptr(src_reg, compiler->dataOffset_members);
    builder.emit_dataptr(dst_reg, compiler->varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion]);
    builder.emit_mov_mr(dst_reg, src_reg, REGISTER_SIZE);

    builder.emit_dataptr(src_reg, compiler->dataOffset_strings);
    builder.emit_dataptr(dst_reg, compiler->varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion]);
    builder.emit_mov_mr(dst_reg, src_reg, REGISTER_SIZE);
    return SIGNAL_SUCCESS;
}
// Generate data from the type checker
SignalIO GenContext::generateData() {
    using namespace engone;

    TRACE_FUNC()

    // type checker requested some space for global variables
    if(info.ast->preallocatedGlobalSpace())
        bytecode->appendData(nullptr, info.ast->preallocatedGlobalSpace());

    // IMPORTANT: TODO: Some data like 64-bit integers needs alignment.
    //   Strings don's so it's fine for now but don't forget about fixing this.
    for(auto& pair : info.ast->_constStringMap) {
        // Assert(pair.first.size()!=0);
        // Assert(pair.first.size()!=0);
        int offset = 0;
        if(pair.first.back()=='\0')
            offset = bytecode->appendData(pair.first.data(), pair.first.length() + 1);
        else
            offset = bytecode->appendData(pair.first.data(), pair.first.length() + 1); // +1 to include null termination, this is to prevent mistakes when using C++ functions which expect it.
        if(offset == -1){
            continue;
        }
        auto& constString = info.ast->getConstString(pair.second);
        constString.address = offset;
    }


    if(compiler->typeinfo_import_id == 0) {
        // TODO: We don't have import to type information. Either that's a bug or we didn't import Lang.btb.
        //   If it's a bug we want to message the user. How do we know that though?
        //   We shouldn't spam the user about type information not being imported unless they wanted it.
    } else {
        IdentifierVariable* identifiers[VAR_COUNT]{nullptr,nullptr,nullptr};
        // compiler->lock_imports.lock();
        auto imp = compiler->imports.get(compiler->typeinfo_import_id-1);
        ScopeId typeinfo_scope = imp->scopeId;
        // compiler->lock_imports.unlock();

        identifiers[VAR_INFOS]   = (IdentifierVariable*)info.ast->findIdentifier(typeinfo_scope, CONTENT_ORDER_MAX, "lang_typeInfos", nullptr);
        identifiers[VAR_MEMBERS] = (IdentifierVariable*)info.ast->findIdentifier(typeinfo_scope, CONTENT_ORDER_MAX, "lang_members", nullptr);
        identifiers[VAR_STRINGS] = (IdentifierVariable*)info.ast->findIdentifier(typeinfo_scope, CONTENT_ORDER_MAX, "lang_strings", nullptr);

        if(!(identifiers[0] && identifiers[1] && identifiers[2])) {
            // This only happens if there is a bug in the compiler or if Lang.bt
            // was modified to not contain the identifiers we are looking for.
            // TODO: Improve error message
            log::out << log::RED << "One source file imported type information but one of these identifiers could not be resolved:\n";
            log::out << log::WHITE << " lang_typeInfos, lang_members, lang_strings\n\n";
            info.errors++;
        } else {
            FORAN(identifiers) {
                Assert(identifiers[nr]->is_var());
                compiler->varInfos[nr] = identifiers[nr];
                Assert(compiler->varInfos[nr] && compiler->varInfos[nr]->isGlobal());
                Assert(compiler->varInfos[nr]->versions_dataOffset.size() == 1);
            }

            // TODO: Optimize, don't append the types that aren't used in the code.
            //  Implement a feature like FuncImpl.usages
            // TODO: Optimize in general? Many cache misses. Probably need to change the 
            //  Type and AST structure though.

            int count_types = info.ast->_typeInfos.size();
            int count_members = 0;
            int count_stringdata = 0;
            for(int i=0;i<info.ast->_typeInfos.size();i++){
                auto ti = info.ast->_typeInfos[i];
                if(!ti)  continue;
                count_stringdata += ti->name.length() + 1; // +1 for null termination
                if(ti->structImpl) {
                    count_members += ti->astStruct->members.size();
                    for(int mi=0;mi<ti->astStruct->members.size();mi++){
                        auto& mem = ti->astStruct->members[mi];
                        count_stringdata += mem.name.length() + 1; // +1 for null termination
                    }
                }
                if(ti->astEnum) {
                    count_members += ti->astEnum->members.size();
                    for(int mi=0;mi<ti->astEnum->members.size();mi++){
                        auto& mem = ti->astEnum->members[mi];
                        count_stringdata += mem.name.length() + 1; // +1 for null termination
                    }
                }
                if(ti->funcType) {
                    // count_members += ti->astEnum->members.size();
                    // for(int mi=0;mi<ti->astEnum->members.size();mi++){
                    //     auto& mem = ti->astEnum->members[mi];
                    //     count_stringdata += mem.name.length() + 1; // +1 for null termination
                    // }
                }
            }
            bytecode->ensureAlignmentInData(REGISTER_SIZE); // just to be safe
            int off_typedata   = bytecode->appendData(nullptr, count_types * sizeof(lang::TypeInfo));
            bytecode->ensureAlignmentInData(REGISTER_SIZE); // just to be safe
            int off_memberdata = bytecode->appendData(nullptr, count_members * sizeof(lang::TypeMember));
            int off_stringdata = bytecode->appendData(nullptr, count_stringdata);
            
            lang::TypeInfo* typedata = (lang::TypeInfo*)(bytecode->dataSegment.data() + off_typedata);
            lang::TypeMember* memberdata = (lang::TypeMember*)(bytecode->dataSegment.data() + off_memberdata);
            char* stringdata = (char*)(bytecode->dataSegment.data() + off_stringdata);

            // TODO: Zero initialize memory? Or use '_'?

            // log::out << "TypeInfo size " <<sizeof(lang::TypeInfo);

            u32 string_offset = 0;
            int member_count = 0;
            for(int i=0;i<info.ast->_typeInfos.size();i++){
                auto ti = info.ast->_typeInfos[i];
                if(!ti) { 
                    typedata[i] = {}; // zero initialize
                    // typedata[i].type = lang::Primitive::NONE;
                    continue;
                }

                typedata[i].size = ti->getSize();
                if(ti->astEnum)                      typedata[i].type = lang::Primitive::ENUM;
                else if(ti->astStruct)               typedata[i].type = lang::Primitive::STRUCT;
                else if(AST::IsDecimal(ti->id))      typedata[i].type = lang::Primitive::DECIMAL;
                else if(AST::IsInteger(ti->id)) {    
                    if(AST::IsSigned(ti->id))        typedata[i].type = lang::Primitive::SIGNED_INT;
                    else                             typedata[i].type = lang::Primitive::UNSIGNED_INT;
                } else if(ti->id == TYPE_CHAR)        typedata[i].type = lang::Primitive::CHAR;
                else if(ti->id == TYPE_BOOL)          typedata[i].type = lang::Primitive::BOOL;
                else if(ti->funcType)                typedata[i].type = lang::Primitive::FUNCTION;
                else typedata[i].type = lang::Primitive::NONE; // compiler specific type like ast_typeid or ast_string, ast_fncall
                
                typedata[i].name.beg = string_offset;
                // The order of beg, end matters. string_offset is at beginning now
                strncpy(stringdata + string_offset, ti->name.c_str(), ti->name.length() + 1);
                string_offset += ti->name.length() + 1;
                // Now string_offset is at end. DON'T MOVE AROUND THE LINES!
                typedata[i].name.end = string_offset - 1; // -1 won't include null termination
                typedata[i].members.beg = 0; // set later
                typedata[i].members.end = 0;

                if(ti->structImpl) {
                    typedata[i].members.beg = member_count;

                    for(int mi=0;mi<ti->structImpl->members.size();mi++){
                        auto& mem = ti->structImpl->members[mi];
                        auto& ast_mem = ti->astStruct->members[mi];

                        memberdata[member_count].name.beg = string_offset;

                        memcpy(stringdata + string_offset, ast_mem.name.c_str(), ast_mem.name.length());
                        stringdata[string_offset + ast_mem.name.length()] = '\0';
                        string_offset += ast_mem.name.length() + 1;

                        memberdata[member_count].name.end = string_offset - 1; // -1 won't include null termination

                        // TODO: Enum member

                        memberdata[member_count].type.index0 = mem.typeId._infoIndex0;
                        memberdata[member_count].type.index1 = mem.typeId._infoIndex1;
                        memberdata[member_count].type.ptr_level = mem.typeId.getPointerLevel();
                        memberdata[member_count].offset = mem.offset;

                        member_count++;
                    }
                    typedata[i].members.end = member_count;
                } else if(ti->astEnum) {
                    typedata[i].members.beg = member_count;

                    for(int mi=0;mi<ti->astEnum->members.size();mi++){
                        auto& ast_mem = ti->astEnum->members[mi];

                        memberdata[member_count].name.beg = string_offset;

                        memcpy(stringdata + string_offset, ast_mem.name.c_str(), ast_mem.name.length());
                        stringdata[string_offset + ast_mem.name.length()] = '\0';
                        string_offset += ast_mem.name.length() + 1;

                        memberdata[member_count].name.end = string_offset - 1; // -1 won't include null termination

                        memberdata[member_count].value = ast_mem.enumValue;

                        member_count++;
                    }
                    typedata[i].members.end = member_count;
                } else if(ti->funcType) {
                    // typedata[i].members.beg = member_count;

                    // for(int mi=0;mi<ti->astEnum->members.size();mi++){
                    //     auto& ast_mem = ti->astEnum->members[mi];

                    //     memberdata[member_count].name.beg = string_offset;

                    //     memcpy(stringdata + string_offset, ast_mem.name.c_str(), ast_mem.name.length());
                    //     stringdata[string_offset + ast_mem.name.length()] = '\0';
                    //     string_offset += ast_mem.name.length() + 1;

                    //     memberdata[member_count].name.end = string_offset - 1; // -1 won't include null termination

                    //     memberdata[member_count].value = ast_mem.enumValue;

                    //     member_count++;
                    // }
                    // typedata[i].members.end = member_count;
                }
            }
            Assert(string_offset == count_stringdata && count_members == member_count);

            // TODO: Assert that the variables are of the right type

            int polyVersion = 0;

            lang::Slice* slice_types   = (lang::Slice*)(bytecode->dataSegment.data() + compiler->varInfos[VAR_INFOS]->versions_dataOffset[polyVersion]);
            lang::Slice* slice_members = (lang::Slice*)(bytecode->dataSegment.data() + compiler->varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion]);
            lang::Slice* slice_strings = (lang::Slice*)(bytecode->dataSegment.data() + compiler->varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion]);
            slice_types->len = count_types;
            slice_members->len = count_members;
            slice_strings->len = count_stringdata;
            compiler->dataOffset_types = off_typedata;
            compiler->dataOffset_members = off_memberdata;
            compiler->dataOffset_strings = off_stringdata;
            // log::out << "types "<<count_types<<", members "<<count_members << ", strings "<<count_stringdata <<"\n";

            // This is scrap
            // bytecode->ptrDataRelocations.add({
            //     (u32)varInfos[VAR_INFOS]->versions_dataOffset[polyVersion],
            //     (u32)off_typedata});
            // bytecode->ptrDataRelocations.add({
            //     (u32)varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion],
            //     (u32)off_memberdata});
            // bytecode->ptrDataRelocations.add({
            //     (u32)varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion],
            //     (u32)off_stringdata});
        }
    }

    return SIGNAL_SUCCESS;
}

// Generate data from the type checker
SignalIO GenContext::generateGlobalData() {
    using namespace engone;

    TRACE_FUNC()

    // TODO: Polymorphism is not considered for globals inside functions, we need to set poly version for that

    ASTStatement* last_stmt = nullptr;
    CALLBACK_ON_ASSERT(
        ERR_SECTION(
            ERR_HEAD2(last_stmt->location)
            ERR_MSG("Cannot evaluate expression for global variable at compile time. Compile time evaluation is experimental and a lot of things does not work. Perhaps you called a function in the expression? Or took a reference? Or function pointer?")
            ERR_LINE2(last_stmt->location, "here")
        )
    )

    for (int i=0;i<ast->globals_to_evaluate.size();i++) {
        ASTStatement* stmt = ast->globals_to_evaluate[i].stmt;
        last_stmt = stmt;
        ScopeId scopeId = ast->globals_to_evaluate[i].scope;
        // Assert(stmt->firstExpression); // statement should not have been added if there was no expression
        Assert(stmt->arrayValues.size() == 0); // we don't handle initializer lists
        Assert(stmt->varnames.size() == 1); // multiple varnames means that the expression produces multiple values which is annoying to handle so skip it for now

        // TODO: Code below should be the same as the one in generateFunction.
        //   If we change the code in generateFunction but forget to here then
        //   we will be in trouble. So, how do we combine the code?

        auto temp_tinycode = compiler->get_temp_tinycode();

        tinycode = temp_tinycode;
        builder.init(bytecode, tinycode, compiler);
        currentScopeId = ast->globalScopeId;
        currentFrameOffset = 0;
        currentScopeDepth = -1;
        currentPolyVersion = 0;

        BCRegister data_ptr = BC_REG_B;

        // we allocate space for pointer to global data here
        // VM will manually put the pointer at this memory location
        // we do 16 because of 16-byte alignment rule in calling conventions
        builder.emit_alloc_local(BC_REG_INVALID, 16);

        // log::out << "glob " << stmt->varnames[0].name << " " << stmt->varnames[0].identifier->versions_dataOffset[currentPolyVersion]<<"\n";

        TypeId type{};
        if(!stmt->firstExpression) {
            type = stmt->varnames[0].identifier->versions_typeId[currentPolyVersion];

            if(!type.isValid()) {
                continue;
            }
            
            auto info = ast->getTypeInfo(type);
            if(!info || !info->astStruct) {
                continue;
            }

            generateDefaultValue(BC_REG_INVALID, 0, type, &stmt->location);
        } else {
            TEMP_ARRAY_N(TypeId, tempTypes, 5)
            inside_compile_time_execution = true;
            inside_global = true;
            auto result = generateExpression(stmt->firstExpression, &tempTypes, 0);
            inside_global = false;
            inside_compile_time_execution = false;
            // TODO: We generate expression with from global scope so that we can't access local variables but what about constant functions? There may be more issues?
            if (result != SIGNAL_SUCCESS) {
                if (!info.hasForeignErrors()) {
                    ERR_SECTION(
                        ERR_HEAD2(stmt->location)
                        ERR_MSG("Cannot evaluate expression for global variable at compile time. TODO: Provide better error message.")
                        ERR_LINE2(stmt->location, "here")
                    )
                }
                continue;
            }
            if (tempTypes.size() == 0 || !tempTypes[0].isValid()) {
                if (!info.hasForeignErrors()) {
                    ERR_SECTION(
                        ERR_HEAD2(stmt->location)
                        ERR_MSG("Bad type.")
                        ERR_LINE2(stmt->location, "here")
                    )
                }
                continue;
            }
            type = tempTypes[0];
        }
        
        compiler->compile_stats.errors += errors;

        // TODO: Check that the generated type fits in the allocate global data. Does type match the one in the statement?

        // get pointer to global data from stack
        builder.emit_mov_rm_disp(data_ptr, BC_REG_LOCALS, REGISTER_SIZE, -REGISTER_SIZE);

        auto result = generatePop(data_ptr, 0, type);
        Assert(result == SIGNAL_SUCCESS);

        // log::out << log::GOLD <<"global: " <<stmt->varnames[0].name << "\n";
        // tinycode->print(0,-1,bytecode);

        // setup VM with stack and global pointer
        VirtualMachine vm{};
        vm.silent = true;
        vm.init_stack();
        u8* ptr_to_global_data = (u8*)bytecode->dataSegment.data();
        int data_offset = stmt->varnames[0].identifier->versions_dataOffset[currentPolyVersion];
        u8* ptr_to_value = ptr_to_global_data + data_offset;
        if(REGISTER_SIZE == 4) {
            u32 mem = 0x1000'0000;
            u32 mem_size = bytecode->dataSegment.size();
            vm.add_memory_mapping(mem, (u64)ptr_to_global_data, mem_size);
            *((u32*)(vm.stack.data() + vm.stack.max - REGISTER_SIZE)) = mem + data_offset;
        } else {
            *((void**)(vm.stack.data() + vm.stack.max - REGISTER_SIZE)) = ptr_to_value;
        }
        // let VM evaluate expression and put into global data
        vm.execute(bytecode, temp_tinycode->name, true);
        
        struct Env {
            TypeId type;
            u8* ptr = 0;
            int memberIndex;
        };
        DynamicArray<Env> stack{};
        stack.add({type, ptr_to_value, 0});
        while(stack.size()) {
            auto env = stack.last();
            stack.pop();
            
            if(env.type.getPointerLevel()) {
                i64 value = 0;
                memcpy(&value, env.ptr, REGISTER_SIZE);
                if(value != 0) {
                    ERR_SECTION(
                        ERR_HEAD2(stmt->location)
                        ERR_MSG("The type of the global variable contains a pointer which isn't allowed. Pointers created at compile time cannot exist at runtime. It may be implemented with relocations to the data section in the future.")
                        ERR_LINE2(stmt->location, "here")
                    )
                    break;
                }
                continue;
            }
            // TODO: Better error message
            TypeInfo* typeinfo = ast->getTypeInfo(env.type.baseType());
            if(typeinfo->funcType) {
                i64 value = 0;
                memcpy(&value, env.ptr, REGISTER_SIZE);
                if(value != 0) {
                    ERR_SECTION(
                        ERR_HEAD2(stmt->location)
                        ERR_MSG("The type of the global variable contains a function pointer which isn't allowed. Function pointers at compile time cannot exist at runtime. It may be implemented with relocations in the future.")
                        ERR_LINE2(stmt->location, "here")
                    )
                    break;
                }
            } else if(typeinfo->structImpl) {
                for(int i=typeinfo->structImpl->members.size()-1;i>=0;i--) {
                    auto& mem = typeinfo->structImpl->members[i];
                    stack.add({mem.typeId, env.ptr + mem.offset, i});
                }
            }
        }
    }

    POP_LAST_CALLBACK()

    return SIGNAL_SUCCESS;
}


// Generate data from the type checker
SignalIO GenContext::executeGlobalRunDirective(GlobalRunDirective* run_directive) {
    using namespace engone;

    TRACE_FUNC()

    // TODO: Polymorphism is not considered for globals inside functions, we need to set poly version for that

    VirtualMachine vm{};
    
    ScopeId scopeId = run_directive->scope;
    ASTStatement* statement = run_directive->statement;
    lexer::SourceLocation location = statement->location;

    CALLBACK_ON_ASSERT(
        ERR_SECTION(
            ERR_HEAD2(location)
            ERR_MSG_LOG("Virtual machine failed when executing run directive. Call stack:\n")
            for(int i=0;i<vm.call_stack.size();i++) {
                log::out << " " << vm.call_stack[i].func->name << "\n";
            }
            // TODO: Call stack
            ERR_LINE2(location, "here")
        )
    )

    // TODO: Code below should be the same as the one in generateFunction.
    //   If we change the code in generateFunction but forget to here then
    //   we will be in trouble. So, how do we combine the code?
    auto temp_tinycode = compiler->get_temp_tinycode();
    {
        tinycode = temp_tinycode;
        builder.init(bytecode, temp_tinycode, compiler);
        currentScopeId = ast->globalScopeId;
        currentFrameOffset = 0;
        currentScopeDepth = -1;
        currentPolyVersion = 0;

        // auto di = bytecode->debugInformation;
        // auto dfun = di->addFunction(nullptr, tinycode, "<comp-time-path>", 1);
        // dfun->import_id = imp->import_id;
        // dfun->import_id = 0;
        // debugFunction = dfun;

        // Assert(!currentFuncImpl);
        FuncImpl temp{};
        currentFuncImpl = &temp;
        temp.polyVersion = 0;

        int index_of_frame_size = 0;
        builder.emit_alloc_local(BC_REG_INVALID, &index_of_frame_size);
        add_frame_fix(index_of_frame_size);

        inside_compile_time_execution = true;
        auto result = generateStatement(statement);
        inside_compile_time_execution = false;
        
        int index;
        builder.emit_free_local(&index);
        add_frame_fix(index);
        builder.emit_ret();

        fix_frame_values(currentFuncImpl, tinycode);

        compiler->compile_stats.errors += errors;

        // TODO: We generate expression with from global scope so that we can't access local variables but what about constant functions? There may be more issues?
        if (result != SIGNAL_SUCCESS) {
            if (!info.hasForeignErrors()) {
                ERR_SECTION(
                    ERR_HEAD2(location)
                    ERR_MSG("Cannot evaluate expression for global variable at compile time.")
                    // TODO: Provide better error message
                    ERR_LINE2(location, "here")
                )
            }
            return SIGNAL_FAILURE;
        }
    }

    // log::out << log::GOLD <<"global: " <<stmt->varnames[0].name << "\n";
    // tinycode->print(0,-1,bytecode);

    vm.silent = true;
    vm.init_stack();
    if(REGISTER_SIZE == 4) {
        void* ptr_to_global_data = bytecode->dataSegment.data();
        u32 mem = 0x1000'0000;
        u32 mem_size = bytecode->dataSegment.size();
        vm.add_memory_mapping(mem, (u64)ptr_to_global_data, mem_size);
    }
    // let VM evaluate expression and put into global data
    vm.execute(bytecode, temp_tinycode->name, true);

    POP_LAST_CALLBACK()
    
    if(vm.error.type != VM_ERROR_NONE) {
        printVMFailedMessage(vm, location);
        return SIGNAL_FAILURE;
    }

    return SIGNAL_SUCCESS;
}

void GenContext::printVMFailedMessage(VirtualMachine& vm, lexer::SourceLocation location) {
    using namespace engone;
    if(vm.error.type == VM_UNRESOLVED_CALL) {
        ERR_SECTION(
            ERR_HEAD2(location)
            ERR_MSG_LOG("One of the functions the virtual machine tried to call was incomplete. A run directive cannot call a function that contains another run directive, remove it to fix this error. This function call '"<<vm.error.message<<"'.\n")
            // for(int i=0;i<vm.call_stack.size();i++) {
            //     log::out << " " << vm.call_stack[i].func->name << "\n";
            // }
            ERR_LINE2(location, "here")
        )
    } else {
        ERR_SECTION(
            ERR_HEAD2(location)
            ERR_MSG_LOG("Virtual machine failed for an unspecified reason. Call stack:\n")
            for(int i=0;i<vm.call_stack.size();i++) {
                log::out << " " << vm.call_stack[i].func->name << "\n";
            }
            ERR_LINE2(location, "here")
        )
    }
}

void TestGenerate(BytecodeBuilder& b);

bool GenerateScope(ASTScope* scope, Compiler* compiler, CompilerImport* imp, DynamicArray<TinyBytecode*>* out_codes, bool is_initial_import, bool gen_func_with_run_directives) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue);

    // _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)

    GenContext context{};
    context.init_context(compiler);
    context.imp = imp;
    context.out_codes = out_codes;
    context.gen_func_with_run_directives = gen_func_with_run_directives;

    context.generateFunctions(scope);
    
    Identifier* iden = nullptr;
    // we assume main function has run directives
    // if(is_initial_import && !gen_func_with_run_directives) {
    if(is_initial_import && gen_func_with_run_directives) {
        if(compiler->entry_point.size() != 0) {
            iden = compiler->ast->findIdentifier(imp->scopeId,0,compiler->entry_point, nullptr, true, true);
            if(!iden) {
                // If no main function exists then the code in global scope of
                // initial import will be the main function.

                // TODO: Code here
                CallConvention main_conv = BETCALL;
                switch(compiler->options->target) {
                    case TARGET_BYTECODE: main_conv = BETCALL; break;
                    case TARGET_WINDOWS_x64: main_conv = STDCALL; break;
                    case TARGET_LINUX_x64: main_conv = UNIXCALL; break;
                    case TARGET_ARM: main_conv = UNIXCALL; break;
                    // case TARGET_ARM: main_conv = ARMCALL; break;
                    default: Assert(false);
                }
                TinyBytecode* tb_main = context.bytecode->createTiny(compiler->entry_point,main_conv);
                context.bytecode->index_of_main = tb_main->index;

                // TODO: Code below should be the same as the one in generateFunction.
                //   If we change the code in generateFunction but forget to here then
                //   we will be in trouble. So, how do we combine the code?

                context.tinycode = tb_main;
                context.builder.init(context.bytecode, context.tinycode, compiler);
                context.currentScopeId = context.ast->globalScopeId;
                context.currentFrameOffset = 0;
                context.currentScopeDepth = -1;
                context.currentPolyVersion = 0;

                out_codes->add(tb_main);

                auto di = context.bytecode->debugInformation;
                auto dfun = di->addFunction(nullptr, context.tinycode, imp->path, 1);
                dfun->import_id = imp->import_id;
                context.debugFunction = dfun;
                // Don't add yet?
                context.bytecode->addExportedFunction(tb_main->name, context.tinycode->index);

                Assert(!context.currentFuncImpl);
                FuncImpl temp{};
                context.currentFuncImpl = &temp;
                temp.polyVersion = 0;
                temp.signature.returnTypes.add({});
                temp.signature.returnTypes.last().typeId = TYPE_INT32;
                temp.signature.returnTypes.last().offset = -compiler->arch.REGISTER_SIZE;

                int index_of_frame_size = 0;
                context.builder.emit_alloc_local(BC_REG_INVALID, &index_of_frame_size);
                context.add_frame_fix(index_of_frame_size);

                context.generatePreload();

                context.at_top_level = true;
                context.generateBody(scope);
                context.at_top_level = false;
                // TestGenerate(context.builder);
                
                // We can't skip ret like this the last ret may exist in a
                // if-block. That if block will jump to the instruction after the ret 
                // so WE MUST emit an extra ret.
                // TODO: We could optimize a bit if the last instruction doesn't come from
                //   an if, while, for, switch or whatever else we might have. This makes it hard.
                // if(context.builder.get_last_opcode() != BC_RET) { }
                int index;
                context.builder.emit_free_local(&index);
                context.add_frame_fix(index);
                context.builder.emit_ret();

                context.fix_frame_values(context.currentFuncImpl, tb_main);
            }
        }
    }
    
    context.compiler->compile_stats.errors += context.errors;

    return true;
}
void GenContext::init_context(Compiler* compiler) {
    this->compiler = compiler;
    ast = compiler->ast;
    bytecode = compiler->bytecode;
    reporter = &compiler->reporter;
    scratch_allocator.init(0x10000);
    FRAME_SIZE = compiler->arch.FRAME_SIZE;
    REGISTER_SIZE = compiler->arch.REGISTER_SIZE;
}

void TestGenerate(BytecodeBuilder& b) {
    // Tests for x64 generator. How well does it convert the code?
    
    // #############3
    // Complex code, good test for x64 generator
    // #############

    // b.emit_alloc_local(BC_REG_INVALID, 8);

    // b.emit_li32(BC_REG_B,9);
    // b.emit_mov_rm_disp(BC_REG_A,BC_REG_LOCALS,4,-8);

    // // b.emit_add(BC_REG_A,BC_REG_A,false,4);
    // b.emit_add(BC_REG_A,BC_REG_B,false,4);
    // b.emit_push(BC_REG_B);
    // b.emit_add(BC_REG_B,BC_REG_B,false,4);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_A,4,-8);
    // // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_B,4,-8);

    // b.emit_pop(BC_REG_D);
    // // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_D,4,-8);
    
    // b.emit_jz(BC_REG_A, 0);

    // b.emit_free_local(8);

    // #############
    // Efficient register usage
    // #################
    // b.emit_alloc_local(BC_REG_INVALID, 8);

    // b.emit_li32(BC_REG_A,9);
    // b.emit_li32(BC_REG_B,9);
    // b.emit_li32(BC_REG_C,9);

    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_A,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_B,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_C,4,-8);

    // // will x64 reuse registers since we overwrite the values?
    // b.emit_li32(BC_REG_E,9);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_E,4,-8);

    // b.emit_li32(BC_REG_C,9);
    // b.emit_li32(BC_REG_B,9);

    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_C,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_B,4,-8);
    
    // b.emit_free_local(8);

    // #############
    // Calls and register allocation
    // #################
    
    // b.emit_alloc_local(BC_REG_INVALID, 8);
    
    // b.emit_li32(BC_REG_A,9);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_A,4,-8);

    // b.emit_alloc_local(BC_REG_INVALID, 8);
    // b.emit_mov_rm_disp(BC_REG_A,BC_REG_LOCALS,4,-8);
    // b.emit_set_arg(BC_REG_A, 0, 4, false);
    // int n;
    // b.emit_call(LinkConvention::NONE, CallConvention::BETCALL, &n);
    // b.emit_free_local(8);
    // b.emit_get_val(BC_REG_B, -8, 4, false);
    
    // b.emit_push(BC_REG_B);
    
    // b.emit_alloc_local(BC_REG_INVALID, 8);
    // b.emit_mov_rm_disp(BC_REG_A,BC_REG_LOCALS,4,-8);
    // b.emit_set_arg(BC_REG_A, 0, 4, false);
    // b.emit_call(LinkConvention::NONE, CallConvention::BETCALL, &n);
    // b.emit_free_local(8);
    // b.emit_get_val(BC_REG_C, -8, 4, false);
    
    // b.emit_pop(BC_REG_B);
    
    // b.emit_alloc_local(BC_REG_INVALID, 8);
    // b.emit_set_arg(BC_REG_B, 0, 4, false);
    // b.emit_set_arg(BC_REG_C, 4, 4, false);
    // b.emit_call(LinkConvention::NONE, CallConvention::BETCALL, &n);
    // b.emit_free_local(8);
    
    // b.emit_free_local(8);

    // #############
    // Push and pop management
    // (bytecode push/pop and x64 automatic push/pop or other way of dealing wi)
    // #################
    
    // b.emit_alloc_local(BC_REG_INVALID, 8);

    // b.emit_li32(BC_REG_A,9);
    // b.emit_li32(BC_REG_B,9);
    // b.emit_li32(BC_REG_C,9);
    // b.emit_li32(BC_REG_D,9);
    // b.emit_li32(BC_REG_E,9);
    // b.emit_li32(BC_REG_F,9);
    // b.emit_li32(BC_REG_G,9);
    // b.emit_li32(BC_REG_H,9);
    // b.emit_li32(BC_REG_I,9);

    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_A,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_B,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_C,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_D,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_E,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_F,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_G,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_H,4,-8);
    // b.emit_mov_mr_disp(BC_REG_LOCALS,BC_REG_I,4,-8);
    
    // b.emit_free_local(8);
    
    // #############
    // Read / write ordering
    // #################

    // b.emit_li32(BC_REG_A, 2);
    // b.emit_li32(BC_REG_D, 5);
    // b.emit_push(BC_REG_D);
    // b.emit_push(BC_REG_A);
    
    // b.emit_pop(BC_REG_A); // throw range.now
    // b.emit_pop(BC_REG_D); // range.end we care about

    // b.emit_mov_rm_disp(BC_REG_C, BC_REG_LOCALS, 4, -8);
    // // builder.emit_mov_rm_disp(index_reg, BC_REG_BP, 4, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

    // b.emit_incr(BC_REG_C, 1);
    
    // b.emit_mov_mr_disp(BC_REG_LOCALS, BC_REG_C, 4, -8);
    // // builder.emit_mov_mr_disp(BC_REG_BP, index_reg, 4, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

    // // bytecode->addDebugText("For condition\n");
    // b.emit_add(BC_REG_C, BC_REG_D, false, 4);

    // b.emit_jz(BC_REG_C, 0);

    // b.emit_jmp(0);


    // ########################3
    //  EDGE CASE
    // ########################3

    // b.emit_

    // b.emit_li32(BC_REG_A, 9);
    // b.emit_add(BC_REG_F, BC_REG_A, false, 4);

    // b.emit_mov_mr(BC_REG_LOCALS, BC_REG_F, 4);
}
