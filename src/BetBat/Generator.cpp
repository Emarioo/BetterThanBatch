#include "BetBat/Generator.h"

#undef ERRT
#undef ERR

#define ERRT(T) engone::log::out << engone::log::RED <<"GenError "<<(T.line)<<":"<<(T.column)<<", "
#define ERR() engone::log::out << engone::log::RED <<"GenError, "

GenInfo::Variable* GenInfo::getVariable(const std::string& name){
    auto pair = variables.find(name);
    if(pair!=variables.end())
        return pair->second;
    
    // if(currentFunction.empty())
    //     return 0;
        
    // auto func = getFunction(currentFunction);
    // auto pair2 = func->variables.find(name);
    // if(pair2!=func->variables.end()) 
    //     return &pair2->second;
    return 0;
}
void GenInfo::removeVariable(const std::string& name){
    auto pair = variables.find(name);
    if(pair!=variables.end()){
        delete pair->second;
        variables.erase(pair);
    }
    // if(currentFunction.empty())
    //     return;
        
    // auto func = getFunction(currentFunction);
    // auto pair2 = func->variables.find(name);
    // if(pair2!=func->variables.end()) 
    //     func->variables.erase(pair2);
}
GenInfo::Variable* GenInfo::addVariable(const std::string& name){
    // if(currentFunction.empty()){
    return (variables[name] = new Variable());
    // }
    // auto func = getFunction(currentFunction);
    // return &(func->variables[name] = {});
}

int GenerateExpression(GenInfo& info, ASTExpression* expression, DataType* outDataType){
    using namespace engone;
    if(expression->isValue){
        // data type
        if(expression->dataType==AST_INT32){
            i32 val = expression->i32Value;
            
            info.code.add({BC_LI,BC_REG_RAX});
            info.code.add(val);
            info.code.add({BC_PUSH,BC_REG_RAX});
            // 1 + 3 + 8
            // + (+ 1 2) (+ 3 4)
            
        } else if(expression->dataType==AST_BOOL8){
            bool val = expression->b8Value;
            
            info.code.add({BC_LI,BC_REG_RAX});
            info.code.add(val);
            info.code.add({BC_PUSH,BC_REG_RAX});
        } else if(expression->dataType==AST_FLOAT32){
            float val = expression->f32Value;
            
            info.code.add({BC_LI,BC_REG_RAX});
            info.code.add(*(u32*)&val);
            info.code.add({BC_PUSH,BC_REG_RAX});
        }  else if(expression->dataType==AST_VAR){
            // check data type and get itttt
            auto var = info.getVariable(*expression->varName);
            if(var){
                // TODO: check data type?
                info.code.add({BC_LI,BC_REG_RAX});
                info.code.add(var->frameOffset);
                info.code.add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX});
                // fp + offset
                info.code.add({BC_MOV_MR,BC_REG_RAX,BC_REG_RAX});
                info.code.add({BC_PUSH,BC_REG_RAX});
                
                *outDataType = var->dataType;
                return 1;
            }else{
                ERRT(expression->token) << expression->token<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outDataType = AST_NONETYPE;
                return 0;            
            }
        } else {
            info.code.add({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized
            ERRT(expression->token) << expression->token<<" is an unknown data type\n";
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            *outDataType = AST_NONETYPE;
            return 0;
        }
        *outDataType = expression->dataType;
    }else{
        DataType ltype,rtype;
        GenerateExpression(info,expression->left,&ltype);
        GenerateExpression(info,expression->right,&rtype);
        
        if(ltype!=rtype){
            ERRT(expression->token) << expression->token<<" mismatch of data types ("<<DataTypeToStr(ltype)<<" - "<<DataTypeToStr(rtype)<<")\n";
            // log::out << log::RED<<"DataType mismatch\n";
            *outDataType = AST_NONETYPE;
            return 0;
        }
        // pop $rdx
        // pop $rax
        // addi $rax $rdx $rax
        // push $rax
        
        info.code.add({BC_POP,BC_REG_RDX});
        info.code.add({BC_POP,BC_REG_RAX});
        
        #define GEN_OP(X,Y) if(expression->dataType==AST_##X) info.code.add({Y,BC_REG_RAX,BC_REG_RDX,BC_REG_RAX});
        if(ltype==AST_FLOAT32){
            GEN_OP(ADD,BC_ADDI)
            else GEN_OP(SUB,BC_SUBI)
            else GEN_OP(MUL,BC_MULI)
            else GEN_OP(DIV,BC_DIVI)
            else
                log::out << log::RED<<"GenExpr: operation not implemented\n";    
        }
        GEN_OP(ADD,BC_ADDI)
        else GEN_OP(SUB,BC_SUBI)
        else GEN_OP(MUL,BC_MULI)
        else GEN_OP(DIV,BC_DIVI)
        else
            log::out << log::RED<<"GenExpr: operation not implemented\n";
        #undef GEN_OP
        
        info.code.add({BC_PUSH,BC_REG_RAX});
        
        *outDataType = ltype;
    }
    return 1;
}

BytecodeX Generate(AST* ast, int* err){
    using namespace engone;
    _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)
    
    
    GenInfo info{};
    info.ast = ast;
    
    auto statement = ast->body->statement;
    while(statement){
        
        if(statement->type==ASTStatement::ASSIGN){
            auto var = info.getVariable(*statement->name);
            if(!var){
                var = info.addVariable(*statement->name);
                // info.code.add
                var->frameOffset = info.nextFrameOffset;
                info.nextFrameOffset+=8;
            }
            DataType dtype;
            GenerateExpression(info, statement->expression,&dtype);
            
            if(dtype==statement->dataType || statement->dataType==AST_NONETYPE){
                var->dataType = dtype;
                info.code.add({BC_LI,BC_REG_RBX});
                info.code.add(var->frameOffset);
                info.code.add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX}); // rbx = fp + offset
                info.code.add({BC_POP,BC_REG_RDX});
                info.code.add({BC_MOV_RM,BC_REG_RDX,BC_REG_RBX});
            }else{
                ERR() << "type mismatch at assignment '"<<*statement->name<<"'\n";
            }
            
            info.code.add({BC_LI,BC_REG_RCX});
            info.code.add(8);
            info.code.add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
            // what happens with stack?            
        }
        
        statement = statement->next;   
    }
    if(err) *err = 0;
    return info.code;
}