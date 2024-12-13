#include "BetBat/decl_gen.h"

#include "Engone/PlatformLayer.h"

std::string ToCTypeName(AST* ast, TypeId type) {
    std::string out = "";
    switch(type.baseType().getId()){
        // case TYPE_VOID:
        case TYPE_UINT8: out = "unsigned char"; break;
        case TYPE_UINT16: out = "unsigned short"; break;
        case TYPE_UINT32: out = "unsigned int"; break;
        case TYPE_UINT64: out = "unsigned long int"; break;
        case TYPE_INT8: out = "char"; break;
        case TYPE_INT16: out = "short"; break;
        case TYPE_INT32: out = "int"; break;
        case TYPE_INT64: out = "long int"; break;
        // case TYPE_BOOL:
        // case TYPE_CHAR:
        case TYPE_FLOAT32: out = "float"; break;
        case TYPE_FLOAT64: out = "double"; break;
        default: {
            auto info = ast->getTypeInfo(type.baseType());
            if(info->funcType) {
                // wait a second....
                // I have to generate function pointers!?
                return "void*";
            }
            if(info->astStruct && info->astStruct->polyArgs.size()) {
                Assert(info->structImpl);
                out = info->astStruct->name;
                for(auto& arg : info->structImpl->polyArgs) {
                    out += "_" + ToCTypeName(ast, arg);
                }
                return out;
            }
            return ast->typeToString(type);
        }
    }
    for(int i=0;i<type.getPointerLevel();i++)
        out += "*";
    return out;
}

bool WriteDeclFiles(const std::string& lib_path, Bytecode* bytecode, AST* ast, bool is_dynamic_library) {
    using namespace engone;
    
    enum FileDeclType {
        DECL_BTB = 1,
        DECL_C = 2,
    };
    int file_type = DECL_BTB|DECL_C;
    std::string raw_name = "";
    int slash = lib_path.find_last_of("/");
    int dot = lib_path.find_last_of(".");
    if(slash == -1) {
        raw_name = lib_path.substr(0, dot);
    } else if (slash < dot) {
        raw_name = lib_path.substr(slash+1, dot - slash-1);
    } else {
        raw_name = lib_path.substr(slash+1);
    }
    std::string load_lib_name = raw_name;
    std::string upper_raw_name = raw_name;
    for(int i=0;i<upper_raw_name.size();i++)
        ((char*)upper_raw_name.data())[i] = upper_raw_name[i] & ~32;
    std::string decl_path_no_ext = lib_path.substr(0,dot) + "_decl";
    
    std::string text_btb{};
    std::string text_c{};
    text_btb += "/*\n";
    // TOO: Trim exe path in case it's absolute? Users probably don't want to distribute their absolute path.
    text_btb += "    Auto-generated import declarations for " + lib_path + "\n";
    text_btb += "*/\n";
    text_btb += "\n";
    text_c += text_btb; // comment same for both
    
    if (file_type & DECL_BTB){
        text_btb += "#load \"";
        // TODO: Path calculation does not handle all cases
        if(lib_path[0] != '.' || lib_path[1] != '/') {
            if(lib_path[1] != '/') {
                text_btb += "./" + lib_path;
            } else {
                text_btb += "." + lib_path;
            }
        }
        text_btb += "\" as " + load_lib_name + "\n";
        text_btb += "\n";
    }
    if(file_type & DECL_C) {
        text_c += "#pragma once\n";
        text_c += "\n";
        
        if (is_dynamic_library) {
            text_c += "#ifndef "+upper_raw_name+"_DLL\n";
            text_c += "    #define "+upper_raw_name+"_DLL\n";
            text_c += "#endif\n";
        }
        text_c += "#if defined(__GNUC__) || defined(__clang__)\n";
        text_c += "    #ifdef "+upper_raw_name+"_DLL\n";
        text_c += "        #define "+upper_raw_name+"_API __attribute__((visibility(\"default\")))\n";
        text_c += "    #else\n";
        text_c += "        #define "+upper_raw_name+"_API\n";
        text_c += "    #endif\n";
        text_c += "    #define "+upper_raw_name+"_ALIAS(N) __attribute__((alias(N)))\n";
        text_c += "#elif defined(_MSC_VER)\n";
        text_c += "    #ifdef "+upper_raw_name+"_DLL\n";
        text_c += "        #define "+upper_raw_name+"_API __declspec(dllimport)\n";
        text_c += "    #else\n";
        text_c += "        #define "+upper_raw_name+"_API\n";
        text_c += "    #endif\n";
        text_c += "    #define "+upper_raw_name+"_ALIAS(N) __declspec(alias(N))\n";
        // text_c += "#else\n";
        // text_c += "    #define "+upper_raw_name+"_API\n";
        // text_c += "    #define "+upper_raw_name+"_ALIAS(N)\n";
        text_c += "#endif\n";
        
        text_c += "\n";
        text_c += "#ifdef __cplusplus\n";
        text_c += "extern \"C\" {\n";
        text_c += "#endif\n";
        text_c += "\n";
    }
    
    // =======================================
    //        List types from functions 
    // =======================================
    
    DynamicArray<bool> emitted_types{};
    DynamicArray<bool> defined_types{};
    emitted_types.resize(ast->_typeInfos.size());
    log::out << log::YELLOW << "Total types: "<<emitted_types.size()<<"\n";
    
    DynamicArray<TinyBytecode*> tinycodes{};
    DynamicArray<TypeId> types{};
    for(int fi=0;fi<bytecode->exportedFunctions.size();fi++) {
        auto& tinycode = bytecode->tinyBytecodes[bytecode->exportedFunctions[fi].tinycode_index];
        if(!tinycode->funcImpl) {
            log::out << log::YELLOW << "No funcimpl for "<<tinycode->name<<"\n";
            continue;
        }
        Assert(tinycode->funcImpl->signature.polyArgs.size() == 0);
        
        // we add tinycodes to a list so we don't have to have duplicated filter out logic
        // in other loop over tinycodes. (logic that checks !tinycode->funcImpl)
        tinycodes.add(tinycode);
        
        for(int i=0;i<tinycode->funcImpl->signature.argumentTypes.size();i++) {
            auto& arg_type = tinycode->funcImpl->signature.argumentTypes[i].typeId;
            if(!emitted_types[arg_type.baseType().getId()]) {
                types.add(arg_type.baseType());
                emitted_types[arg_type.baseType().getId()] = true;
            }
        }
        for(int i=0;i<tinycode->funcImpl->signature.returnTypes.size();i++) {
            auto& ret_type = tinycode->funcImpl->signature.returnTypes[i].typeId;
            if(!emitted_types[ret_type.baseType().getId()]) {
                types.add(ret_type.baseType());
                emitted_types[ret_type.baseType().getId()] = true;
            }
         }
    }
    
    // =====================================
    //     Recursively find used types
    // =====================================
    
    for(int ti=0;ti<types.size();ti++) {
        TypeId type = types[ti];
        TypeInfo* typeinfo = ast->getTypeInfo(type);
        
        if (typeinfo->structImpl) {
            if(typeinfo->astStruct->polyArgs.size() != 0) {
                TypeId id = typeinfo->astStruct->base_typeId;
                if(!emitted_types[id.getId()]) {
                    types.add(id.baseType());
                    emitted_types[id.getId()] = true;
                }
            }
            for(int i=0;i<typeinfo->astStruct->members.size();i++) {
                auto& mem_impl = typeinfo->structImpl->members[i];
                if(!emitted_types[mem_impl.typeId.baseType().getId()]) {
                    types.add(mem_impl.typeId.baseType());
                    emitted_types[mem_impl.typeId.baseType().getId()] = true;
                }
            }
        } else if(typeinfo->funcType) {
            Assert(typeinfo->funcType->polyArgs.size() == 0);
            for(int i=0;i<typeinfo->funcType->argumentTypes.size();i++) {
                auto& arg_type = typeinfo->funcType->argumentTypes[i].typeId;
                if(!emitted_types[arg_type.baseType().getId()]) {
                    types.add(arg_type.baseType());
                    emitted_types[arg_type.baseType().getId()] = true;
                }
            }
            for(int i=0;i<typeinfo->funcType->returnTypes.size();i++) {
                auto& ret_type = typeinfo->funcType->argumentTypes[i].typeId;
                if(!emitted_types[ret_type.baseType().getId()]) {
                    types.add(ret_type.baseType());
                    emitted_types[ret_type.baseType().getId()] = true;
                }
            }
        }
    }
    
    // =====================================
    //     Emit types in reverse ensuring that structs 
    ///    referred to by other structs are emitted first.
    // =====================================
    
    for (int ti=types.size()-1;ti>=0;ti--) {
        TypeId type = types[ti];
        TypeInfo* typeinfo = ast->getTypeInfo(type);
        
        log::out << "Emit " << log::LIME << ast->typeToString(type) << "\n";
        
        // TODO: Add comments from original file where type was defined
        if(typeinfo->astEnum) {
            if(file_type & DECL_BTB) {
                text_btb += "enum ";
                text_btb += typeinfo->astEnum->name;
                if(typeinfo->astEnum->typeId != TYPE_INT32) {
                    text_btb += " : " + ast->typeToString(typeinfo->astEnum->typeId) + " {\n";
                } else {
                    text_btb += " {\n";
                }
            }
            if(file_type & DECL_C) {
                text_c += "typedef enum ";
                // TODO: annotations
                text_c += typeinfo->astEnum->name;
                if(typeinfo->astEnum->typeId != TYPE_INT32 || typeinfo->astEnum->typeId != TYPE_UINT32) {
                    text_btb += " {\n";
                } else {
                    // Enums are always int in C, C++ allows ": inttype"
                    // TODO: How we deal with it?
                    text_btb += " : " + ToCTypeName(ast, typeinfo->astEnum->typeId) + " {\n";
                }
            }
            
            int max_name_len = 0;
            for(int i=0;i<typeinfo->astEnum->members.size();i++) {
                auto& mem = typeinfo->astEnum->members[i];
                if(mem.name.size() > max_name_len)
                    max_name_len = mem.name.size();
            }
            for(int i=0;i<typeinfo->astEnum->members.size();i++) {
                auto& mem = typeinfo->astEnum->members[i];
                if(file_type & DECL_BTB) {
                    text_btb += "    " + mem.name;
                    for(int j=0;j<max_name_len - mem.name.size();j++)
                        text_btb += " ";
                    text_btb += " = " + std::to_string(mem.enumValue) + ",\n";
                }
                if(file_type & DECL_C) {
                    text_c += "    " + mem.name;
                    for(int j=0;j<max_name_len - mem.name.size();j++)
                        text_btb += " ";
                    text_c += " = " + std::to_string(mem.enumValue) + ",\n";
                }
            }
            if(file_type & DECL_BTB) {
                text_btb += "}\n";
            }
            if(file_type & DECL_C) {
                text_c += "} "+typeinfo->astEnum->name+";\n";
            }
        } else if (typeinfo->structImpl) {
            // Generate polymorphic type for BTB
            // Generate specialized type for C
            auto prev_file_type = file_type;
            auto file_type = prev_file_type; // this allows us to disable BTB or C code generation
            
            if(typeinfo->astStruct->polyArgs.size() == 0) {
                
            } else if(typeinfo->structImpl) {
                // Don't generate derived polymorphic struct for BTB
                // We create the base polymoprhic struct instead
                file_type &= ~DECL_BTB;
            } else {
                file_type &= ~DECL_C;
                // Don't generate C code for base polymorphic struct.
                // C doesn't have template/generics/polymorphism.
                // We generate the specialized structs instead.
            }
            int max_name_len = 0;
            for(int i=0;i<typeinfo->astStruct->members.size();i++) {
                if(file_type & DECL_BTB) {
                    auto& mem = typeinfo->astStruct->members[i];
                    if(mem.name.size() > max_name_len)
                        max_name_len = mem.name.size();
                }
                if(file_type & DECL_C) {
                    auto& mem_impl = typeinfo->structImpl->members[i];
                    std::string type_name = ToCTypeName(ast, mem_impl.typeId);
                    if(type_name.size() > max_name_len)
                        max_name_len = type_name.size();
                }
            }
            if(file_type & DECL_BTB) {
                text_btb += "struct ";
                if(typeinfo->astStruct->no_padding) {
                    text_btb += "@no_padding ";
                }
                // TODO: annotations
                text_btb += typeinfo->astStruct->name;
                if(typeinfo->astStruct->polyArgs.size()) {
                    text_btb += "<";
                    for(int i=0;i<typeinfo->astStruct->polyArgs.size();i++) {
                        auto& arg = typeinfo->astStruct->polyArgs[i];
                        if(i!=0)
                            text_btb += ",";
                        text_btb += arg.name;
                    }
                    text_btb += ">";
                }
                text_btb += " {\n";
            }
            if(file_type & DECL_C) {
                if(typeinfo->astStruct->no_padding)
                    text_c += "#pragma pack(push, 1)\n";
                text_c += "typedef struct {\n";
                // TODO: annotations
            }
            for(int i=0;i<typeinfo->astStruct->members.size();i++) {
                auto& mem = typeinfo->astStruct->members[i];
                auto& mem_impl = typeinfo->structImpl->members[i];
                
                // TODO: Handle array_length expression in the future (just a literal for now)
                if(file_type & DECL_BTB) {
                    text_btb += "    " + mem.name + ": ";
                    for(int j=0;j<max_name_len - mem.name.size();j++)
                        text_btb += " ";
                    text_btb += ast->typeToString(mem_impl.typeId);
                    if(mem.array_length > 0) {
                        text_btb += "[" + std::to_string(mem.array_length) + "]";
                    }
                    text_btb += ";\n";
                }
                if(file_type & DECL_C) {
                    std::string type_name = ToCTypeName(ast, mem_impl.typeId);
                    if (mem_impl.typeId.baseType() == type) {
                        text_c += "    struct " + type_name;
                    } else {
                        text_c += "    " + type_name;
                    }
                    for(int j=0;j<max_name_len - type_name.size();j++)
                        text_c += " ";
                    text_c += +" " + mem.name;
                    if(mem.array_length > 0) {
                        text_c += "[" + std::to_string(mem.array_length) + "]\n";
                    }
                    text_c += ";\n";
                }
                // TODO: Add default value
            }
            if(file_type & DECL_BTB) {
                text_btb += "}\n";
            }
            if(file_type & DECL_C) {
                text_c += "} ";
                text_c += typeinfo->astStruct->name;
                for(int i=0;i<typeinfo->structImpl->polyArgs.size();i++) {
                    auto& arg = typeinfo->structImpl->polyArgs[i];
                    text_c += "_";
                    text_c +=  ToCTypeName(ast, arg);
                    // We do not need to add poly type to 'types' list because
                    // if it's used in a member then it will be generated, if not then we don't need it.
                }
                text_c += ";\n";
                if(typeinfo->astStruct->no_padding)
                    text_c += "#pragma pack(pop)\n";
            }
        }
        // other types are primitives
    }
    
    if(file_type & DECL_BTB) {
        text_btb += "\n";
    }
    if(file_type & DECL_C) {
        text_c += "\n";
    }
    
    // ========================
    //     Emit functions
    // ========================
    
    for(auto tinycode : tinycodes) {
        // TODO: Add comment if the exported function had one above it?
        auto astfunc = tinycode->funcImpl->astFunction;
        auto& signature = tinycode->funcImpl->signature;
        if(file_type & DECL_BTB) {
            text_btb += "fn ";
            if(bytecode->target == TARGET_WINDOWS_x64) {
                if(signature.convention != STDCALL) {
                    text_btb += "@" + std::string(ToString(signature.convention)) + " ";
                }
            } else if(bytecode->target == TARGET_LINUX_x64) {
                if(signature.convention != UNIXCALL) {
                    text_btb += "@" + std::string(ToString(signature.convention)) + " ";
                }
            } else {
                text_btb += "@" + std::string(ToString(signature.convention)) + " ";
            }
            text_btb += "@import(" + load_lib_name;
            if(astfunc->export_alias.size() != 0 && astfunc->export_alias != tinycode->name)
                text_btb += ", alias=\""+astfunc->export_alias+"\"";
            text_btb += ") ";
            text_btb += tinycode->name + "(";
            
            for(int i=0;i<signature.argumentTypes.size();i++) {
                auto& arg_type = signature.argumentTypes[i].typeId;
                auto& arg = astfunc->arguments[i];
                
                if(i != 0)
                    text_btb += ", ";
                    
                text_btb += arg.name + ": " + ast->typeToString(arg_type);
                
                // TODO: Add default value
            }
            text_btb += ")";
            if(signature.returnTypes.size() != 0)
                text_btb += " -> ";
            for(int i=0;i<signature.returnTypes.size();i++) {
                auto& ret_type = signature.returnTypes[i].typeId;
                if(i != 0)
                    text_btb += ", ";
                    
                text_btb += ast->typeToString(ret_type);
            }
            text_btb += ";\n";
        }
        if(file_type & DECL_C) {
            text_c += upper_raw_name + "_API ";
            if(astfunc->export_alias.size() != 0 && astfunc->export_alias != tinycode->name)
                text_c += upper_raw_name + "_ALIAS(\""+astfunc->export_alias+"\") ";
            
            Assert(signature.returnTypes.size() <= 1);
            if(signature.returnTypes.size() == 1) {
                auto& ret_type = signature.returnTypes[0].typeId;
                text_c += ToCTypeName(ast, ret_type) + " ";
            } else {
                text_c += "void ";
            }
            
            text_c += tinycode->name + "(";
            
            for(int i=0;i<signature.argumentTypes.size();i++) {
                auto& arg_type = signature.argumentTypes[i].typeId;
                auto& arg = astfunc->arguments[i];
                
                if(i != 0)
                    text_c += ", ";
                    
                text_c += ToCTypeName(ast, arg_type) + " " + arg.name;
            }
            text_c += ");\n";
        }
    }
    
    if(file_type & DECL_C) {
        text_c += "\n";
        text_c += "#ifdef __cplusplus\n";
        text_c += "} // extern \"C\"\n";
        text_c += "#endif\n";
        text_c += "\n";
        text_c += "#undef " + upper_raw_name + "_API\n";
        text_c += "#undef " + upper_raw_name + "_ALIAS\n";
    }
    
    // =======================
    //     Write the file
    // =======================
    
    if(file_type & DECL_BTB) {
        auto file = FileOpen(decl_path_no_ext + ".btb", FILE_CLEAR_AND_WRITE);
        FileWrite(file, text_btb.c_str(), text_btb.size());
        FileClose(file);
    }
    if(file_type & DECL_C) {
        auto file = FileOpen(decl_path_no_ext + ".h", FILE_CLEAR_AND_WRITE);
        FileWrite(file, text_c.c_str(), text_c.size());
        FileClose(file);
    }
    return true;
}