#include "BetBat/AST.h"
#include "Engone/PlatformLayer.h"

const char* OpToStr(int optype){
    #define CASE(A,B) case AST_##A: return #B;
    switch(optype){
        CASE(ADD,+)
        CASE(SUB,-)
        CASE(MUL,*)
        CASE(DIV,/)
    }
    #undef CASE
    return "?";
}
const char* StateToStr(int type){
    #define CASE(A,B) case ASTStatement::A: return #B;
    switch(type){
        CASE(ASSIGN,assign)
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
        
        // CASE(ADD,+)
        // CASE(SUB,-)
        // CASE(MUL,*)
        // CASE(DIV,/)
    }
    #undef CASE
    return "?";
}
AST* AST::Create(){
    AST* ast = (AST*)engone::Allocate(sizeof(AST));
    new(ast)AST();
    return ast;
}
void AST::Destroy(AST* ast){
    ast->~AST();
    engone::Free(ast,sizeof(AST));
}
void AST::init(){

}
void AST::cleanup(){
    memory.resize(0);
}
u64 CreateGeneral(AST* ast, u64 size){
    using namespace engone;

    if(ast->memory.max < ast->memory.used + size*2)
        if(!ast->memory.resize(ast->memory.max*2 + size*2+10))
            return 0;
    u64 offset = ast->memory.used;
    ast->memory.used += size;

    u64 mods = 8-((u64)ast->memory.data + offset)%8;
    if(mods==8) mods = 0;
    offset+=mods;
    ast->memory.used+=mods;

    // log::out << "Create "<<offset<<" "<<(u64)ast->memory.data<<"\n";
    // TODO: alignment
    return offset;
}
ASTBody* AST::createBody(){
    return new ASTBody();

    u64 offset = CreateGeneral(this,sizeof(ASTBody));
    ASTBody* ptr = (ASTBody*)memory.data + offset;
    new(ptr)ASTBody();
    offset+=AST_MEM_OFFSET;
    return (ASTBody*)offset;
}
ASTStatement* AST::createStatement(int type){
    ASTStatement* ptr = new ASTStatement();
    ptr->type = type;
    return ptr;

    // u64 offset = CreateGeneral(this,sizeof(ASTStatement));
    // ASTStatement* ptr = (ASTStatement*)memory.data + offset;
    // new(ptr)ASTStatement();
    // ptr->type=type;
    // offset+=AST_MEM_OFFSET;
    // return (ASTStatement*)offset;
}
ASTExpression* AST::createExpression(DataType type){
    ASTExpression* ptr = new ASTExpression();

    // u64 offset = CreateGeneral(this,sizeof(ASTExpression));
    // ASTExpression* ptr = (ASTExpression*)memory.data + offset;
    // new(ptr)ASTExpression();
    ptr->isValue = (u32)type<AST_PRIMITIVE_COUNT;
    ptr->dataType = type;
    return ptr;

    // offset+=AST_MEM_OFFSET;
    // return (ASTExpression*)offset;
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
        relocate(body)->print(this, depth+1);
    }
}
void ASTBody::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Body\n";
    if(statement){
        ast->relocate(statement)->print(ast, depth+1);
    }
}
void ASTStatement::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    log::out << "Statement "<<StateToStr(type);

    if(type==ASSIGN){
        if(dataType!=0)
            log::out << " "<<DataTypeToStr(dataType)<<"";
        log::out << " "<<*name<<"\n";
        if(expression){
            ast->relocate(expression)->print(ast, depth+1);
        }
    }
    // log::out << "\n";
    if(next){
        ast->relocate(next)->print(ast, depth);
    }
}
void ASTExpression::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    
    if(isValue){
        log::out << "Expr "<<DataTypeToStr(dataType)<<" ";
        if(dataType==AST_FLOAT32) log::out << f32Value;
        else if(dataType==AST_INT32) log::out << i32Value;
        else if(dataType==AST_BOOL8) log::out << b8Value;
        else if(dataType==AST_VAR) log::out << *varName;
        else
            log::out << "missing print conversion";
        log::out << "\n";
    } else {
        log::out << "Expr "<<OpToStr(dataType)<<" ";
        log::out << "\n";
        if(left){
            ast->relocate(left)->print(ast, depth+1);
        }
        if(right){
            ast->relocate(right)->print(ast, depth+1);
        }
    }
}