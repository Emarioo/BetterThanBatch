#include "BetBat/DWARF.h"
#include "BetBat/Compiler.h"

namespace dwarf {
    void ProvideSections(ObjectFile* objectFile, Program* program, Compiler* compiler, bool provide_section_data) {
        using namespace engone;
        
        int FRAME_SIZE = compiler->arch.FRAME_SIZE;
        int REGISTER_SIZE = compiler->arch.REGISTER_SIZE;
        TargetPlatform target = compiler->options->target;
        
        SectionNr section_info    = -1;
        SectionNr section_line    = -1;
        SectionNr section_abbrev  = -1;
        SectionNr section_aranges = -1;
        SectionNr section_frame   = -1;
        if(!provide_section_data) {
            section_info = objectFile->createSection(".debug_info", ObjectFile::FLAG_DEBUG);  
            section_line = objectFile->createSection(".debug_line", ObjectFile::FLAG_DEBUG);  
            section_abbrev = objectFile->createSection(".debug_abbrev", ObjectFile::FLAG_DEBUG);  
            section_aranges = objectFile->createSection(".debug_aranges", ObjectFile::FLAG_DEBUG);    
            section_frame = objectFile->createSection(".debug_frame", ObjectFile::FLAG_DEBUG);                
            return;
        } else {
            section_info = objectFile->findSection(".debug_info");
            section_line = objectFile->findSection(".debug_line");
            section_abbrev = objectFile->findSection(".debug_abbrev");
            section_aranges = objectFile->findSection(".debug_aranges");
            section_frame = objectFile->findSection(".debug_frame");
        }
        Assert(section_info != -1);
        Assert(section_line != -1);
        Assert(section_abbrev != -1);
        Assert(section_aranges != -1);
        Assert(section_frame != -1);

        SectionNr section_text = objectFile->findSection(".text");
        Assert(section_text != -1);
        auto stream_text = (const ByteStream*)objectFile->getStreamFromSection(section_text);
        SectionNr section_data = objectFile->findSection(".data");

        auto debug = program->debugInformation;
        ByteStream* stream = nullptr;

        bool suc = false;
        #define CHECK Assert(suc);

        // Note that X may be index++
        // X must therefore only be used once. Use a temporary variable if you need it more times.
        #define WRITE_LEB(X) \
            stream->write_unknown((void**)&ptr, 16); \
            written = ULEB128_encode(ptr, 16, X); \
            stream->wrote_unknown(written);
        #define WRITE_SLEB(X) \
            stream->write_unknown((void**)&ptr, 16); \
            written = SLEB128_encode(ptr, 16, X); \
            stream->wrote_unknown(written);
            
        #define WRITE_FORM(ATTR,FORM) \
            stream->write_unknown((void**)&ptr, 16); \
            written = ULEB128_encode(ptr, 16, ATTR); \
            stream->wrote_unknown(written);\
            stream->write_unknown((void**)&ptr, 16); \
            written = ULEB128_encode(ptr, 16, FORM); \
            stream->wrote_unknown(written);

        int abbrev_compUnit = 0;
        int abbrev_func = 0;
        int abbrev_var = 0;
        int abbrev_global_var = 0;
        int abbrev_param = 0;
        int abbrev_base_type = 0;
        int abbrev_pointer_type = 0;
        int abbrev_struct = 0;
        int abbrev_struct_member = 0;
        int abbrev_enum = 0;
        int abbrev_enum_member_64 = 0;
        int abbrev_enum_member_32 = 0;
        int abbrev_lexical_block = 0;

        stream = objectFile->getStreamFromSection(section_abbrev);
        {
            u8* ptr = nullptr;
            int written = 0;
            int nextAbbrevCode = 1;
            
            abbrev_compUnit = nextAbbrevCode++;
            WRITE_LEB(abbrev_compUnit) // code
            WRITE_LEB(DW_TAG_compile_unit) // tag
            stream->write1(DW_CHILDREN_yes);
            
            WRITE_FORM(DW_AT_producer,  DW_FORM_string)
            // WRITE_FORM(DW_AT_language,  DW_FORM_data1)
            WRITE_FORM(DW_AT_name,      DW_FORM_string)
            WRITE_FORM(DW_AT_comp_dir,  DW_FORM_string)
            if(REGISTER_SIZE == 4) {
                WRITE_FORM(DW_AT_low_pc,    DW_FORM_data4)
                WRITE_FORM(DW_AT_high_pc,   DW_FORM_data4)
            } else {
                WRITE_FORM(DW_AT_low_pc,    DW_FORM_addr)
                WRITE_FORM(DW_AT_high_pc,   DW_FORM_addr)
            }
            WRITE_FORM(DW_AT_stmt_list, DW_FORM_data4)
            WRITE_LEB(0) // value?
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_func = nextAbbrevCode++;
            WRITE_LEB(abbrev_func) // code
            WRITE_LEB(DW_TAG_subprogram) // tag
            stream->write1(DW_CHILDREN_yes);

            WRITE_FORM(DW_AT_external,     DW_FORM_flag) // DW_FORM_flag_present is used in DWARF 5
            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            // WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            if(REGISTER_SIZE == 4) {
                WRITE_FORM(DW_AT_low_pc,       DW_FORM_data4)
                WRITE_FORM(DW_AT_high_pc,      DW_FORM_data4)
            } else {
                WRITE_FORM(DW_AT_low_pc,       DW_FORM_addr)
                WRITE_FORM(DW_AT_high_pc,      DW_FORM_addr)
            }
            WRITE_FORM(DW_AT_frame_base,   DW_FORM_block1)
            // WRITE_FORM(DW_AT_call_all_tail_calls, DW_FORM_flag_present)
            WRITE_FORM(DW_AT_sibling,      DW_FORM_ref4)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation

            abbrev_var = nextAbbrevCode++;
            WRITE_LEB(abbrev_var) // code
            WRITE_LEB(DW_TAG_variable) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            WRITE_FORM(DW_AT_location,     DW_FORM_block1)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            // abbrev for globals is currently the same as a local variables
            abbrev_global_var = nextAbbrevCode++;
            WRITE_LEB(abbrev_global_var) // code
            WRITE_LEB(DW_TAG_variable) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            WRITE_FORM(DW_AT_location,     DW_FORM_block1)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_param = nextAbbrevCode++;
            WRITE_LEB(abbrev_param) // code
            WRITE_LEB(DW_TAG_formal_parameter) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            WRITE_FORM(DW_AT_location,     DW_FORM_block1)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_base_type = nextAbbrevCode++;
            WRITE_LEB(abbrev_base_type) // code
            WRITE_LEB(DW_TAG_base_type) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_byte_size,    DW_FORM_data1)
            WRITE_FORM(DW_AT_encoding,     DW_FORM_data1)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation

            abbrev_pointer_type = nextAbbrevCode++;
            WRITE_LEB(abbrev_pointer_type) // code
            WRITE_LEB(DW_TAG_pointer_type) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_byte_size,    DW_FORM_data1)
            WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation

            abbrev_struct = nextAbbrevCode++;
            WRITE_LEB(abbrev_struct) // code
            WRITE_LEB(DW_TAG_structure_type) // tag
            stream->write1(DW_CHILDREN_yes);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_byte_size,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_FORM(DW_AT_sibling,  DW_FORM_ref4)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation

            abbrev_struct_member = nextAbbrevCode++;
            WRITE_LEB(abbrev_struct_member) // code
            WRITE_LEB(DW_TAG_member) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,           DW_FORM_string)
            WRITE_FORM(DW_AT_type,           DW_FORM_ref4)
            WRITE_FORM(DW_AT_data_member_location, DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_enum = nextAbbrevCode++;
            WRITE_LEB(abbrev_enum) // code
            WRITE_LEB(DW_TAG_enumeration_type) // tag
            stream->write1(DW_CHILDREN_yes);

            WRITE_FORM(DW_AT_name,          DW_FORM_string)
            WRITE_FORM(DW_AT_byte_size,     DW_FORM_data1)
            WRITE_FORM(DW_AT_type,          DW_FORM_ref4)
            // WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_FORM(DW_AT_sibling,  DW_FORM_ref4)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_enum_member_64 = nextAbbrevCode++;
            WRITE_LEB(abbrev_enum_member_64) // code
            WRITE_LEB(DW_TAG_enumerator) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,           DW_FORM_string)
            WRITE_FORM(DW_AT_const_value,    DW_FORM_data8)
            // WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_enum_member_32 = nextAbbrevCode++;
            WRITE_LEB(abbrev_enum_member_32) // code
            WRITE_LEB(DW_TAG_enumerator) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,           DW_FORM_string)
            WRITE_FORM(DW_AT_const_value,    DW_FORM_data4)
            // WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_lexical_block = nextAbbrevCode++;
            WRITE_LEB(abbrev_lexical_block) // code
            WRITE_LEB(DW_TAG_lexical_block) // tag
            stream->write1(DW_CHILDREN_yes);

            if(REGISTER_SIZE == 4) {
                WRITE_FORM(DW_AT_low_pc,           DW_FORM_data4)
                WRITE_FORM(DW_AT_high_pc,          DW_FORM_data4)
            } else {
                WRITE_FORM(DW_AT_low_pc,           DW_FORM_addr)
                WRITE_FORM(DW_AT_high_pc,          DW_FORM_addr)
            }
            
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            // NOTE: DWARF-3 page 81 mentions DW_AT_bit_stride which can hold multiple symbol names for
            //  one type if I understood it correctly. This would be convenient for enum @bitfield.
            //  ACTUALLY, the debugger seems to display bit masked enums anyway but I guess DW_AT_bit_stride
            //  allows for masking a range of bits in the integer?
            
            WRITE_LEB(0) // zero terminate abbreviation section
        }
        
        stream = objectFile->getStreamFromSection(section_info);
        {
            int offset_section = stream->getWriteHead();

            CompilationUnitHeader* comp = nullptr;
            stream->write_late((void**)&comp, sizeof(CompilationUnitHeader));
            // comp->unit_length = don't know yet;
            comp->version = 3; // DWARF VERSION
            int reloc_abbrev_offset = (u64)&comp->debug_abbrev_offset - (u64)comp;
            comp->debug_abbrev_offset = 0; // 0 since we only have one compilation unit
            comp->address_size = REGISTER_SIZE;
            
            u8* ptr = nullptr;
            int written = 0;
            
            // debugging entries
            struct Reloc {
                u32 section_offset;
                u32 addend;
            };
            DynamicArray<Reloc> relocs{};
            relocs.reserve(debug->functions.size() * 2 + 4);

            WRITE_LEB(abbrev_compUnit) // abbrev code
            stream->write("BTB Compiler 0.2.0");
            // stream->write1(0); // language
            if(debug->files.size() == 0) {
                log::out << "debug->files is zero, can't correctly generate DWARF\n";
                return;
            }
            // std::string path = debug->files[0];
            std::string path = compiler->options->source_file;
            int dot_index = path.find_last_of(".");
            if (dot_index == -1) {
                path += ".btb";
            }
            // compiler->options->source_file
            std::string proj_dir = TrimLastFile(path);
            proj_dir = proj_dir.substr(0,proj_dir.length()-1);
            std::string file = TrimDir(path);
            stream->write(file.c_str()); // source file
            stream->write(proj_dir.c_str()); // project dir
            // stream->write("unknown.btb"); // source file
            // stream->write("project/src"); // project dir
            relocs.add({ stream->getWriteHead() - offset_section, 0 });
            if(REGISTER_SIZE == 4) {
                stream->write4(0); // start address of code
            } else {
                stream->write8(0); // start address of code
            }
            // Assert(false);
            relocs.add({ stream->getWriteHead() - offset_section, (u32)stream_text->getWriteHead() });
            if(REGISTER_SIZE == 4) {
                stream->write4(stream_text->getWriteHead()); // end address of text code
            } else {
                stream->write8(stream_text->getWriteHead()); // end address of text code
            }
            int reloc_statement_list = stream->getWriteHead() - offset_section;
            stream->write4(0); // statement list, address/pointer to reloc thing

            struct TypeRef {
                // each array element belongs to a pointer level
                u32 reference[4] = { 0 };
                int queuePosition = -1; // ALWAYS DO movable_pos + queuePosition. queuePosition will be out dated otherwise.
                int availablePointerLevel = 0;
            };
            DynamicArray<TypeId> queuedTypes{};
            DynamicArray<TypeRef> allTypes{};
            allTypes.resize(debug->ast->_typeInfos.size());

            int movable_pos = 0; // i am cheeky
            auto addType = [&](TypeId typeId) {
                auto& allType = allTypes[typeId.getId()];

                if (allType.queuePosition == -1) {
                    movable_pos++;
                    allType.queuePosition = queuedTypes.size() - movable_pos;
                    queuedTypes.add(typeId);
                } else {
                    auto& queuedType = queuedTypes[allType.queuePosition + movable_pos];
                    if(typeId.getPointerLevel() > queuedType.getPointerLevel())
                        queuedType.setPointerLevel(typeId.getPointerLevel()); // increase pointer level
                }
            };
            auto getTypeRef = [&](TypeId typeId) {
                auto& allType = allTypes[typeId.getId()];
                return allType.reference[typeId.getPointerLevel()];
            };
            
             for(int i=0;i<debug->global_variables.size();i++) {
                auto& var = *debug->global_variables[i];
                addType(var.typeId);
             }
            // TODO: Holy snap the polymorphism will actually end me.
            //  How do we make this work with polymorphism? Maybe it does work?
            for(int i=0;i<debug->functions.size();i++) {
                auto fun = debug->functions[i];
                for(int vi=0;vi<fun->localVariables.size();vi++) {
                    auto& var = fun->localVariables[vi];
                    addType(var.typeId);
                }
                
                if(!fun->funcImpl)
                    continue;

                // TODO: Add return values, types

                for(int pi=0;pi<fun->funcImpl->signature.argumentTypes.size();pi++){
                    auto& arg_impl = fun->funcImpl->signature.argumentTypes[pi];
                    addType(arg_impl.typeId);
                }
            }
            
            // #define DEBUG_VAL32 0x5f5f5f5f
            #define DEBUG_VAL32 0x1

            AST* ast = debug->ast;
            struct LateTypeRef {
                u32 section_offset; // offset to u32, ref4 form
                TypeId typeId;
            };
            DynamicArray<LateTypeRef> lateTypeRefs{};
            log::out.enableConsole(false);
            while(queuedTypes.size() > 0) {
                // We process types first in the queue and add new types at the back.
                // This creates a round-going process.
                TypeId queuedType = queuedTypes[0];
                auto typeInfo = ast->getTypeInfo(queuedType.baseType());
                auto& allType = allTypes[queuedType.getId()];

                allType.queuePosition = -1;
                queuedTypes.removeAt(0);
                movable_pos--;

                log::out << "Proc queued type "<<ast->typeToString(queuedType)<<"["<<queuedType.getId()<<"] ("<<queuedTypes.size()<<" left)\n";
                if(allType.reference[0] == 0) { // don't write base reference if it already exists
                    if(typeInfo->structImpl) {
                        Assert(typeInfo->astStruct);
                        // log::out << " struct\n";

                        // struct type
                        allType.reference[0] = stream->getWriteHead() - offset_section;
                        WRITE_LEB(abbrev_struct)

                        stream->write(typeInfo->name.c_str(), typeInfo->name.length() + 1);
                        // stream->write(typeInfo->name.ptr, typeInfo->name.len + 1);
                        stream->write2(typeInfo->getSize());

                        u32* sibling_ref4 = nullptr;
                        stream->write_late((void**)&sibling_ref4, sizeof(u32));
                        
                        //  {
                        // WRITE_LEB(abbrev_struct_member)
                        // std::string tmp ="hahaonijk";
                        // stream->write(tmp.c_str(), tmp.length());
                        // // stream->write(memAst.name.ptr, memAst.name.len);
                        // stream->write1('\0');
                        // int typeref = getTypeRef(TYPE_UINT32);
                        // if(typeref == 0) {
                        //     addType(TYPE_UINT32);
                        //     // log::out << "Late "<<ast->typeToString(memImpl.typeId)<<" at "<< (stream->getWriteHead() - offset_section)<<"\n";
                        //     lateTypeRefs.add({stream->getWriteHead() - offset_section, TYPE_UINT32 });
                        //     stream->write4(DEBUG_VAL32); // not known yet
                        // } else {
                        //     stream->write4(typeref); // ref4
                        // }
                        // stream->write2(12); // data location
                        // }
                        
                        Assert(typeInfo->structImpl->members.size() == typeInfo->astStruct->members.size());
                        for (int mi=0;mi<typeInfo->structImpl->members.size();mi++) {
                            auto& memImpl = typeInfo->structImpl->members[mi];
                            auto& memAst = typeInfo->astStruct->members[mi];

                            // NOTE: If two members are named the same then the debugger won't
                            //   show the members of the type.

                            WRITE_LEB(abbrev_struct_member)
                            stream->write(memAst.name.c_str(), memAst.name.length());
                            // stream->write(memAst.name.ptr, memAst.name.len);
                            stream->write1('\0');
                            int typeref = getTypeRef(memImpl.typeId);
                            if(typeref == 0) {
                                addType(memImpl.typeId);
                                // log::out << "Late "<<ast->typeToString(memImpl.typeId)<<" at "<< (stream->getWriteHead() - offset_section)<<"\n";
                                lateTypeRefs.add({stream->getWriteHead() - offset_section, memImpl.typeId });
                                stream->write4(DEBUG_VAL32); // not known yet
                            } else {
                                stream->write4(typeref); // ref4
                            }
                            Assert(memImpl.offset < 0xFFFF);
                            stream->write2(memImpl.offset); // data location
                        }
                       
                        WRITE_LEB(0); // end of members in structure

                        *sibling_ref4 = stream->getWriteHead() - offset_section;
                    } else if(typeInfo->astEnum) {
                        log::out << " enum\n";

                        allType.reference[0] = stream->getWriteHead() - offset_section;
                        WRITE_LEB(abbrev_enum)
                        
                        stream->write(typeInfo->name.c_str(), typeInfo->name.length() + 1);
                        // stream->write(typeInfo->name.ptr, typeInfo->name.len + 1);
                        Assert(typeInfo->getSize() < 256); // size should fit in one byte
                        stream->write1(typeInfo->getSize()); // size
                        
                        int typeRef = getTypeRef(typeInfo->astEnum->colonType);
                        if (typeRef == 0) {
                            addType(typeInfo->astEnum->colonType);
                            lateTypeRefs.add({stream->getWriteHead() - offset_section, typeInfo->astEnum->colonType });
                            stream->write4(DEBUG_VAL32); // not known yet
                        } else {
                            stream->write4(typeRef);
                        }
                        
                        u32* sibling_ref4 = nullptr;
                        stream->write_late((void**)&sibling_ref4, sizeof(u32));

                        for (int mi=0;mi<typeInfo->astEnum->members.size();mi++) {
                            auto& mem = typeInfo->astEnum->members[mi];
                            
                            if (typeInfo->getSize() > 4) {
                                WRITE_LEB(abbrev_enum_member_64)
                                // stream->write(mem.name.ptr, mem.name.len);
                                stream->write(mem.name.c_str(), mem.name.length());
                                stream->write1('\0');
                                
                                stream->write8(mem.enumValue);
                            } else {
                                WRITE_LEB(abbrev_enum_member_32)
                                stream->write(mem.name.c_str(), mem.name.length());
                                // stream->write(mem.name.ptr, mem.name.len);
                                stream->write1('\0');
                                
                                stream->write4(mem.enumValue);   
                            }
                        }
                        
                        WRITE_LEB(0); // end of members in enum
                        
                        *sibling_ref4 = stream->getWriteHead() - offset_section;
                    } else if(typeInfo->funcType) {
                        // TODO: Implement function references/pointers
                        log::out << " func ref (not implemented)\n";
                        allType.reference[0] = stream->getWriteHead() - offset_section;
                        WRITE_LEB(abbrev_base_type)

                        stream->write(typeInfo->name.c_str(), typeInfo->name.length() + 1);
                        stream->write1(REGISTER_SIZE); // size
                        stream->write1(DW_ATE_unsigned);
                    // } else if (queuedType == AST_FUNC_REFERENCE) {
                    //     // TODO: Implement function references/pointers
                    //     log::out << " func ref (not implemented)\n";
                    //     allType.reference[0] = stream->getWriteHead() - offset_section;
                    //     WRITE_LEB(abbrev_base_type)

                    //     stream->write(typeInfo->name.c_str(), typeInfo->name.length() + 1);
                    //     stream->write1(8); // size
                    //     stream->write1(DW_ATE_unsigned);
                    } else {
                        Assert(queuedType.getId() < TYPE_PRIMITIVE_COUNT);
                        // other type

                        log::out << " prim\n";
                        allType.reference[0] = stream->getWriteHead() - offset_section;
                        WRITE_LEB(abbrev_base_type)
                        
                        // stream->write(typeInfo->name.ptr, typeInfo->name.len + 1);
                        stream->write(typeInfo->name.c_str(), typeInfo->name.length() + 1);
                        stream->write1(typeInfo->getSize()); // size

                        switch(typeInfo->id.getId()) { // encoding (1 byte)
                            case TYPE_VOID:
                                // Assert(false);
                                stream->write1(DW_ATE_unsigned);
                                break;
                            case TYPE_UINT8:
                            case TYPE_UINT16:
                            case TYPE_UINT32:
                            case TYPE_UINT64:
                                stream->write1(DW_ATE_unsigned);
                                break;
                            case TYPE_INT8:
                            case TYPE_INT16:
                            case TYPE_INT32:
                            case TYPE_INT64:
                                stream->write1(DW_ATE_signed);
                                break;
                            case TYPE_BOOL:
                                stream->write1(DW_ATE_boolean);
                                break;
                            case TYPE_CHAR:
                                stream->write1(DW_ATE_unsigned_char);
                                break;
                            case TYPE_FLOAT32:
                            case TYPE_FLOAT64:
                                stream->write1(DW_ATE_float);
                                break;
                            default: {
                                Assert(false);
                                break;
                            }
                        }
                    }
                }

                for(int j=allType.availablePointerLevel;j<queuedType.getPointerLevel();j++){
                    log::out << " plevel "<<j<<"\n";
                    // Note that pointer level = j + 1
                    allType.reference[j + 1] = stream->getWriteHead() - offset_section;
                    WRITE_LEB(abbrev_pointer_type)
                    
                    stream->write1(REGISTER_SIZE); // size, pointers are 8 in size
                    stream->write4(allType.reference[j]); // ref4, we refer to the previous pointer level (or struct/base type)
                }
            }
            log::out.enableConsole(true);

            for(int i=0;i<lateTypeRefs.size();i++) {
                auto& ref = lateTypeRefs[i];
                u32 typeref = allTypes[ref.typeId.getId()].reference[ref.typeId.getPointerLevel()];
                // log::out << "Late write "<<ast->typeToString(ref.typeId)<<" at "<<ref.section_offset<<" = "<<typeref<<"\n";
                stream->write_at<u32>(offset_section + ref.section_offset, typeref);
                
                // if(i <= 3)
                // stream->write_at<u32>(offset_section + ref.section_offset, 0x102);
            }
           
            // for(int i=0;i<debug->global_variables.size();i++) {
            //     auto& var = *debug->global_variables[i];
            //     WRITE_LEB(abbrev_global_var)
            //     stream->write(var.name.c_str());

            //     // TODO: Store line and column information in local variables in DebugInformation
            //     int var_file = var.file + 1;
            //     int var_line = var.line;
            //     int var_column = var.column;

            //     stream->write2(var_file); // file
            //     Assert(var_line < 0x10000); // make sure we don't overflow
            //     stream->write2(var_line); // line
            //     Assert(var_column < 0x10000); // make sure we don't overflow
            //     stream->write2(var_column); // column

            //     int typeref = allTypes[var.typeId.getId()].reference[var.typeId.getPointerLevel()];
            //     Assert(typeref != 0);
            //     stream->write4(typeref); // type reference

            //     u8* block_length = nullptr;
            //     stream->write1(5); // DW_AT_location, begins with block length
            //     stream->write1(DW_OP_addr); // operation, addr describes that we should use 4 bytes
            //     int offset = stream->getWriteHead();
            //     stream->write4(var.location);
                
            //     objectFile->addRelocation(section_info, ObjectFile::RELOCA_ADDR64, offset, objectFile->getSectionSymbol(section_data), var.location);
            // }
            // We need this because we added a 16-byte offset in .debug_frame.
            // I don't know why gcc generates DWARF that way but we do the same because I don't know how it works.
            // This offset makes things work and I can't be bothered to question it at the moment.
            // Another problem for future me.
            // - Emarioo, 2024-01-01 (Happy new year!)
            int RBP_CONSTANT_OFFSET = -16;
            if(compiler->options->target == TARGET_ARM) {
                RBP_CONSTANT_OFFSET = 0;
            }

            for(int i=0;i<debug->functions.size();i++) {
                auto fun = debug->functions[i];

                // NOTE: It's really important that the function has a name
                //   Local variables does not show up in debugger otherwise.
                Assert(fun->name.size() != 0);
                WRITE_LEB(abbrev_func) // abbrev code
                bool global_func = false;
                WRITE_LEB(global_func) // external or not
                stream->write(fun->name.c_str(), fun->name.length() + 1); // name

                // Assert(func.lines.size() > 0);

                int file_index = fun->fileIndex + 1;
                Assert(file_index < 0x10000); // make sure we don't overflow
                stream->write2((u16)(file_index)); // file

                int line = fun->declared_at_line;
                Assert(line < 0x10000); // make sure we don't overflow
                stream->write2((u16)(line)); // line
                stream->write2((u16)(1)); // column

                // TODO: We skip the return type for now. Can we have multiple?
                u32 proc_low = fun->asm_start;
                u32 proc_high = fun->asm_end;
                relocs.add({ stream->getWriteHead() - offset_section, proc_low });
                if(REGISTER_SIZE == 4) {
                    stream->write4(proc_low); // pc low
                } else {
                    stream->write8(proc_low); // pc low
                }
                relocs.add({ stream->getWriteHead() - offset_section, proc_high });
                if(REGISTER_SIZE == 4) {
                    stream->write4(proc_high); // pc high
                } else {
                    stream->write8(proc_high); // pc high
                }

                stream->write1((u8)(1)); // frame base, begins with block length
                stream->write1((u8)(DW_OP_call_frame_cfa)); // block content
                
                u32* sibling_ref4 = nullptr;
                stream->write_late((void**)&sibling_ref4, sizeof(u32));
                if(fun->funcImpl) {
                    Assert(fun->funcImpl->signature.argumentTypes.size() == fun->funcAst->arguments.size());
                    // log::out << "func " << fun->name<<"\n";
                    for(int pi=0;pi<fun->funcImpl->signature.argumentTypes.size();pi++){
                        auto& arg_ast = fun->funcAst->arguments[pi];
                        auto& arg_impl = fun->funcImpl->signature.argumentTypes[pi];
                        WRITE_LEB(abbrev_param)
                        stream->write(arg_ast.name.c_str(), arg_ast.name.length()); // arg_ast.name is a Token and not zero terminated
                        // stream->write(arg_ast.name.ptr, arg_ast.name.len); // arg_ast.name is a Token and not zero terminated
                        stream->write1(0); // we must zero terminate here

                        auto src = compiler->lexer.getTokenSource_unsafe(arg_ast.location);
                        Assert((u32)src->line < 0x10000); // make sure we don't overflow
                        Assert((u32)src->column < 0x10000); // make sure we don't overflow

                        stream->write2((file_index)); // file
                        stream->write2((src->line)); // line
                        stream->write2((src->column)); // column

                        int typeref = allTypes[arg_impl.typeId.getId()].reference[arg_impl.typeId.getPointerLevel()];
                        Assert(typeref != 0);
                        stream->write4(typeref); // type reference

                        // NOTE: I have a worry here. What if the argument wasn't placed on the stack but put inside a register?
                        //  And what if we never put the argument in the register on the stack and overwrote the argument value?
                        //  This doesn't happen now but what about the future?
                        u8* block_length = nullptr;
                        stream->write_late((void**)&block_length, 1); // DW_AT_location, begins with block length
                        int off_start = stream->getWriteHead();
                        stream->write1(DW_OP_fbreg); // operation, fbreg describes that we should use a register (rbp) with an offset to get the argument.

                        int arg_off = arg_impl.offset;
                        // log::out << "  arg " << arg_off<<"\n";
                        if(target == TARGET_ARM) {
                            // NOTE: THIS won't work if param is bigger than register size
                            arg_off = -fun->args_offset - arg_impl.offset - 2*REGISTER_SIZE;
                            // if(pi < 4) {
                            //     arg_off = -arg_impl.offset - REGISTER_SIZE;
                            // } else {
                            //     arg_off = FRAME_SIZE + arg_impl.offset - 4 * REGISTER_SIZE;
                            // }
                        } else if(fun->funcAst->callConvention == UNIXCALL) {
                            // TODO: Don't hardcode this. What about non-volatile registers, what if x64_gen changes and puts more stuff on stack?
                            if(fun->name == "main") {
                                if(pi == 0) {
                                    // I came up with the offsets by trial and error
                                    arg_off = RBP_CONSTANT_OFFSET;
                                } else {
                                    // I came up with the offsets by trial and error
                                    arg_off = arg_impl.offset - RBP_CONSTANT_OFFSET;
                                }
                            } else {
                                // this offset was calculated without trial and error
                                // -16 for non-volatile registers: rbx, r12
                                arg_off = -16-8-arg_impl.offset - RBP_CONSTANT_OFFSET;
                            }
                        }
                        WRITE_SLEB(arg_off)
                        *block_length = stream->getWriteHead() - off_start; // we write block length later since we don't know the size of the LEB128 integer
                    }
                }
                
                auto indent = [&](int n) {
                    for (int i=0;i<n;i++) log::out <<" ";
                };
                
                log::out.enableConsole(false);
                bool left_scope_once = false;
                
                struct Scope {
                    ScopeId id;
                    // bool skip = false;
                };
                DynamicArray<Scope> scopeStack{};
                if(fun->funcAst) {
                    scopeStack.add({fun->funcAst->scopeId});
                } else {
                    auto imp = compiler->getImport(fun->import_id);
                    scopeStack.add({imp->scopeId});

                    // NOTE: I tried to be cheeky with the code below but
                    //   that's not gonna work since variables may be in
                    //   two local scopes. Assuming all variables come from
                    //   that local scope is a terrible assumption. - Emarioo, 2024-05-19

                    // Previously we added ast->globalScopeId
                    // But with rewrite-0.2.1 we changed so that we have import
                    // scopes and only them can be in global scope
                    // Temporarily, we will find the highest used scope
                    // This only runs on the main function so it's not really
                    // going to slow things down.
                    // ScopeId highest = -1;
                    // for(int vi=0;vi<fun->localVariables.size();vi++) {
                    //     auto& var = fun->localVariables[vi];
                    //     if(highest == -1 || var.scopeId < highest) {
                    //         highest = var.scopeId;
                    //     }
                    // }
                    // scopeStack.add({highest});
                    // scopeStack.add(ast->globalScopeId);
                }
                int curLevel = 0;
                
                for(int vi=0;vi<fun->localVariables.size();vi++) {
                    auto& var = fun->localVariables[vi];
                    // ScopeId curScope = scopeStack.last();
                    
                    // make sure we have the right lexical scope for the variable
                    while (true) {
                        // If the variable is the same scope as the one one the stack then we good!
                        if(var.scopeId == scopeStack.last().id) {
                            break;
                        }
                        
                        // Calculate some information
                        DynamicArray<ScopeId> scopes_to_generate{};
                        int found_on_stack = -1; // the last scope on the stack that is a close or far parent to the variable
                        ScopeId next_var_scope = var.scopeId;
                        WHILE_TRUE {
                            for(int i=scopeStack.size()-1;i>=0;i--) {
                                ScopeInfo* scope = ast->getScope(scopeStack[i].id);
                                if(scope->id == next_var_scope) {
                                    // found the scope we needed
                                    found_on_stack = i;
                                    break;
                                }
                            }
                            if(found_on_stack == -1) {
                                scopes_to_generate.add(next_var_scope);
                                ScopeInfo* var_scope = ast->getScope(next_var_scope);
                                next_var_scope = var_scope->parent;
                                continue;   
                            }
                            break;
                        }
                        // pop scopes until the variables distant (or close) parent
                        while(scopeStack.size()-1 != found_on_stack) {
                            scopeStack.pop();
                            WRITE_LEB(0) // end lexical scope
                            
                            curLevel--;
                            indent(curLevel);
                            log::out << "end scope "<<(curLevel+1)<<"\n";
                        }
                        // generate the parent scopes for the variable
                        for(int i=scopes_to_generate.size()-1;i>=0;i--) {
                            ScopeInfo* scope = ast->getScope(scopes_to_generate[i]);
                            scopeStack.add({scopes_to_generate[i]});
                            u32 proc_low = fun->asm_start + scope->asm_start;
                            u32 proc_high = fun->asm_start + scope->asm_end;
                            
                            WRITE_LEB(abbrev_lexical_block)
                            relocs.add({ stream->getWriteHead() - offset_section, proc_low });
                            if(REGISTER_SIZE == 4) {
                                stream->write4(proc_low); // pc low
                            } else {
                                stream->write8(proc_low); // pc low
                            }
                            relocs.add({ stream->getWriteHead() - offset_section, proc_high });
                            if(REGISTER_SIZE == 4) {
                                stream->write4(proc_high); // pc high
                            } else {
                                stream->write8(proc_high); // pc high
                            }
                            indent(curLevel);
                            curLevel++;
                            log::out << "scope "<<curLevel<<"\n";
                        }
                    }
                    
                    indent(curLevel);
                    log::out << "var "<<var.name<<" "<<var.scopeId<<"\n";
                    
                    if(var.is_global && section_data == 0) {
                        continue;
                    }

                    WRITE_LEB(abbrev_var)
                    stream->write(var.name.c_str());

                    // TODO: Store line and column information in local variables in DebugInformation
                    int var_line = 1;
                    int var_column = 1;

                    stream->write2((file_index)); // file
                    Assert(var_line < 0x10000); // make sure we don't overflow
                    stream->write2(var_line); // line
                    Assert(var_column < 0x10000); // make sure we don't overflow
                    stream->write2(var_column); // column

                    int typeref = allTypes[var.typeId.getId()].reference[var.typeId.getPointerLevel()];
                    Assert(typeref != 0);
                    stream->write4(typeref); // type reference

                    u8* block_length = nullptr;
                    stream->write_late((void**)&block_length, 1); // DW_AT_location, begins with block length
                    int off_start = stream->getWriteHead();
                    if(var.is_global) {
                        stream->write1(DW_OP_addr); // operation, addr describes that we should use 4 bytes
                        int offset = stream->getWriteHead();
                        // stream->write4(var.frameOffset);
                        if(REGISTER_SIZE == 8) {
                            stream->write8(var.frameOffset);
                        } else {
                            stream->write4(var.frameOffset);
                            // stream->write8(0);
                        }
                        objectFile->addRelocation(section_info, ObjectFile::RELOCA_ADDR64, offset, objectFile->getSectionSymbol(section_data), var.frameOffset);
                    } else {
                        stream->write1(DW_OP_fbreg); // operation, fbreg describes that we should use a register (rbp) with an offset to get the argument.
                        
                        int extra_off = -fun->offset_from_bp_to_locals; // extra offset due to non-volatile registers, i think return values with betcall is accounted for in var.frameOffset
                        if(target == TARGET_ARM) {
                            // DWARF adds some extra offset so we do this to revert it?
                            extra_off -= REGISTER_SIZE;
                        }
                        WRITE_SLEB(var.frameOffset + extra_off + RBP_CONSTANT_OFFSET)
                    }
                    *block_length = stream->getWriteHead() - off_start; // we write block length later since we don't know the size of the LEB128 integer
                }
                while(curLevel>0) {
                    curLevel--;
                    indent(curLevel);
                    log::out << "end scope "<<(curLevel+1)<<"\n";
                    WRITE_LEB(0) // end lexical scope
                }
                log::out.enableConsole(true);

                WRITE_LEB(0) // end subprogram entry

                *sibling_ref4 = stream->getWriteHead() - offset_section;
            }

            WRITE_LEB(0) // end compile unit

            comp->unit_length = stream->getWriteHead() - offset_section - sizeof(CompilationUnitHeader::unit_length);

            // ################
            //   RELOCATIONS
            // ################
            
            objectFile->addRelocation(section_info, ObjectFile::RELOCA_SECREL, reloc_abbrev_offset, objectFile->getSectionSymbol(section_abbrev), 0);
            objectFile->addRelocation(section_info, ObjectFile::RELOCA_SECREL, reloc_statement_list, objectFile->getSectionSymbol(section_line), 0);
            for(int i=0;i<relocs.size();i++) {
                auto& rel = relocs[i];
                objectFile->addRelocation(section_info, ObjectFile::RELOCA_ADDR64, rel.section_offset, objectFile->getSectionSymbol(section_text), rel.addend);
            }
        }
        stream = objectFile->getStreamFromSection(section_line);
        {
            int offset_section = stream->getWriteHead();
            
            u8* ptr = nullptr;
            int written = 0;
            
            LineNumberProgramHeader* header = nullptr;
            stream->write_late((void**)&header, sizeof(LineNumberProgramHeader));
            // comp->unit_length = don't know yet;
            header->version = 3; // Specific version for debug_line (not the DWARF version)
            // header->header_length = don't know yet;
            header->minimum_instruction_length = 1;
            header->default_is_stmt = 1;
            header->line_base = -5; // affects special opcodes
            header->line_range = 10; // affects special opcodes
            header->opcode_base = 1; // start at one, we increase it some more below
            
            // standard opcode lengths
            u8 opcode_lengths[]{ 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1 };
            for(int i=0;i<sizeof(opcode_lengths);i++){
                header->opcode_base++; // increase when adding standard_opcode_lengths
                stream->write1(opcode_lengths[i]);
            }
            
            DynamicArray<i32> file_dir_indices{};
            file_dir_indices.resize(debug->files.size());
            DynamicArray<std::string> dir_entries{}; // unique entries
            for(int i=0;i<debug->files.size();i++) {
                auto& file = debug->files[i];

                std::string dir = TrimLastFile(file); // skip / at end
                if (dir.size() == 0 || dir == "/") {
                    file_dir_indices[i] = 0;
                    continue;
                }

                dir = dir.substr(0,dir.length()-1);
                int dir_index = -1;
                for(int j=0;j<dir_entries.size();j++) {
                    if(dir_entries[j] == dir) {
                        dir_index = j;
                        break;
                    }
                }
                if(dir_index == -1) {
                    dir_entries.add(dir);
                    file_dir_indices[i] = dir_entries.size()-1;
                } else {
                    file_dir_indices[i] = dir_index;
                }
            }

            // include directories
            for(int i=0;i<dir_entries.size();i++) {
                stream->write(dir_entries[i].c_str());
            }
            stream->write1(0); // zero to mark end of directories
            
            // file names
            for(int i=0;i<debug->files.size();i++) {
                auto& file = debug->files[i];
                std::string no_dir = TrimDir(file);
                // Assert(dir_entries[file_dir_indices[i]]+"/"+no_dir == file);
                stream->write(no_dir.c_str());
                WRITE_LEB(1 + file_dir_indices[i]) // index into include_directories starting from 1, index as 0 represents the current working directory
                WRITE_LEB(0) // file last modified
                WRITE_LEB(0) // file size
            }

            // Uncommented code for hardcoded files when debugging the compiler
            // stream->write("examples/dev.btb");
            // WRITE_LEB(0) // index into include_directories starting from 1, index as 0 represents the current working directory
            // WRITE_LEB(0) // file last modified
            // WRITE_LEB(0) // file size

            stream->write1(0); // terminate file entries
            
            // line number statements
            // Line numbers are computed by running a state machine with registers that
            // "create" a matrix of instructions and line numbers per row (roughly, theoretically?).
            
            // The statements begin from the first instruction in the text
            // section to the last instruction. The operations we can run modify
            // the registers in a way that represents the instruction to line conversion.

            // Our debug information stores lines in functions. These functions exist in an array.
            // For the state machine, we must find the function that appears first in the text section.
            // The function with the lowest address.
            // Then we find the next lowest function and so on.

            // TODO: Optimize ordering. Perhaps the debug information can ensure that
            //  the functions are ordered already or something.
            DynamicArray<bool> used_functions{};
            used_functions.resize(debug->functions.size());
            memset(used_functions.data(), 0, used_functions.size());

            DynamicArray<int> functions_in_order{}; // indices of the functions
            for(int i=0;i<debug->functions.size();i++) {
                u64 lowest_address = -1;
                int lowest_index = -1;
                for(int j=0;j<debug->functions.size();j++) {
                    auto fun = debug->functions[j];
                    if(fun->asm_start == fun->asm_end) {
                        Assert(false); // why would this happen?
                        continue;
                    }
                    if(used_functions[j])
                        continue;
                    if(fun->asm_start < lowest_address || lowest_address == -1) {
                        lowest_address = fun->asm_start;
                        lowest_index = j;
                    }
                }
                if(lowest_index == -1)
                    break; // no function left
                functions_in_order.add(lowest_index);
                used_functions[lowest_index] = true;
            }

            // The code won't generate any line number information if there are no functions.
            Assert(("bug in debug information, specify a main function as a fix",functions_in_order.size() > 0));
            header->header_length = (stream->getWriteHead() - offset_section) - (sizeof(LineNumberProgramHeader::header_length) + (u64)&header->header_length - (u64)header);

            // ####################
            //  Line number generation!
            // ########################

            /* Helpful notes when improving this code
                The line program and state machine should cover the whole
                program text. Otherwise you may end up with some spots in the debugger that says "Unknown source".
                That's annoying so make sure all instructions are covered. The last few instructions doesn't need to be covered
                by a line i believe, just make sure to advance the pc counter.
            */

            int reg_file = -1;
            int reg_address = 0;
            int reg_column = 1;
            int reg_line = 1;
            WRITE_LEB(DW_LNS_set_column)
                WRITE_LEB(1) // column one for now
                reg_column = 1;

            stream->write1(0); // extended opcode uses a zero at first
            stream->write1(1 + REGISTER_SIZE); // then count of bytes for the whole extended operation
            WRITE_LEB(DW_LNE_set_address) // then opcode and data
                int reloc_pc_addr = stream->getWriteHead() - offset_section;
                if(REGISTER_SIZE == 8) {
                    stream->write8(0);
                } else {
                    stream->write4(0);
                }
                reg_address = 0;


            // if(functions_in_order.size()>0) {
            //     auto fun = debug->functions[functions_in_order[0]];
            //     reg_file = fun->fileIndex + 1;
            //     WRITE_LEB(DW_LNS_set_file)
            //     WRITE_LEB(reg_file)
            // }

            // increment address and line
            auto add_row = [&](int code, int line){
                int dt_code = code - reg_address;
                int dt_line = line - reg_line;

                Assert(dt_code >= 0); // Make sure we always increment
                // Assert(dt_line >= 0); // DWARF allows decrementing but we need to extra stuff to handle it so we just don't allow it.

                u32 special_opcode = (dt_line - header->line_base) +
                        (header->line_range * dt_code) + header->opcode_base; 

                if(special_opcode <= 255 && dt_line >= header->line_base && dt_line < header->line_base + header->line_range) {
                    stream->write1(special_opcode);

                    // just making sure we encode the special opcode correctly
                    // by decoding it again.
                    int adjusted_opcode = special_opcode - header->opcode_base;
                    int incr_code = 
                        (adjusted_opcode / header->line_range) * header->minimum_instruction_length;

                    int incr_line = header->line_base + (adjusted_opcode % header->line_range);
                    Assert(dt_code == incr_code);
                    Assert(dt_line == incr_line);

                    reg_address += dt_code;
                    reg_line += dt_line;
                } else {
                    // must use standard opcode
                    if(dt_code != 0) {
                        reg_address += dt_code;
                        WRITE_LEB(DW_LNS_advance_pc)
                        WRITE_LEB(dt_code)
                    }
                    if(dt_line != 0) {
                        reg_line += dt_line;
                        WRITE_LEB(DW_LNS_advance_line)
                        WRITE_SLEB(dt_line)
                    }
                    // if(dt_code != 0 || dt_line != 0)
                    WRITE_LEB(DW_LNS_copy)
                }
            };

            // WRITE_LEB(DW_LNS_copy)
            for(int fi : functions_in_order) {
                auto& fun = debug->functions[fi];
                int file_index = fun->fileIndex + 1;
                if(reg_file != file_index) {
                    std::string file = debug->files[fun->fileIndex];
                    if (file == "<preload>") // hacky thing, what if we have other files that aren't actually files, virtual files with test cases perhaps?
                        continue;

                    reg_file = file_index;
                    WRITE_LEB(DW_LNS_set_file)
                    WRITE_LEB(reg_file)
                }

                // TODO: Set column
                // log::out << fun->name<<" "<<fun->asm_start << " " <<fun->declared_at_line<<"\n";
                add_row(fun->asm_start, fun->declared_at_line);
                
                add_row(fun->asm_start, fun->declared_at_line);

                int lastOffset = 0;
                int lastLine = 0;
                for(int i=0;i<fun->lines.size();i++) {
                    auto& line = fun->lines[i];
                    // Ensure that the lines are stored in ascending order
                    Assert(lastOffset <= line.asm_address);
                    // Assert(lastLine <= line.lineNumber);
                    lastOffset = line.asm_address;
                    lastLine = line.lineNumber;
                    // log::out << "line " << line.lineNumber << " " << (fun->asm_start + line.asm_address)<<"\n";
                    add_row(line.asm_address + fun->asm_start, line.lineNumber);
                }

                // add_row(fun->asm_end, reg_line); // reuse the line?

                // the debugger won't stop at the last instruction because the line
                // number is the same. I was hoping it would stop at the return statement
                // so that you could see the final result of all variables but we need
                // some stuff elsewhere to make that possible.
                // add_row(fun.funcEnd, fun.lines.last().lineNumber);
            }

            // Assert(false);
            int dt = stream_text->getWriteHead() - reg_address;
            // int dt = 0;
            if(dt) {
                WRITE_LEB(DW_LNS_advance_pc)
                reg_address += dt;
                WRITE_LEB(dt)
            }

            stream->write1(0); // extended opcode begins with zero
            stream->write1(1); // number of bytes for ext. opcode
            WRITE_LEB(DW_LNE_end_sequence) // actual opcode

            int section_size = stream->getWriteHead() - offset_section;
            header->unit_length = section_size - sizeof(LineNumberProgramHeader::unit_length);

            // ###############
            //   RELOCATIONS
            // ###############

            objectFile->addRelocation(section_line, ObjectFile::RELOCA_ADDR64, reloc_pc_addr, objectFile->getSectionSymbol(section_text), 0);
        }
        stream = objectFile->getStreamFromSection(section_aranges);
        {
            int offset_section = stream->getWriteHead();
            
            u8* ptr = nullptr;
            int written = 0;
            
            ArangesHeader* header = nullptr;
            stream->write_late((void**)&header, sizeof(ArangesHeader));
            // header->unit_length = don't know yet;
            header->version = 2; // Specific version for debug_aranges (not the DWARF version)
            // header->header_length = don't know yet;
            int reloc_debug_info_offset = (u64)&header->debug_info_offset - (u64)header;
            header->debug_info_offset = 0;
            header->address_size = REGISTER_SIZE;
            header->segment_size = 0;
            
            suc = stream->write_align(REGISTER_SIZE);
            CHECK
            
            int reloc_text_start = stream->getWriteHead() - offset_section;
            // if(REGISTER_SIZE == 8) {
            stream->write8(0);
            stream->write8(stream_text->getWriteHead());

            stream->write8(0); // null entry
            stream->write8(0);
            // } else {
            //     stream->write4(0);
            //     stream->write4(stream_text->getWriteHead());

            //     stream->write4(0); // null entry
            //     stream->write4(0);
            // }

            header->unit_length = (stream->getWriteHead() - offset_section) - sizeof(ArangesHeader::unit_length);
            
            // ###############
            //   RELOCATIONS
            // ###############
            objectFile->addRelocation(section_aranges, ObjectFile::RELOCA_SECREL, reloc_debug_info_offset, objectFile->getSectionSymbol(section_info), 0);
            objectFile->addRelocation(section_aranges, ObjectFile::RELOCA_ADDR64, reloc_text_start, objectFile->getSectionSymbol(section_text), 0);
        }
        stream = objectFile->getStreamFromSection(section_frame);
        {
            int offset_section = stream->getWriteHead();
            
            /* NOTES ABOUT .debug_frame
                State machine and matrix with columns of locations and whether registers are preserved and which register is the return address.
                We provide instructions which generates the matrix.
                Some instructions are encoding in the opcode, others use operands like u32, LEB128.
                The value of some operands are affected by a factor specified in Common Information Entry (CIE)
                Read the DWARF-3 specification (page 112) for which instructions use a factor.
            */

            u8* ptr = nullptr;
            int written = 0;

            enum DW_registers : u8 {
                // x86
                DW_rbp = 6,
                DW_rsp = 7,
                DW_rip = 16,
                
                // ARM
                DW_r11 = 11,
                DW_r13 = 13,
                DW_r14 = 14,
            };
            
            int code_factor = 1;
            {
                CommonInformationEntry* header = nullptr;
                stream->write_late((void**)&header, sizeof(CommonInformationEntry));
                // header->length = don't know yet;
                header->CIE_id = 0xFFFFFFFF; // marks that this is isn't a FDE but a CIE
                header->version = 3; // Specific version for debug_frame (not the DWARF version)
                
                stream->write1(0); // augmentation string, ZERO for now?
                
                if(compiler->options->target == TARGET_ARM) {
                    code_factor = 2;
                    WRITE_LEB(2) // code_alignment factor
                    WRITE_SLEB(-4) // data_alignment_factor
                    WRITE_LEB(14) // return address register, lr register

                    // initial instructions for the columns?
                    // Copied from what g++ generates
                    stream->write1(DW_CFA_def_cfa);
                    WRITE_LEB(DW_r13)
                    WRITE_LEB(0)
                } else {
                    // x86_64
                    code_factor = 1;
                    WRITE_LEB(1) // code_alignment factor
                    WRITE_SLEB(-8) // data_alignment_factor

                    WRITE_LEB(DW_rip) // return address register, ?

                    // initial instructions for the columns?
                    // Copied from what g++ generates
                    stream->write1(DW_CFA_def_cfa);
                    WRITE_LEB(DW_rsp)
                    WRITE_LEB(8)

                    stream->write1(DW_CFA_offset(DW_rip));
                    WRITE_LEB(1) // 1 becomes -8 (1 * data_alignment_factor)

                }
                // What I have understood from what GCC generates is that the whole CIE should start at an 8-byte alignment
                // and end at an 8-byte alignment. The same goes for the following FDEs
                stream->write_align(REGISTER_SIZE);
                header->length = (stream->getWriteHead() - offset_section) - sizeof(CommonInformationEntry::length);
            }
            struct Reloc {
                int symindex_section;
                u64 sectionOffset;
                u32 addend;
            };
            DynamicArray<Reloc> relocs{};
            int symindex_debug_frame = objectFile->getSectionSymbol(section_frame);
            int symindex_text = objectFile->getSectionSymbol(section_text);
            for (int fi=0;fi<debug->functions.size();fi++) {
                auto& fun = debug->functions[fi];
                Assert(fun->asm_end != 0);

                Assert(stream->getWriteHead() % REGISTER_SIZE == 0);
                int offset_fde_start = stream->getWriteHead();

                int fd_size = REGISTER_SIZE == 8 ? FrameDescriptionEntry::SIZE64 : FrameDescriptionEntry::SIZE32;

                FrameDescriptionEntry* header = nullptr;
                stream->write_late((void**)&header, fd_size);
                header->length = 0; // don't know yet
                
                header->CIE_pointer = 0; // relocated later
                relocs.add({symindex_debug_frame, offset_fde_start - offset_section  + (u64)&header->CIE_pointer - (u64)header });
                
                if(REGISTER_SIZE == 8) {
                    header->initial_location = fun->asm_start; // relocated later
                    
                    relocs.add({symindex_text, offset_fde_start - offset_section  + (u64)&header->initial_location - (u64)header, fun->asm_start });
                        
                    header->address_range = fun->asm_end - fun->asm_start;
                } else {
                    // we set to zero because relocation will use an addend
                    // this only works with ELF and R_ARM_ABS32.
                    header->initial_location32 = fun->asm_start; // relocated later
                    
                    relocs.add({symindex_text, offset_fde_start - offset_section  + (u64)&header->initial_location32 - (u64)header, fun->asm_start });
                        
                    header->address_range32 = (fun->asm_end - fun->asm_start);
                }

                if(compiler->options->target == TARGET_ARM) {
                    // instructions based on what arm-none-eabi-gcc generates with -g flag
                    stream->write1(DW_CFA_advance_loc4);
                    stream->write4(4/code_factor);

                    stream->write1(DW_CFA_def_cfa_offset);
                    WRITE_LEB(4)

                    stream->write1(DW_CFA_offset(DW_r11));
                    WRITE_LEB(1) // * data_alignment_factor

                    stream->write1(DW_CFA_advance_loc4);
                    stream->write4(4/code_factor);

                    stream->write1(DW_CFA_def_cfa_register);
                    WRITE_LEB(DW_r11)

                    stream->write1(DW_CFA_advance_loc4);
                    stream->write4((fun->asm_end - fun->asm_start - 8 - 8)/code_factor);
                    // NOTE: Above is a relative hop to almost the end of the function
                    //   -8 because push {fp, lr} and mov fp, sp
                    //   -8 because pop {fp, lr} and bx lr

                    stream->write1(DW_CFA_def_cfa_register);
                    WRITE_LEB(DW_r13)
                    
                    stream->write1(DW_CFA_advance_loc4);
                    stream->write4(4/code_factor);

                    stream->write1(DW_CFA_restore(DW_r11));
                    
                    stream->write1(DW_CFA_def_cfa_offset);
                    WRITE_LEB(0)
                } else {
                    // x86_64
                    // instructions, based on what g++ generates and a little from DWARF specification
                    stream->write1(DW_CFA_advance_loc4);
                    stream->write4(2); // move past push rbp (2 bytes with the general push instruction we used)

                    stream->write1(DW_CFA_def_cfa_offset);
                    WRITE_LEB(16)

                    stream->write1(DW_CFA_offset(DW_rbp));
                    WRITE_LEB(2) // * data_alignment_factor

                    stream->write1(DW_CFA_advance_loc4);
                    stream->write4(3);

                    stream->write1(DW_CFA_def_cfa_register);
                    WRITE_LEB(DW_rbp)

                    stream->write1(DW_CFA_advance_loc4);
                    stream->write4(fun->asm_end - fun->asm_start - 5 - 1);
                    // NOTE: Above is a relative hop to almost the end of the function
                    //   -5 because push rbp, mov rbp, rsp in the beginning of the function
                    //   -1 because we asm_end is exclusive

                    stream->write1(DW_CFA_restore(DW_rbp));

                    stream->write1(DW_CFA_def_cfa);
                    WRITE_LEB(DW_rsp)
                    WRITE_LEB(8)
                }
                
                // potential padding
                stream->write_align(REGISTER_SIZE);
                header->length = (stream->getWriteHead() - offset_fde_start) - sizeof(FrameDescriptionEntry::length);
            }

            // ################
            //   RELOCATIONS
            // ################

            for(int i=0;i<relocs.size();i++) {
                auto& rel = relocs[i];

                if(rel.symindex_section == symindex_text) {
                    objectFile->addRelocation(section_frame, ObjectFile::RELOCA_ADDR64, rel.sectionOffset, rel.symindex_section, rel.addend);
                    // objectFile->addRelocation(section_frame, ObjectFile::RELOC_PC32, rel.sectionOffset + 4, rel.symindex_section);
                    // objectFile->addRelocation(section_frame, ObjectFile::RELOC_PC32, rel.sectionOffset, rel.symindex_section);
                } else {
                    objectFile->addRelocation(section_frame, ObjectFile::RELOCA_SECREL, rel.sectionOffset, rel.symindex_section, rel.addend);
                }
            }
        }
    }

    int ULEB128_encode(u8* buffer, u32 buffer_space, u64 value) {
        // https://en.wikipedia.org/wiki/LEB128
        int i = 0;
        do {
            u8 byte = value & 0x7F;
            value >>= 7;
            if(value != 0)
                byte |= 0x80;
            
            buffer[i] = byte;
            i++;
        } while(value != 0);
        return i;
    }
    u64 ULEB128_decode(u8* buffer, u32 buffer_space) {
        // https://en.wikipedia.org/wiki/LEB128
        u64 result = 0;
        u8 shift = 0;
        int i = 0;
        while(true) {
            u8 byte = buffer[i];
            result |= ((byte&0x7F) << shift);
            if ((byte & 0x80) == 0)
                break;
            shift += 7;
            i++;
        }
        return result;
    }
    int SLEB128_encode(u8* buffer, u32 buffer_space, i64 value) {
        // https://en.wikipedia.org/wiki/LEB128
        bool more = 1;
        bool negative = (value < 0);

        /* the size in bits of the variable value, e.g., 64 if value's type is int64_t */
        int size = 64;
        int i = 0;
        while (more) {
            u8 byte = value & 0x7F; // low-order 7 bits of value
            value >>= 7;
            /* the following is only necessary if the implementation of >>= uses a
                logical shift rather than an arithmetic shift for a signed left operand */
            if (negative)
                value |= (~(i64)0 << (size - 7)); /* sign extend */

            /* sign bit of byte is second high-order bit (0x40) */
            if ((value == 0 && 0 == (0x40 & byte)) || (value == -1 && (0x40 & byte)))
                more = 0;
            else {
                byte |= 0x80; // set high-order bit of byte;
            }
            buffer[i] = byte;
            i++;
        }
        return i;
    }
    i64 SLEB128_decode(u8* buffer, u32 buffer_space) {
        // https://en.wikipedia.org/wiki/LEB128
        i64 result = 0;
        int shift = 0;

        /* the size in bits of the result variable, e.g., 64 if result's type is int64_t */
        int size = 64;
        int i = 0;
        u8 byte = 0;
        do {
            byte = buffer[i]; //next byte in input
            i++;
            result |= ((byte & 0x7f) << shift);
            shift += 7;
        } while ((byte & 0x80));

        /* sign bit of byte is second high-order bit (0x40) */
        if ((shift < size) && (byte & 0x40))
            /* sign extend */
            result |= (~0 << shift);
        return result;
    }
    void LEB128_test() {
        using namespace engone;
        u8 buffer[32];
        int errors = 0;
        int written = 0;
        u64 fin = 0;
        i64 fin_s = 0;
        
        // test isn't very good. it just encodes and decodes which doesn't mean they encode and decode properly.
        // encode and decode can both be wrong and still produce the correct result since both of them are wrong.
        auto TEST = [&](u64 X) {
            written = ULEB128_encode(buffer, sizeof(buffer), X);
            fin = ULEB128_decode(buffer, sizeof(buffer));
            if(X != fin) { 
                errors++;
                engone::log::out << engone::log::RED << "FAILED: "<< X << " -> "<<fin<<"\n";
            }
            // log::out << X <<" -> "<<fin<<"\n"; 
            // for(int i=0;i<written;i++) {
            //     log::out << " "<<i<<": "<<buffer[i]<<"\n";
            // }
        };
        auto TEST_S = [&](i64 X)  {
            written = SLEB128_encode(buffer, sizeof(buffer), X);
            fin_s = SLEB128_decode(buffer, sizeof(buffer));
            if(X != fin_s) { 
                errors++;
                engone::log::out << engone::log::RED << "FAILED: "<< X << " -> "<<fin_s<<"\n";
            }
            // log::out << X <<" -> "<<fin_s<<"\n";
            // for(int i=0;i<written;i++) {
            //     // log::out << " "<<i<<": "<<buffer[i]<<"\n";
            //     log::out << " "<<i<<": 0x"<<NumberToHex(buffer[i])<<"\n";
            // }
        };
        
        TEST(2);
        TEST(127);
        TEST(128);
        TEST(129);
        TEST(130);
        TEST(12857);

        TEST_S(2);
        TEST_S(127);
        TEST_S(-123456);
        TEST_S(-129);
        TEST_S(-55);
        TEST_S(12857);
        TEST_S(-12857);
            
        if(errors == 0){
            engone::log::out << engone::log::GREEN << "LEB128 tests succeeded\n";
        }
    }
}