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

        CASE(REFER,&)
        CASE(DEREF,*)
    }
    #undef CASE
    return "?";
}
const char* StateToStr(int type){
    #define CASE(A,B) case ASTStatement::A: return #B;
    switch(type){
        CASE(ASSIGN,assign)
        CASE(IF,if)
        CASE(WHILE,while)
    }
    #undef CASE
    return "?";
}
const char* DataTypeToStr(int type){
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
    ast->dataTypes["none"] = AST_NONETYPE;
    ast->dataTypes["i32"] = AST_INT32;
    ast->dataTypes["f32"] = AST_FLOAT32;
    ast->dataTypes["bool"] = AST_BOOL8;
    ast->dataTypes["var"] = AST_VAR;
    return ast;
}

void AST::Destroy(AST* ast){
    ast->~AST();
    engone::Free(ast,sizeof(AST));
}
void AST::cleanup(){
    using namespace engone;
    log::out << log::RED << "AST::cleanup not implemented\n";
}
DataType AST::getDataType(const std::string& name){
    auto pair = dataTypes.find(name);
    if (pair==dataTypes.end())
        return AST_NONETYPE;
    return pair->second;
}
const std::string* AST::getDataType(DataType id){
    for (auto& pair : dataTypes){
        if(pair.second == id){
            return &pair.first;
        }
    }
    return 0;
}
DataType AST::getOrAddDataType(const std::string& name){
    auto id = getDataType(name);
    if(!id)
        id = addDataType(name);
    return id;
}
DataType AST::addDataType(const std::string& name){
    Assert(!name.empty())
    if(name.back()=='*')
        return dataTypes[name] = nextPointerTypeId++;
    else
        return dataTypes[name] = nextDataTypeId++;
}
bool AST::IsPointer(DataType id){
    return id & POINTER_BIT;
}
ASTBody* AST::createBody(){
    return new ASTBody();
}
ASTStatement* AST::createStatement(int type){
    ASTStatement* ptr = new ASTStatement();
    ptr->type = type;
    return ptr;
}
ASTFunction* AST::createFunction(const std::string& name){
    ASTFunction* ptr = new ASTFunction();
    ptr->name = new std::string(name);
    return ptr;
}
ASTExpression* AST::createExpression(DataType type){
    ASTExpression* ptr = new ASTExpression();
    ptr->isValue = (u32)type<AST_PRIMITIVE_COUNT;
    ptr->dataType = type;
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
    if(function)
        function->print(ast,depth+1);
    if(statement){
        statement->print(ast, depth+1);
    }
}
void ASTFunction::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Func "<<*name<<" (";
    for(int i=0;i<(int)arguments.size();i++){
        auto& arg = arguments[i];
        auto dtname = ast->getDataType(arg.dataType);
        log::out << arg.name << ": "<<*dtname;
        if(i+1!=(int)arguments.size()){
            log::out << ", ";
        }
    }
    log::out << ")";
    if(!returnTypes.empty())
        log::out << "->";
    for(auto& ret : returnTypes){
        auto dtname = ast->getDataType(ret);
        log::out <<*dtname<<", ";
    }
    log::out << "\n";
    if(body){
        body->print(ast,depth+1);
    }
    if(next)
        next->print(ast,depth);
}
void ASTStatement::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Statement "<<StateToStr(type);

    if(type==ASSIGN){
        auto dtname = ast->getDataType(dataType);
        if(dataType!=0)
            // log::out << " "<<DataTypeToStr(dataType)<<"";
            log::out << " "<<*dtname<<"";
        log::out << " "<<*name<<"\n";
        if(expression){
            expression->print(ast, depth+1);
        }
    }else if(type==IF){
        log::out << "\n";
        if(expression){
            expression->print(ast,depth+1);
        }
        if(body){
            body->print(ast,depth+1);
        }
        if(elseBody){
            elseBody->print(ast,depth+1);
        }
    }else if(type==WHILE){
        log::out << "\n";
        if(expression){
            expression->print(ast,depth+1);
        }
        if(body){
            body->print(ast,depth+1);
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
        auto dtname = ast->getDataType(dataType);
        // log::out << "Expr "<<DataTypeToStr(dataType)<<" ";
        log::out << "Expr "<<*dtname<<" ";
        if(dataType==AST_FLOAT32) log::out << f32Value;
        else if(dataType==AST_INT32) log::out << i32Value;
        else if(dataType==AST_BOOL8) log::out << b8Value;
        else if(dataType==AST_VAR) log::out << *varName;
        else
            log::out << "missing print impl.";
        log::out << "\n";
    } else {
        log::out << "Expr "<<OpToStr(dataType)<<" ";
        log::out << "\n";
        if(left){
            left->print(ast, depth+1);
        }
        if(right){
            right->print(ast, depth+1);
        }
    }
}