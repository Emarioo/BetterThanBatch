
// #define ARG_REG 1
// #define ARG_CONST 2
// #define ARG_NUMBER 4
// #define ARG_STRING 8
// // regIndex: index of the register in the instruction
// bool GenInstructionArg(ParseInfo& info, int instType, int& num, int flags, uint8 regIndex){
//     using namespace engone;
//     Assert(("CompileArg... flags must be ARG_REG or so ...",flags!=0));
    
//     Token now = info.now(); // make sure prev exists
//     if(now.flags&TOKEN_SUFFIX_LINE_FEED){
//         ERRTL(now) << "Expected arguments\n";
//         return false;
//     }
//     if(0==(now.flags&TOKEN_SUFFIX_SPACE)){
//         ERRTL(now) << "Expected a space after "<< now <<"\n";
//         return false;
//     }
//     CHECK_ENDR;
//     Token token = info.next();
//     if(flags&ARG_REG){
//         if(token=="$"){
//             if(token.flags&TOKEN_SUFFIX_LINE_FEED){
//                 ERRTOKL <<"Expected a register ($integer or $ab)\n";
//                 return false;
//             } else if(token.flags&TOKEN_SUFFIX_SPACE){
//                 ERRTOKL << "Expected a register without space ($integer or $ab)\n";
//                 return false;
//             }
//             CHECK_ENDR
//             token = info.next();
            
//             if(IsInteger(token)){
//                 num = ConvertInteger(token);
//                 return true;
//             }
//             if(token.length==1){
//                 char s = *token.str;
//                 if(*token.str>='a'&&*token.str<='z')
//                     num = s-'a' + 10;
//                 else if(*token.str>='A'&&*token.str<='Z')
//                     num = s-'A' + 10;
//                 else{
//                     ERRTOK <<"Expected $integer or $ab\n";
//                     return false;
//                 }
//             }else if(token.length==2){
//                 char s0 = *token.str;
//                 char s1 = *(token.str+1);
//                 if(token == REG_FRAME_POINTER_S)
//                     num = REG_FRAME_POINTER;
//                 // else if(token == REG_STACK_POINTER_S)
//                 //     num = REG_STACK_POINTER;
//                 else if(token == REG_RETURN_VALUE_S)
//                     num = REG_RETURN_VALUE;
//                 else if(token == REG_ARGUMENT_S)
//                     num = REG_ARGUMENT;
//                 else if(s0>='a'&&s0<='z' && s1>='a'&&s1<='z')
//                     num = s1-'a' + ('z'-'a')*(s0-'a') + 10;
//                 else if(s0>='A'&&s0<='Z' && s1>='A'&&s1<='Z')
//                     num = s1-'A' + ('Z'-'A')*(s0-'A') + 10;
//                 else{
//                     ERRTOK << "Expected $integer or $ab\n";
//                     return false;
//                 }
//             }else{
//                 ERRTOK  <<"Expected $integer or $ab\n";
//                 return false;
//             }
//             return true;
//         }
//     }
//     if(flags&ARG_CONST){
//         if((token.flags&TOKEN_QUOTED)==0&&IsName(token)){
//             if(instType!=BC_LOADNC){
//                 // resolve other instruction like jump by using load const
//                 num = REG_ACC0 + regIndex;
//                 int index=0;
//                 // info.code.add(BC_LOADC,num);
//                 // info.code.add(*(Instruction*)&index);
                
//                 info.code.addLoadNC(num,index);

//                 info.instructionsToResolve.push_back({info.code.length()-1,token});
//                 log::out << info.code.get(info.code.length()-1) << " (resolve "<<token<<")\n";
//                 // log::out << info.code.get(info.code.length()-2) << " (resolve "<<token<<")\n";
//             }else{
//                 // resolve bc_load_const directly
//                 // num = REG_TEMP0 + regIndex;
//                 // info.instructionsToResolve.push_back({info.code.length()+1,token});
//                 info.instructionsToResolve.push_back({info.code.length(),token});
//             }
//             return true;
//         }
//     }
//     if(flags&ARG_NUMBER){
//         if(IsDecimal(token)){
//             Decimal number = ConvertDecimal(token);
//             uint index = info.code.addConstNumber(number);
//             _GLOG(log::out<<"Constant ? = "<<number<<" to "<<index<<"\n";)
//             num = REG_ACC0 + regIndex;
//             info.code.add(BC_NUM,num);
//             _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)
//             info.code.addLoadNC(num,index);
//             _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)

//             // info.code.add(BC_LOADC,num);
//             // _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)
//             // info.code.add(*(Instruction*)&index); // address/index
//             return true;
//         }
//     }
//     if(flags&ARG_STRING){
//         if(token.flags&TOKEN_QUOTED){
//             uint index = info.code.addConstString(token);
//             _GLOG(log::out<<"Constant ? = "<<token<<" to "<<index<<"\n";)
//             num = REG_ACC0 + regIndex;
//             info.code.add(BC_STR,num);
//             _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)

//             info.code.addLoadSC(num,index);
//             _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)

//             // info.code.add(BC_LOADC,num);
//             // _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)
//             // info.code.add(*(Instruction*)&index); // address/index
//             return true;
//         }
//     }
//     ERRTOK <<"Expected ";
//     if(flags&ARG_REG){
//         log::out <<"register (eg. $23, $d)";
//     }
//     if(flags&ARG_CONST){
//         if(flags&ARG_REG){
//             if((flags&ARG_NUMBER)||(flags&ARG_STRING))
//                 log::out << ", ";
//             else
//                 log::out << " or ";
//         }
//         log::out <<"constant";
//     }
//     if(flags&ARG_NUMBER){
//         if((flags&ARG_REG)||(flags&ARG_CONST)){
//             if((flags&ARG_NUMBER)||(flags&ARG_STRING))
//                 log::out << ", ";
//             else
//                 log::out << " or ";
//         }
//         log::out <<"decimal";
//     }
//     if(flags&ARG_STRING){
//         if((flags&ARG_REG)||(flags&ARG_CONST)||(flags&ARG_NUMBER))
//             log::out << " or ";
//         log::out <<"quotes";
//     }
//     log::out << "\n";
//     return false;
// }
// Bytecode GenerateInstructions(Tokens& tokens, int* outErr){
//     using namespace engone;
//     _VLOG(log::out<<"\n##   Generator (instructions)   ##\n";)
    
//     std::unordered_map<std::string,int> instructionMap;
//     #define MAP(K,V) instructionMap[K] = V;
    
//     MAP("add",BC_ADD)
//     MAP("sub",BC_SUB)
//     MAP("mul",BC_MUL)
//     MAP("div",BC_DIV)
//     MAP("less",BC_LESS)
//     MAP("greater",BC_GREATER)
//     MAP("equal",BC_EQUAL)
//     MAP("nequal",BC_NOT_EQUAL)
//     MAP("and",BC_AND)
//     MAP("or",BC_OR)
    
//     MAP("jumpnif",BC_JUMPNIF)

//     MAP("mov",BC_MOV)
//     MAP("copy",BC_COPY)
//     MAP("run",BC_RUN)
//     MAP("loadv",BC_LOADV)
//     MAP("storev",BC_STOREV)

//     MAP("num",BC_NUM)
//     MAP("str",BC_STR)
//     MAP("del",BC_DEL)
//     MAP("jump",BC_JUMP)
//     MAP("loadc",BC_LOADNC)
//     // MAP("push",BC_PUSH)
//     // MAP("pop",BC_POP)
//     MAP("return",BC_RETURN)
//     // MAP("enter",BC_ENTERSCOPE)
//     // MAP("exit",BC_EXITSCOPE)

//     ParseInfo info{tokens};

//     while(!info.end()){
//         Token token = info.next();
        
//         if(token.flags&TOKEN_SUFFIX_LINE_FEED){
//             ERRTOK << "Expected something after " << token << "\n";
//             info.nextLine();
//             continue;
//         }

//         // first we expect an adress or instruction name
//         Token name = token;
    
//         CHECK_END
//         token = info.next();

//         if(token == ":"){
//             // Jump address or constant
//             if((token.flags & TOKEN_SUFFIX_LINE_FEED)||info.end()){
//                 // Jump adress
//                 uint address = info.code.length();
//                 int index = info.code.addConstNumber(address);
//                 info.nameOfNumberMap[name] = index;
//                 _GLOG(log::out<<"Address " << name <<" = "<<address<<"\n";)
//             }else{
//                 CHECK_END
//                 token = info.next();
//                 if(0==(token.flags&TOKEN_SUFFIX_LINE_FEED)){
//                     ERRTOK <<"Expected line feed after "<<token<<"\n";
//                     info.nextLine();
//                     continue;
//                 }
//                 if(IsDecimal(token)){
//                     Decimal number = ConvertDecimal(token);
//                     int index = info.code.addConstNumber(number);
//                     info.nameOfNumberMap[name] = index;
//                     _GLOG(log::out<<"Constant " << name<<" = "<<number<<" to "<<index<<"\n";)
//                 }else if(token.flags&TOKEN_QUOTED){
//                     int index = info.code.addConstString(token);
//                     info.nameOfStringMap[name] = index;
//                     _GLOG(log::out<<"Constant " << name<<" = "<<token<<" to "<<index<<"\n";)
//                 }else {
//                     ERRTOK << " " <<token<<" cannot be a constant\n";
//                     info.nextLine();
//                     continue;   
//                 }
//             }
//         } else if(0==(name.flags & TOKEN_SUFFIX_SPACE)){
//             ERRTL(name) <<"Expected a space\n";
//             info.nextLine();
//             continue;
//         } else {
//             // Instruction confirmed
//             auto find = instructionMap.find(name);
//             if(find==instructionMap.end()){
//                 ERRTOK <<"Invalid instruction " << name<<"\n";
//                 info.nextLine();
//                 continue;
//             }
//             int instType = find->second;
//             info.revert();
            
//             int regCount = ((instType & BC_MASK) >> 6);
            
//             Instruction inst{};
//             inst.type = instType;
            
//             bool error=false;
//             int tmp=0;
//             for(int i=0;i<regCount;i++){
//                 // if(i==2 && instType==BC_JUMPIF){
//                 //     if(!CompileInstructionArg(info,tmp,ARG_CONST_NUMBER|ARG_REG)){
//                 //         info.nextLine();
//                 //         continue;
//                 //     }
//                 // } else 
//                 if(!GenInstructionArg(info,instType,tmp,ARG_REG|ARG_CONST|ARG_NUMBER|ARG_STRING,i)) {
//                     error=true;
//                     break;
//                 }
//                 inst.regs[i] = tmp;
//             }
//             if(error){
//                 info.nextLine();
//                 continue;
//             }
            
//             if(instType == BC_LOADNC||instType==BC_LOADSC){
//                 if(!GenInstructionArg(info,instType,tmp,ARG_CONST,1)) {
//                     info.nextLine();
//                     continue;
//                 }
//             }
            
//             info.code.add(inst);
//             _GLOG(log::out << info.code.get(info.code.length()-1)<<("\n");)
//             // if(instType==BC_LOADC){
//             //     int zeroInst=0;
//             //     info.code.add(*(Instruction*)&zeroInst);
//             // }
//             if(LineEndError(info))
//                 continue;
//         }
//     }

//     for(auto& addr : info.instructionsToResolve){
//         Instruction& inst = info.code.get(addr.instIndex);
//         auto find = info.nameOfNumberMap.find(addr.token);
//         int index = -1;
//         if(find==info.nameOfNumberMap.end()){
//             auto find2 = info.nameOfStringMap.find(addr.token);
//             if(find2==info.nameOfStringMap.end()){
//                 ERRT(addr.token) <<"Constant "<<addr.token<<" could not be resolved\n";
//             }else{
//                 index = find2->second;
//             }
//         }else{
//             index = find->second;
//         }
//         if(inst.type==BC_LOADNC||inst.type==BC_LOADSC){
//             inst.reg1 = index&0xFF;
//             inst.reg2 = (index>>8)&0xFF;
//             // inst = *(Instruction*)&index;

//         }else{
//             ERRT(addr.token)<<"Cannot resolve\n";
//         }
//     }

//     return info.code;
// }
