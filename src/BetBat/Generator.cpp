#include "BetBat/Generator.h"

#undef ERRT
#undef ERR

#define ERRT(T) info.errors++;engone::log::out << engone::log::RED <<"GenError "<<(T.line)<<":"<<(T.column)<<", "
#define ERR() info.errors++;engone::log::out << engone::log::RED <<"GenError, "

#define GEN_SUCCESS 0
#define GEN_ERROR 1

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
/*
##################################
    Generation functions below
##################################
*/
int GenerateExpression(GenInfo& info, ASTExpression* expression, DataType* outDataType){
    using namespace engone;
    FUNC_ENTER
    Assert(expression)
    if(expression->isValue){
        // data type
        if(expression->dataType==AST_INT32){
            i32 val = expression->i32Value;
            
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->add(val);
            info.code->add({BC_PUSH,BC_REG_RAX});
        } else if(expression->dataType==AST_BOOL8){
            bool val = expression->b8Value;
            
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->add(val);
            info.code->add({BC_PUSH,BC_REG_RAX});
        } else if(expression->dataType==AST_FLOAT32){
            float val = expression->f32Value;
            
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->add(*(u32*)&val);
            info.code->add({BC_PUSH,BC_REG_RAX});
        }  else if(expression->dataType==AST_VAR){
            // check data type and get it
            auto var = info.getVariable(*expression->varName);
            if(var){
                // TODO: check data type?
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->add(var->frameOffset);
                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX});
                // fp + offset
                info.code->add({BC_MOV_MR,BC_REG_RAX,BC_REG_RAX});
                info.code->add({BC_PUSH,BC_REG_RAX});
                
                *outDataType = var->dataType;
                return GEN_SUCCESS;
            }else{
                ERRT(expression->token) << expression->token<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outDataType = AST_NONETYPE;
                return GEN_ERROR;
            }
        }  else if(expression->dataType==AST_FNCALL){
            // check data type and get it
            auto var = info.getVariable(*expression->varName);
            if(var){
                // TODO: check data type?
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->add(var->frameOffset);
                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX});
                // fp + offset
                info.code->add({BC_MOV_MR,BC_REG_RAX,BC_REG_RAX});
                info.code->add({BC_PUSH,BC_REG_RAX});
                
                *outDataType = var->dataType;
                return GEN_SUCCESS;
            }else{
                ERRT(expression->token) << expression->token<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outDataType = AST_NONETYPE;
                return GEN_ERROR;
            }
        }  else {
            info.code->add({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized
            ERRT(expression->token) << expression->token<<" is an unknown data type\n";
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            *outDataType = AST_NONETYPE;
            return GEN_ERROR;
        }
        *outDataType = expression->dataType;
    }else{
        // pop $rdx
        // pop $rax
        // addi $rax $rdx $rax
        // push $rax
        
        DataType ltype;
        if(expression->dataType==AST_REFER){
            ASTExpression* expr = expression->left;
            if(expr->dataType!=AST_VAR){
                ERRT(expr->token) << expr->token<<", can only reference a variable\n";
                *outDataType = AST_NONETYPE;
                return GEN_ERROR;
            }
            auto var = info.getVariable(*expr->varName);
            if(var){
                // TODO: check data type?
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->add(var->frameOffset);
                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX}); // fp + offset

                // new pointer data type
                auto dtname = info.ast->getDataType(var->dataType);
                std::string temp = *dtname;
                temp += "*";
                auto id = info.ast->getDataType(temp);
                if(!id)
                    id = info.ast->addDataType(temp);
                
                *outDataType = id;
            } else {
                ERRT(expr->token) << expr->token<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outDataType = AST_NONETYPE;
                return GEN_ERROR;
            }
        }else if(expression->dataType==AST_DEREF){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;

            if(!AST::IsPointer(ltype)){
                ERRT(expression->left->token) << expression->left->token<<", can only derefence a pointer variable\n";
                *outDataType = AST_NONETYPE;
                return GEN_ERROR;
            }
            info.code->add({BC_POP,BC_REG_RBX});
            info.code->add({BC_MOV_MR,BC_REG_RBX,BC_REG_RAX});
            // info.code->add({BC_PUSH,BC_REG_RAX});

            auto dtname = info.ast->getDataType(ltype);
            std::string temp = *dtname;
            temp.pop_back(); // pop *
            auto id = info.ast->getDataType(temp);
            if(!id)
                id = info.ast->addDataType(temp);
            
            *outDataType = id;
        }else if(expression->dataType==AST_NOT){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            
            info.code->add({BC_POP,BC_REG_RAX});
            info.code->add({BC_NOTB,BC_REG_RAX,BC_REG_RAX});
            
            *outDataType = ltype;
        }else{
            // basic operations
            DataType rtype;
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            err = GenerateExpression(info,expression->right,&rtype);
            if(err==GEN_ERROR) return err;
            
            info.code->add({BC_POP,BC_REG_RDX});
            info.code->add({BC_POP,BC_REG_RAX});

            if(AST::IsPointer(ltype)&&rtype==AST_INT32){
                if(expression->dataType==AST_ADD){
                    info.code->add({BC_ADDI,BC_REG_RAX,BC_REG_RDX,BC_REG_RAX});
                    *outDataType = ltype;
                    info.code->add({BC_PUSH,BC_REG_RAX});
                    return GEN_SUCCESS;
                }else{
                    log::out << log::RED<<"GenExpr: operation not implemented\n";
                    *outDataType = AST_NONETYPE;
                    return GEN_ERROR;
                }
            }

            if(ltype!=rtype){
                ERRT(expression->token) << expression->token<<" mismatch of data types ("<<*info.ast->getDataType(ltype)<<" - "<<*info.ast->getDataType(rtype)<<")\n";
                // log::out << log::RED<<"DataType mismatch\n";
                *outDataType = AST_NONETYPE;
                return GEN_ERROR;
            }

            #define GEN_OP(X,Y) if(expression->dataType==AST_##X) info.code->add({Y,BC_REG_RAX,BC_REG_RDX,BC_REG_RAX});
            if(ltype==AST_FLOAT32){
                GEN_OP(ADD,BC_ADDF)
                else GEN_OP(SUB,BC_SUBF)
                else GEN_OP(MUL,BC_MULF)
                else GEN_OP(DIV,BC_DIVF)
                else
                    log::out << log::RED<<"GenExpr: operation not implemented\n";    
            }else{
                GEN_OP(ADD,BC_ADDI)
                else GEN_OP(SUB,BC_SUBI)
                else GEN_OP(MUL,BC_MULI)
                else GEN_OP(DIV,BC_DIVI)

                else GEN_OP(EQUAL,BC_EQ)
                else GEN_OP(NOT_EQUAL,BC_NEQ)
                else GEN_OP(LESS,BC_LT)
                else GEN_OP(LESS_EQUAL,BC_LTE)
                else GEN_OP(GREATER,BC_GT)
                else GEN_OP(GREATER_EQUAL,BC_GTE)
                else GEN_OP(AND,BC_ANDI)
                else GEN_OP(OR,BC_ORI)
                else
                    log::out << log::RED<<"GenExpr: operation not implemented\n";
            }
            #undef GEN_OP
            *outDataType = ltype;
        }
        
        info.code->add({BC_PUSH,BC_REG_RAX});
        
    }
    return GEN_SUCCESS;
}
int GenerateBody(GenInfo& info, ASTBody* body){
    using namespace engone;
    Assert(body)

    bool first=true;
    ASTFunction* function = 0;
    while(true){
        if(first){
            first=false;
            function = body->function;
        } else
            function = function->next;
        if(!function)
            break;
        
        {
            SCOPE_LOG("FUNCTION")
            
            auto func = info.addFunction(*function->name);
            func->astFunc = function;
            
            
            info.code->add({BC_JMP});
            int skipIndex=info.code->length();
            info.code->add(0);
            
            func->address = info.code->length();
            ASTBody* body;
            int result = GenerateBody(info,body);
            
            *((u32*)info.code->get(skipIndex)) = info.code->length();
            
                    // TODO: nonetype is only valid for as implicit initial decleration.
                    //    Not allowed afterwards? or maybe it's find we just check variable type
                    //    not looking at the statement data type?
                    if(!var){
                        var = info.addVariable(*statement->name);
                        var->dataType = statement->dataType;
                        // data type may be zero if it wasn't specified during initial assignment
                        // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                        var->frameOffset = info.nextFrameOffset;
                        info.nextFrameOffset+=8;
                    }
                    var->dataType = dtype;
                    info.code->add({BC_LI,BC_REG_RBX});
                    info.code->add(var->frameOffset);
                    info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX}); // rbx = fp + offset
                    info.code->add({BC_POP,BC_REG_RDX});
                    info.code->add({BC_MOV_RM,BC_REG_RDX,BC_REG_RBX});
                    // move forward stack pointer
                    info.code->add({BC_LI,BC_REG_RCX});
                    info.code->add(8);
                    info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
                }else{
                    ERR() << "Type mismatch for variable "<<*statement->name<<". Should be "<<*info.ast->getDataType(statement->dataType)<<" but was "<<*info.ast->getDataType(dtype)<<"\n";
                    continue;
                    // TODO: Show were in the code
                }
            }
            log::out << " "<<*statement->name << " : "<<*info.ast->getDataType(dtype)<<"\n";   
        }
    }
    
    bool first=true;
    ASTStatement* statement = 0;
    while(true){
        if(first){
            first=false;
            statement = body->statement;
        } else
            statement = statement->next;
        if(!statement)
            break;

        if(statement->type==ASTStatement::ASSIGN){
            SCOPE_LOG("ASSIGN")
            DataType dtype=0;
            int result = GenerateExpression(info, statement->expression,&dtype);

            auto var = info.getVariable(*statement->name);
            if(result==GEN_ERROR) {

            } else if(dtype==AST_NONETYPE){
                ERR() << "datatype for assignment cannot be NONE\n";
            }else{
                if(dtype==statement->dataType || statement->dataType==AST_NONETYPE){
                    // TODO: nonetype is only valid for as implicit initial decleration.
                    //    Not allowed afterwards? or maybe it's find we just check variable type
                    //    not looking at the statement data type?
                    if(!var){
                        var = info.addVariable(*statement->name);
                        var->dataType = statement->dataType;
                        // data type may be zero if it wasn't specified during initial assignment
                        // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                        var->frameOffset = info.nextFrameOffset;
                        info.nextFrameOffset+=8;
                    }
                    var->dataType = dtype;
                    info.code->add({BC_LI,BC_REG_RBX});
                    info.code->add(var->frameOffset);
                    info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX}); // rbx = fp + offset
                    info.code->add({BC_POP,BC_REG_RDX});
                    info.code->add({BC_MOV_RM,BC_REG_RDX,BC_REG_RBX});
                    // move forward stack pointer
                    info.code->add({BC_LI,BC_REG_RCX});
                    info.code->add(8);
                    info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
                }else{
                    ERR() << "Type mismatch for variable "<<*statement->name<<". Should be "<<*info.ast->getDataType(statement->dataType)<<" but was "<<*info.ast->getDataType(dtype)<<"\n";
                    continue;
                    // TODO: Show were in the code
                }
            }
            log::out << " "<<*statement->name << " : "<<*info.ast->getDataType(dtype)<<"\n";
        } else if (statement->type == ASTStatement::IF){
            SCOPE_LOG("IF")
            DataType dtype=0;
            int result = GenerateExpression(info,statement->expression,&dtype);
            if(result==GEN_ERROR){
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERRT(statement->expression->token) << "Expected a boolean, not '"<<DataTypeToStr(dtype)<<"'\n";
            //     continue;
            // }

            info.code->add({BC_POP,BC_REG_RAX});
            info.code->add({BC_JNE,BC_REG_RAX});
            u32 skipIfBodyIndex = info.code->length();
            info.code->add(0);

            result = GenerateBody(info,statement->body);
            if(result==GEN_ERROR)
                continue;

            u32 skipElseBodyIndex = -1;
            if(statement->elseBody){
                info.code->add({BC_JMP});
                skipElseBodyIndex = info.code->length();
                info.code->add(0);
            }

            // fix address for jump instruction
            *(u32*)info.code->get(skipIfBodyIndex) = info.code->length();

            if(statement->elseBody){
                result = GenerateBody(info,statement->elseBody);
                if(result==GEN_ERROR)
                    continue;

                *(u32*)info.code->get(skipElseBodyIndex) = info.code->length();
            }
        }else if(statement->type == ASTStatement::WHILE){
            SCOPE_LOG("WHILE")

            u32 loopAddress = info.code->length();

            DataType dtype=0;
            int result = GenerateExpression(info,statement->expression,&dtype);
            if(result==GEN_ERROR){
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERRT(statement->expression->token) << "Expected a boolean, not '"<<DataTypeToStr(dtype)<<"'\n";
            //     continue;
            // }

            info.code->add({BC_POP,BC_REG_RAX});
            info.code->add({BC_JNE,BC_REG_RAX});
            u32 endIndex = info.code->length();
            info.code->add(0);

            result = GenerateBody(info,statement->body);
            if(result==GEN_ERROR)
                continue;

            info.code->add({BC_JMP});
            info.code->add(loopAddress);

            // fix address for jump instruction
            *(u32*)info.code->get(endIndex) = info.code->length();
        }
    }
    return GEN_SUCCESS;
}
BytecodeX* Generate(AST* ast, int* err){
    using namespace engone;
    _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)
    
    GenInfo info{};
    info.code = BytecodeX::Create();
    info.ast = ast;
    
    int result = GenerateBody(info,info.ast->body);
    // TODO: what to do about result?
    
    if(info.errors)
        log::out << log::RED<<"Generator failed with "<<info.errors<<" errors\n";
    if(err)
        *err = info.errors;
    return info.code;
}