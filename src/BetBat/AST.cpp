#include "BetBat/AST.h"
#include "Engone/PlatformLayer.h"

const char* OpToStr(int optype){
    #define CASE(A,B) case AST_##A: return #B;
    switch(optype){
        CASE(ADD,+)
        CASE(SUB,-)
        CASE(MUL,*)
        CASE(DIV,/)

        CASE(EQUAL,==)
        CASE(NOT_EQUAL,!=)
        CASE(LESS,<)
        CASE(LESS_EQUAL,<=)
        CASE(GREATER,>)
        CASE(GREATER_EQUAL,>=)
        CASE(AND,&&)
        CASE(OR,||)
        CASE(NOT,!)
        
        CASE(CAST,cast)
        CASE(PROP,prop)
        CASE(INITIALIZER,initializer)
        CASE(FROM_NAMESPACE,namespaced)

        CASE(REFER,&)
        CASE(DEREF,* (deref))
    }
    #undef CASE
    return "?";
}
const char* StateToStr(int type){
    #define CASE(A,B) case ASTStatement::A: return #B;
    switch(type){
        CASE(ASSIGN,assign)
        CASE(PROP_ASSIGN,prop_assign)
        CASE(IF,if)
        CASE(WHILE,while)
        CASE(RETURN,return)
        CASE(CALL,call)
    }
    #undef CASE
    return "?";
}
const char* TypeIdToStr(int type){
    #define CASE(A,B) case AST_##A: return #B;
    switch(type){
        CASE(FLOAT32,f32)
        CASE(INT32,i32)
        CASE(BOOL8,bool)
        
        CASE(VAR,var)
    }
    #undef CASE
    return "?";
}
AST* AST::Create(){
    AST* ast = (AST*)engone::Allocate(sizeof(AST));
    new(ast)AST();
    // initialize default data types
    ast->typeInfos["void"]   = new TypeInfo{AST_VOID};
    ast->typeInfos["u8"]     = new TypeInfo{AST_UINT8, 1};
    ast->typeInfos["u16"]    = new TypeInfo{AST_UINT16, 2};
    ast->typeInfos["u32"]    = new TypeInfo{AST_UINT32, 4};
    ast->typeInfos["u64"]    = new TypeInfo{AST_UINT64, 8};
    ast->typeInfos["i8"]     = new TypeInfo{AST_INT8, 1};
    ast->typeInfos["i16"]    = new TypeInfo{AST_INT16,2};
    ast->typeInfos["i32"]    = new TypeInfo{AST_INT32,4};
    ast->typeInfos["i64"]    = new TypeInfo{AST_INT64,8};
    ast->typeInfos["f32"]    = new TypeInfo{AST_FLOAT32,4};
    ast->typeInfos["bool"]   = new TypeInfo{AST_BOOL8,1};
    ast->typeInfos["null"]   = new TypeInfo{AST_NULL,8};
    ast->typeInfos["var"]    = new TypeInfo{AST_VAR};
    ast->typeInfos["call"]   = new TypeInfo{AST_FNCALL};
    return ast;
}

void AST::Destroy(AST* ast){
    if(!ast) return;
    ast->~AST();
    engone::Free(ast,sizeof(AST));
}
void AST::cleanup(){
    using namespace engone;
    log::out << log::RED << "AST::cleanup not implemented\n";
}
TypeInfo* AST::getTypeInfo(const std::string& name){
    auto pair = typeInfos.find(name);
    if (pair==typeInfos.end()){
        if(name.back()=='*')
            return (typeInfos[name] = new TypeInfo{nextPointerTypeId++, 8}); // NOTE: fixed pointer size of 8
        else
            return (typeInfos[name] = new TypeInfo{nextTypeIdId++});
    }
    return pair->second;
}
TypeInfo* AST::getTypeInfo(TypeId id){
    for (auto& pair : typeInfos){
        if(pair.second->id == id){
            return pair.second;
        }
    }
    return 0;
}
TypeId AST::getTypeId(const std::string& name){
    return getTypeInfo(name)->id;
}
const std::string* AST::getTypeId(TypeId id){
    for (auto& pair : typeInfos){
        if(pair.second->id == id){
            return &pair.first;
        }
    }
    return 0;
}
bool AST::IsPointer(Token& token){
    if(!token.str || token.length==0) return false;
    return token.str[token.length-1] == '*';
}
bool AST::IsPointer(TypeId id){
    return id & POINTER_BIT;
}
bool AST::IsInteger(TypeId id){
    return AST_UINT8<=id && id <= AST_INT64;
}
bool AST::IsSigned(TypeId id){
    return AST_INT8<=id && id <= AST_INT64;
}
ASTBody* AST::createBody(){
    return new ASTBody();
}
ASTStatement* AST::createStatement(int type){
    
    ASTStatement* ptr = new ASTStatement();
    ptr->type = type;
    return ptr;
}
ASTStruct* AST::createStruct(const std::string& name){
    ASTStruct* ptr = new ASTStruct();
    ptr->name = new std::string(name);
    return ptr;
}
ASTEnum* AST::createEnum(const std::string& name){
    ASTEnum* ptr = new ASTEnum();
    ptr->name = new std::string(name);
    return ptr;
}
ASTFunction* AST::createFunction(const std::string& name){
    ASTFunction* ptr = new ASTFunction();
    ptr->name = new std::string(name);
    return ptr;
}
ASTExpression* AST::createExpression(TypeId type){
    ASTExpression* ptr = new ASTExpression();
    ptr->isValue = (u32)type<AST_PRIMITIVE_COUNT;
    ptr->typeId = type;
    return ptr;
}
void PrintSpace(int count){
    using namespace engone;
    for (int i=0;i<count;i++) log::out << " ";
}
void AST::print(int depth){
    using namespace engone;
    if(body){
        PrintSpace(depth);
        log::out << "AST\n";
        body->print(this, depth+1);
    }
}
void ASTBody::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Body\n";
    if(structs)
        structs->print(ast, depth+1);
    if(functions)
        functions->print(ast,depth+1);
    if(statements)
        statements->print(ast, depth+1);
}
void ASTFunction::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Func "<<*name<<" (";
    for(int i=0;i<(int)arguments.size();i++){
        auto& arg = arguments[i];
        auto dtname = ast->getTypeId(arg.typeId);
        log::out << arg.name << ": "<<*dtname;
        if(i+1!=(int)arguments.size()){
            log::out << ", ";
        }
    }
    log::out << ")";
    if(!returnTypes.empty())
        log::out << "->";
    for(auto& ret : returnTypes){
        auto dtname = ast->getTypeId(ret);
        log::out <<*dtname<<", ";
    }
    log::out << "\n";
    if(body){
        body->print(ast,depth+1);
    }
    if(next)
        next->print(ast,depth);
}
void ASTStruct::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Struct "<<*name<<" {";
    for(int i=0;i<(int)members.size();i++){
        auto& member = members[i];
        auto dtname = ast->getTypeId(member.typeId);
        log::out << member.name << ": "<<*dtname;
        if(i+1!=(int)members.size()){
            log::out << ", ";
        }
    }
    log::out << "}\n";
    if(next)
        next->print(ast,depth);
}
void ASTEnum::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Enum "<<*name<<" {";
    for(int i=0;i<(int)members.size();i++){
        auto& member = members[i];
        log::out << member.name << "="<<member.id;
        if(i+1!=(int)members.size()){
            log::out << ", ";
        }
    }
    log::out << "}\n";
    if(next)
        next->print(ast,depth);
}
void ASTStatement::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Statement "<<StateToStr(type);

    if(type==ASSIGN){
        auto dtname = ast->getTypeId(typeId);
        if(typeId!=0)
            // log::out << " "<<TypeIdToStr(dataType)<<"";
            log::out << " "<<*dtname<<"";
        log::out << " "<<*name<<"\n";
        if(rvalue){
            rvalue->print(ast, depth+1);
        }
    }else if(type==PROP_ASSIGN){
        log::out << "\n";
        if(lvalue){
            lvalue->print(ast, depth+1);
        }
        if(rvalue)
            rvalue->print(ast, depth+1);
    }else if(type==IF){
        log::out << "\n";
        if(rvalue){
            rvalue->print(ast,depth+1);
        }
        if(body){
            body->print(ast,depth+1);
        }
        if(elseBody){
            elseBody->print(ast,depth+1);
        }
    }else if(type==WHILE){
        log::out << "\n";
        if(rvalue){
            rvalue->print(ast,depth+1);
        }
        if(body){
            body->print(ast,depth+1);
        }
    }else if(type==RETURN){
        log::out << "\n";
        if(rvalue){
            rvalue->print(ast,depth+1);
        }
    }else if(type==CALL){
        log::out << "\n";
        if(rvalue){
            rvalue->print(ast,depth+1);
        }
    }
    if(next){
        next->print(ast, depth);
    }
}
void ASTExpression::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    
    if(isValue){
        auto dtname = ast->getTypeId(typeId);
        // log::out << "Expr "<<TypeIdToStr(dataType)<<" ";
        log::out << "Expr ";
        if(dtname) log::out <<*dtname;
        log::out<<" ";
        if(typeId==AST_FLOAT32) log::out << f32Value;
        else if(typeId>=AST_UINT8&&typeId<=AST_INT64) log::out << i64Value;
        else if(typeId==AST_BOOL8) log::out << b8Value;
        else if(typeId==AST_VAR) log::out << *name;
        else if(typeId==AST_FNCALL) log::out << *name;
        else if(typeId==AST_NULL) log::out << "null";
        else
            log::out << "missing print impl.";
        if(typeId==AST_FNCALL){
            if(left){
                log::out << " args:\n";
                left->print(ast,depth+1);
            }else{
                log::out << " no args\n";
            }
        }else
            log::out << "\n";
    } else {
        log::out << "Expr "<<OpToStr(typeId)<<" ";
        if(typeId==AST_CAST){
            log::out << *ast->getTypeId(castType);
            log::out << "\n";
            left->print(ast,depth+1);
        }else if(typeId==AST_PROP){
            log::out << *name;
            log::out << "\n";
            left->print(ast,depth+1);
        }else if(typeId==AST_INITIALIZER){
            log::out << *name;
            log::out << "\n";
            left->print(ast,depth+1);
        }else if(typeId==AST_FROM_NAMESPACE){
            log::out << *name<<" "<<*member;
            log::out << "\n";
        }else{
            log::out << "\n";
            if(left){
                left->print(ast, depth+1);
            }
            if(right){
                right->print(ast, depth+1);
            }
        }
    }
    if(next){
        next->print(ast, depth);
    }
}