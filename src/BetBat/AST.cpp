#include "BetBat/AST.h"
#include "Engone/PlatformLayer.h"

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
ASTExpression* AST::createExpression(int type){
    ASTExpression* ptr = new ASTExpression();

    // u64 offset = CreateGeneral(this,sizeof(ASTExpression));
    // ASTExpression* ptr = (ASTExpression*)memory.data + offset;
    // new(ptr)ASTExpression();

    ptr->innerType = type;
    if(type>ASTExpression::OPERATION) {
        ptr->type = ASTExpression::OPERATION;
    } else if(type>ASTExpression::OPERATION) {
        ptr->type = ASTExpression::VALUE;
    }
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
    log::out << "Statement "<<type;

    if(type==ASSIGN){
        log::out << ", Assign "<<*name<<"\n";
        if(expression){
            ast->relocate(expression)->print(ast, depth+1);
        }
    }
    log::out << "\n";
    if(next){
        ast->relocate(next)->print(ast, depth);
    }
}
void ASTExpression::print(AST* ast, int depth){
    using namespace engone;
    PrintSpace(depth);
    if(innerType==F32){
        log::out << "Expr "<<f32Value<<"\n";
    }
    if(type==OPERATION){
        log::out << "ExprOP: "<<innerType<<"\n";
        if(left){
            ast->relocate(left)->print(ast, depth+1);
        }
        if(right){
            ast->relocate(right)->print(ast, depth+1);
        }
    }
}