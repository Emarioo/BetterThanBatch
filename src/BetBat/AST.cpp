#include "BetBat/AST.h"
#include "Engone/PlatformLayer.h"

AST* AST::Create(){
    AST* ast = (AST*)engone::Allocate(sizeof(AST));
    new(ast)AST();
    return ast;
}
void AST::init(){

}
void AST::cleanup(){
    memory.resize(0);
}
u64 CreateGeneral(AST* ast, u64 size){
    if(ast->memory.max < ast->memory.used + size)
        if(ast->memory.resize(ast->memory.max*2 + size*2))
            return 0;
    u64 offset = ast->memory.used;
    ast->memory.used += size;
    // TODO: alignment
    return offset;
}
ASTBody* AST::createBody(){
    u64 offset = CreateGeneral(this,sizeof(ASTBody));
    ASTBody* ptr = (ASTBody*)memory.data + offset;
    new(ptr)ASTBody();
    return (ASTBody*)offset;
}
ASTStatement* AST::createStatement(int type){
    u64 offset = CreateGeneral(this,sizeof(ASTStatement));
    ASTStatement* ptr = (ASTStatement*)memory.data + offset;
    new(ptr)ASTStatement();
    ptr->type=type;
    return (ASTStatement*)offset;
}
ASTExpression* AST::createExpression(int type){
    u64 offset = CreateGeneral(this,sizeof(ASTExpression));
    ASTExpression* ptr = (ASTExpression*)memory.data + offset;
    new(ptr)ASTExpression();
    ptr->innerType = type;
    if(type>ASTExpression::OPERATION) {
        ptr->type = ASTExpression::VALUE;
    } else if(type>ASTExpression::OPERATION) {
        ptr->type = ASTExpression::OPERATION;
    }
    return (ASTExpression*)offset;
}