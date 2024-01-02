#include "BetBat/DWARF.h"

namespace dwarf {
    void ProvideSections(ObjectFile* objectFile, Program_x64* program) {
        
    }
    void ProvideSections(DWARFInfo* info, DWARF_ObjectFileType objType) {
        using namespace engone;
        using namespace coff;
        using namespace elf;
        Assert(info->stream && info->debug);
        Assert(info->coff_header || info->elf_header);
        
        bool suc = false;
        #define CHECK Assert(suc);

        switch(objType) {
        case DWARF_OBJ_COFF:{
            {
                info->number_debug_info = ++info->coff_header->NumberOfSections;
                Section_Header* section = nullptr;
                suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
                CHECK
                info->coff_section_debug_info = section;

                info->stringTable->add(".debug_info");
                snprintf(section->Name, 8, "/%u", *info->stringTableOffset);
                *info->stringTableOffset += info->stringTable->last().length() + 1;
                section->NumberOfLineNumbers = 0;
                section->PointerToLineNumbers = 0;
                section->NumberOfRelocations = 0;
                section->PointerToRelocations = 0;
                section->VirtualAddress = 0;
                section->VirtualSize = 0;
                section->SizeOfRawData = 0;
                section->PointerToRawData = 0;
                section->Characteristics = (Section_Flags)(
                    IMAGE_SCN_CNT_INITIALIZED_DATA |
                    IMAGE_SCN_MEM_DISCARDABLE |
                    IMAGE_SCN_ALIGN_1BYTES |
                    IMAGE_SCN_MEM_READ);
            }
            {
                info->number_debug_abbrev = ++info->coff_header->NumberOfSections;
                Section_Header* section = nullptr;
                suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
                CHECK
            
                info->coff_section_debug_abbrev = section;
                info->stringTable->add(".debug_abbrev");
                snprintf(section->Name, 8, "/%u", *info->stringTableOffset);
                *info->stringTableOffset += info->stringTable->last().length() + 1;
                section->NumberOfLineNumbers = 0;
                section->PointerToLineNumbers = 0;
                section->NumberOfRelocations = 0;
                section->PointerToRelocations = 0;
                section->VirtualAddress = 0;
                section->VirtualSize = 0;
                section->SizeOfRawData = 0;
                section->PointerToRawData = 0;
                section->Characteristics = (Section_Flags)(
                    IMAGE_SCN_CNT_INITIALIZED_DATA |
                    IMAGE_SCN_MEM_DISCARDABLE |
                    IMAGE_SCN_ALIGN_1BYTES |
                    IMAGE_SCN_MEM_READ);
            }
            {
                info->number_debug_line = ++info->coff_header->NumberOfSections;
                Section_Header* section = nullptr;
                suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
                CHECK
            
                info->coff_section_debug_line = section;
                info->stringTable->add(".debug_line");
                snprintf(section->Name, 8, "/%u", *info->stringTableOffset);
                *info->stringTableOffset += info->stringTable->last().length() + 1;
                section->NumberOfLineNumbers = 0;
                section->PointerToLineNumbers = 0;
                section->NumberOfRelocations = 0;
                section->PointerToRelocations = 0;
                section->VirtualAddress = 0;
                section->VirtualSize = 0;
                section->SizeOfRawData = 0;
                section->PointerToRawData = 0;
                section->Characteristics = (Section_Flags)(
                    IMAGE_SCN_CNT_INITIALIZED_DATA |
                    IMAGE_SCN_MEM_DISCARDABLE |
                    IMAGE_SCN_ALIGN_1BYTES |
                    IMAGE_SCN_MEM_READ);
            }
            {
                info->number_debug_aranges = ++info->coff_header->NumberOfSections;
                Section_Header* section = nullptr;
                suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
                CHECK
            
                info->coff_section_debug_aranges = section;
                info->stringTable->add(".debug_aranges");
                snprintf(section->Name, 8, "/%u", *info->stringTableOffset);
                *info->stringTableOffset += info->stringTable->last().length() + 1;
                section->NumberOfLineNumbers = 0;
                section->PointerToLineNumbers = 0;
                section->NumberOfRelocations = 0;
                section->PointerToRelocations = 0;
                section->VirtualAddress = 0;
                section->VirtualSize = 0;
                section->SizeOfRawData = 0;
                section->PointerToRawData = 0;
                section->Characteristics = (Section_Flags)(
                    IMAGE_SCN_CNT_INITIALIZED_DATA |
                    IMAGE_SCN_MEM_DISCARDABLE |
                    IMAGE_SCN_ALIGN_1BYTES |
                    IMAGE_SCN_MEM_READ);
            }
            {
                info->number_debug_frame = ++info->coff_header->NumberOfSections;
                Section_Header* section = nullptr;
                suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
                CHECK
            
                info->coff_section_debug_frame = section;
                info->stringTable->add(".debug_frame");
                snprintf(section->Name, 8, "/%u", *info->stringTableOffset);
                *info->stringTableOffset += info->stringTable->last().length() + 1;
                section->NumberOfLineNumbers = 0;
                section->PointerToLineNumbers = 0;
                section->NumberOfRelocations = 0;
                section->PointerToRelocations = 0;
                section->VirtualAddress = 0;
                section->VirtualSize = 0;
                section->SizeOfRawData = 0;
                section->PointerToRawData = 0;
                section->Characteristics = (Section_Flags)(
                    IMAGE_SCN_CNT_INITIALIZED_DATA |
                    IMAGE_SCN_MEM_DISCARDABLE |
                    IMAGE_SCN_ALIGN_1BYTES |
                    IMAGE_SCN_MEM_READ);
            }
            break;
        }
        case DWARF_OBJ_ELF: {
            {
                info->number_debug_info = ++info->elf_header->e_shnum;
                Elf64_Shdr* section = nullptr;
                suc = info->stream->write_late((void**)&section, sizeof(*section));
                CHECK
                memset(section, 0, sizeof(*section));
                info->elf_section_debug_info = section;

                section->sh_name = 0; // set in ELF.cpps
                section->sh_type = SHT_PROGBITS;
                section->sh_flags = 0; // no flags?
                section->sh_addr = 0; // always zero for object files
                section->sh_offset = 0; // place where data lies, LATER
                section->sh_size = 0; // size of data, LATER
                section->sh_link = 0; // not used
                section->sh_info = 0; // not used
                section->sh_addralign = 1;
                section->sh_entsize = 0;
            }
            {
                info->number_debug_abbrev = ++info->elf_header->e_shnum;
                Elf64_Shdr* section = nullptr;
                suc = info->stream->write_late((void**)&section, sizeof(*section));
                CHECK
                memset(section, 0, sizeof(*section));
                info->elf_section_debug_abbrev = section;

                section->sh_name = 0; // set in ELF.cpps
                section->sh_type = SHT_PROGBITS;
                section->sh_flags = 0; // no flags?
                section->sh_addr = 0; // always zero for object files
                section->sh_offset = 0; // place where data lies, LATER
                section->sh_size = 0; // size of data, LATER
                section->sh_link = 0; // not used
                section->sh_info = 0; // not used
                section->sh_addralign = 1;
                section->sh_entsize = 0;
            }
            {
                info->number_debug_line = ++info->elf_header->e_shnum;
                Elf64_Shdr* section = nullptr;
                suc = info->stream->write_late((void**)&section, sizeof(*section));
                CHECK
                memset(section, 0, sizeof(*section));
                info->elf_section_debug_line = section;

                section->sh_name = 0; // set in ELF.cpps
                section->sh_type = SHT_PROGBITS;
                section->sh_flags = 0; // no flags?
                section->sh_addr = 0; // always zero for object files
                section->sh_offset = 0; // place where data lies, LATER
                section->sh_size = 0; // size of data, LATER
                section->sh_link = 0; // not used
                section->sh_info = 0; // not used
                section->sh_addralign = 1;
                section->sh_entsize = 0;
            }
            {
                info->number_debug_aranges = ++info->elf_header->e_shnum;
                Elf64_Shdr* section = nullptr;
                suc = info->stream->write_late((void**)&section, sizeof(*section));
                CHECK
                memset(section, 0, sizeof(*section));
                info->elf_section_debug_aranges = section;

                section->sh_name = 0; // set in ELF.cpps
                section->sh_type = SHT_PROGBITS;
                section->sh_flags = 0; // no flags?
                section->sh_addr = 0; // always zero for object files
                section->sh_offset = 0; // place where data lies, LATER
                section->sh_size = 0; // size of data, LATER
                section->sh_link = 0; // not used
                section->sh_info = 0; // not used
                section->sh_addralign = 1;
                section->sh_entsize = 0;
            }
            {
                info->number_debug_frame = ++info->elf_header->e_shnum;
                Elf64_Shdr* section = nullptr;
                suc = info->stream->write_late((void**)&section, sizeof(*section));
                CHECK
                memset(section, 0, sizeof(*section));
                info->elf_section_debug_frame = section;

                section->sh_name = 0; // set in ELF.cpps
                section->sh_type = SHT_PROGBITS;
                section->sh_flags = 0; // no flags?
                section->sh_addr = 0; // always zero for object files
                section->sh_offset = 0; // place where data lies, LATER
                section->sh_size = 0; // size of data, LATER
                section->sh_link = 0; // not used
                section->sh_info = 0; // not used
                section->sh_addralign = 1;
                section->sh_entsize = 0;
            }
            break;
        }
        default: Assert(false);
        }
        #undef CHECK
    }
    
    void ProvideSectionData(DWARFInfo* info, DWARF_ObjectFileType objType) {
        using namespace engone;
        using namespace coff;
        using namespace elf;
        
        auto stream = info->stream;
        Section_Header* coff_section = nullptr;
        Elf64_Shdr* elf_section = nullptr;
        auto debug = info->debug;

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
        int abbrev_param = 0;
        int abbrev_base_type = 0;
        int abbrev_pointer_type = 0;
        int abbrev_struct = 0;
        int abbrev_member = 0;

        suc = stream->write_align(8);
        CHECK

        #define OBJ_SWITCH(X,Y) switch(objType) { \
            case DWARF_OBJ_COFF: { X; break; }\
            case DWARF_OBJ_ELF: { Y; break; }\
            default: Assert(false); }

        OBJ_SWITCH(
            coff_section = info->coff_section_debug_abbrev,
            elf_section = info->elf_section_debug_abbrev
        )
        if (coff_section || elf_section){
            OBJ_SWITCH(
                coff_section->PointerToRawData = stream->getWriteHead();,
                elf_section->sh_offset = stream->getWriteHead();
            )
            
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
            WRITE_FORM(DW_AT_low_pc,    DW_FORM_addr)
            WRITE_FORM(DW_AT_high_pc,   DW_FORM_addr)
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
            WRITE_FORM(DW_AT_low_pc,       DW_FORM_addr)
            WRITE_FORM(DW_AT_high_pc,      DW_FORM_addr)
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

            abbrev_member = nextAbbrevCode++;
            WRITE_LEB(abbrev_member) // code
            WRITE_LEB(DW_TAG_member) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            WRITE_FORM(DW_AT_data_member_location, DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_file,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            // WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            

            WRITE_LEB(0) // zero terminate abbreviation section
            
            OBJ_SWITCH(
                coff_section->SizeOfRawData = stream->getWriteHead() - coff_section->PointerToRawData;,
                elf_section->sh_size = stream->getWriteHead() - elf_section->sh_offset;
            )
        }
        suc = stream->write_align(8);
        CHECK

        OBJ_SWITCH(
            coff_section = info->coff_section_debug_info,
            elf_section = info->elf_section_debug_info
        )
        if (coff_section || elf_section){
            OBJ_SWITCH(
                coff_section->PointerToRawData = stream->getWriteHead();,
                elf_section->sh_offset = stream->getWriteHead();
            )
            int offset_section = stream->getWriteHead();

            CompilationUnitHeader* comp = nullptr;
            stream->write_late((void**)&comp, sizeof(CompilationUnitHeader));
            // comp->unit_length = don't know yet;
            comp->version = 3; // DWARF VERSION
            int reloc_abbrev_offset = (u64)&comp->debug_abbrev_offset - (u64)comp;
            comp->debug_abbrev_offset = 0; // 0 since we only have one compilation unit
            comp->address_size = 8;
            
            u8* ptr = nullptr;
            int written = 0;
            
            // debugging entries
            struct Reloc {
                u32 section_offset;
            };
            DynamicArray<Reloc> relocs{};
            relocs._reserve(debug->functions.size() * 2 + 4);

            WRITE_LEB(abbrev_compUnit) // abbrev code
            stream->write("BTB Compiler 0.2.0");
            // stream->write1(0); // language
            stream->write("unknown.btb"); // source file
            stream->write("project/src"); // project dir
            relocs.add({ stream->getWriteHead() - offset_section });
            stream->write8(0); // start address of code
            relocs.add({ stream->getWriteHead() - offset_section });
            stream->write8(info->program->size()); // end address of text code
            int reloc_statement_list = stream->getWriteHead() - offset_section;
            stream->write4(0); // statement list, address/pointer to reloc thing

            // TODO: Sibling for structure types, faster search time for debuggers
            // TODO: Types

            struct TypeRef {
                // each array element belongs to a pointer level
                u32 reference[4] = { 0 };
                int queuePosition = -1;
                int availablePointerLevel = 0;
            };
            // struct QueuedType {
            //     TypeId typeId;
            // };
            DynamicArray<TypeId> queuedTypes{};
            DynamicArray<TypeRef> allTypes{};
            allTypes.resize(debug->ast->_typeInfos.size());

            auto addType = [&](TypeId typeId) {
                auto& allType = allTypes[typeId.getId()];

                if (allType.queuePosition == -1) {
                    allType.queuePosition = queuedTypes.size();
                    queuedTypes.add(typeId);
                } else {
                    auto& queuedType = queuedTypes[allType.queuePosition];
                    if(typeId.getPointerLevel() > queuedType.getPointerLevel())
                        queuedType.setPointerLevel(typeId.getPointerLevel()); // increase pointer level
                }
            };
            
            // TODO: Holy snap the polymorphism will actually end me.
            //  How do we make this work with polymorphism? Maybe it does work?
            for(int i=0;i<debug->functions.size();i++) {
                auto func = debug->functions[i];
                if(!func.funcImpl)
                    continue;

                // TODO: Add return values, types

                for(int pi=0;pi<func.funcImpl->argumentTypes.size();pi++){
                    auto& arg_impl = func.funcImpl->argumentTypes[pi];

                    addType(arg_impl.typeId);
                }
                for(int vi=0;vi<func.localVariables.size();vi++) {
                    auto& var = func.localVariables[vi];
                    addType(var.typeId);
                }
            }

            AST* ast = debug->ast;
            struct LateTypeRef {
                u32 section_offset; // offset to u32, ref4 form
                TypeId typeId;
            };
            DynamicArray<LateTypeRef> lateTypeRefs{};
            for(int i=0;i<queuedTypes.size();i++) {
                TypeId queuedType = queuedTypes[i];
                auto typeInfo = ast->getTypeInfo(queuedType.baseType());
                auto& allType = allTypes[queuedType.getId()];

                allType.queuePosition = -1;
                queuedTypes.removeAt(i); // ...we remove it here
                i--;

                // log::out << "Proc queued type "<<i<<"\n";
                if(allType.reference[0] == 0) { // don't write base reference if it already exists
                    if(typeInfo->structImpl) {
                        Assert(typeInfo->astStruct);

                        // struct type
                        allType.reference[0] = stream->getWriteHead() - offset_section;
                        WRITE_LEB(abbrev_struct)

                        stream->write(typeInfo->name.c_str(), typeInfo->name.length() + 1);
                        stream->write2(typeInfo->getSize());

                        u32* sibling_ref4 = nullptr;
                        stream->write_late((void**)&sibling_ref4, sizeof(u32));
                        
                        Assert(typeInfo->structImpl->members.size() == typeInfo->astStruct->members.size());
                        for (int mi=0;mi<typeInfo->structImpl->members.size();mi++) {
                            auto& memImpl = typeInfo->structImpl->members[mi];
                            auto& memAst = typeInfo->astStruct->members[mi];

                            WRITE_LEB(abbrev_member)
                            stream->write(memAst.name.str, memAst.name.length);
                            stream->write1(0);
                            int typeref = allTypes[memImpl.typeId.getId()].reference[memImpl.typeId.getPointerLevel()];
                            if(typeref == 0) {
                                addType(memImpl.typeId);
                                // log::out << "Late "<<ast->typeToString(memImpl.typeId)<<" at "<< (stream->getWriteHead() - offset_section)<<"\n";
                                lateTypeRefs.add({stream->getWriteHead() - offset_section, memImpl.typeId });
                                stream->write4(0); // not known yet
                            } else {
                                stream->write4(typeref); // ref4
                            }
                            stream->write2(memImpl.offset); // data location
                        }
                        
                        WRITE_LEB(0); // end of members in structure

                        *sibling_ref4 = stream->getWriteHead() - offset_section;
                    } else {
                        Assert(i < AST_TRUE_PRIMITIVES);
                        // other type

                        // log::out << "Prim "<<typeInfo->name<<"\n";
                        allType.reference[0] = stream->getWriteHead() - offset_section;
                        WRITE_LEB(abbrev_base_type)
                        
                        stream->write(typeInfo->name.c_str(), typeInfo->name.length() + 1);
                        stream->write1(typeInfo->getSize()); // size

                        switch(typeInfo->id.getId()) { // encoding (1 byte)
                            case AST_VOID:
                                Assert(false);
                                break;
                            case AST_UINT8:
                            case AST_UINT16:
                            case AST_UINT32:
                            case AST_UINT64:
                                stream->write1(DW_ATE_unsigned);
                                break;
                            case AST_INT8:
                            case AST_INT16:
                            case AST_INT32:
                            case AST_INT64:
                                stream->write1(DW_ATE_signed);
                                break;
                            case AST_BOOL:
                                stream->write1(DW_ATE_boolean);
                                break;
                            case AST_CHAR:
                                stream->write1(DW_ATE_unsigned_char);
                                break;
                            case AST_FLOAT32:
                            case AST_FLOAT64:
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
                    // Note that pointer level = j + 1
                    allType.reference[j + 1] = stream->getWriteHead() - offset_section;
                    WRITE_LEB(abbrev_pointer_type)
                    
                    stream->write1(8); // size, pointers are 8 in size
                    stream->write4(allType.reference[j]); // ref4, we refer to the previous pointer level (or struct/base type)
                }
            }

            for(int i=0;i<lateTypeRefs.size();i++) {
                auto& ref = lateTypeRefs[i];
                u32 typeref = allTypes[ref.typeId.getId()].reference[ref.typeId.getPointerLevel()];
                // log::out << "Late write "<<ast->typeToString(ref.typeId)<<" at "<<ref.section_offset<<"\n";
                stream->write_at<u32>(offset_section + ref.section_offset, typeref);
            }

            // We need this because we added a 16-byte offset in .debug_frame.
            // I don't know why gcc generates DWARF that way but we do the same because I don't know how it works.
            // This offset makes things work and I can't be bothered to question it at the moment.
            // Another problem for future me.
            // - Emarioo, 2024-01-01 (Happy new year!)
            #define RBP_CONSTANT_OFFSET (-16)

            for(int i=0;i<debug->functions.size();i++) {
                auto func = debug->functions[i];

                WRITE_LEB(abbrev_func) // abbrev code
                bool global_func = false;
                WRITE_LEB(global_func) // external or not
                stream->write(func.name.c_str(), func.name.length() + 1); // name

                // Assert(func.lines.size() > 0);

                int file_index = func.fileIndex + 1;
                Assert(file_index < 0x10000); // make sure we don't overflow
                stream->write2((u16)(file_index)); // file
                int line = func.entry_line;
                Assert(line < 0x10000); // make sure we don't overflow
                stream->write2((u16)(line)); // line
                stream->write2((u16)(1)); // column

                // TODO: We skip the return type for now. Can we have multiple?

                relocs.add({ stream->getWriteHead() - offset_section });
                stream->write8((u64)(func.funcStart)); // pc low
                relocs.add({ stream->getWriteHead() - offset_section });
                stream->write8((u64)(func.funcEnd)); // pc high

                stream->write1((u8)(1)); // frame base, begins with block length
                stream->write1((u8)(DW_OP_call_frame_cfa)); // block content
                
                u32* sibling_ref4 = nullptr;
                stream->write_late((void**)&sibling_ref4, sizeof(u32));
                if(func.funcImpl) {
                    Assert(func.funcImpl->argumentTypes.size() == func.funcAst->arguments.size());
                    for(int pi=0;pi<func.funcImpl->argumentTypes.size();pi++){
                        auto& arg_ast = func.funcAst->arguments[pi];
                        auto& arg_impl = func.funcImpl->argumentTypes[pi];
                        WRITE_LEB(abbrev_param)
                        stream->write(arg_ast.name.str, arg_ast.name.length); // arg_ast.name is a Token and not zero terminated
                        stream->write1(0); // we must zero terminate here

                        stream->write2((file_index)); // file
                        Assert(arg_ast.name.line < 0x10000); // make sure we don't overflow
                        stream->write2((arg_ast.name.line)); // line
                        Assert(arg_ast.name.column < 0x10000); // make sure we don't overflow
                        stream->write2((arg_ast.name.column)); // column

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
                        WRITE_SLEB(arg_impl.offset)
                        *block_length = stream->getWriteHead() - off_start; // we write block length later since we don't know the size of the LEB128 integer
                    }
                }
                for(int vi=0;vi<func.localVariables.size();vi++) {
                    auto& var = func.localVariables[vi];

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
                    stream->write1(DW_OP_fbreg); // operation, fbreg describes that we should use a register (rbp) with an offset to get the argument.
                    WRITE_SLEB(var.frameOffset + RBP_CONSTANT_OFFSET)
                    *block_length = stream->getWriteHead() - off_start; // we write block length later since we don't know the size of the LEB128 integer
                }

                WRITE_LEB(0) // end subprogram entry

                *sibling_ref4 = stream->getWriteHead() - offset_section;
            }

            WRITE_LEB(0) // end compile unit

            OBJ_SWITCH(
                coff_section->SizeOfRawData = info->stream->getWriteHead() - coff_section->PointerToRawData;,
                elf_section->sh_size = info->stream->getWriteHead() - elf_section->sh_offset;
            )
            comp->unit_length = info->stream->getWriteHead() - offset_section - sizeof(CompilationUnitHeader::unit_length);

            // ################
            //   RELOCATIONS
            // ################
            Assert(false); // relocations for ELF
            auto section = coff_section;
            section->PointerToRelocations = stream->getWriteHead();
            section->NumberOfRelocations = 0;
            {
                section->NumberOfRelocations++;
                bool suc = false;
                COFF_Relocation* coffReloc = nullptr;
                suc = stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
                CHECK

                coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECREL;
                coffReloc->VirtualAddress = reloc_abbrev_offset;
                coffReloc->SymbolTableIndex = info->symindex_debug_abbrev; // .text
            }
            {
                section->NumberOfRelocations++;
                bool suc = false;
                COFF_Relocation* coffReloc = nullptr;
                suc = stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
                CHECK

                coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECREL;
                coffReloc->VirtualAddress = reloc_statement_list;
                coffReloc->SymbolTableIndex = info->symindex_debug_line; // .text
            }
            for(int i=0;i<relocs.size();i++) {
                auto& rel = relocs[i];
                section->NumberOfRelocations++;
                bool suc = false;
                COFF_Relocation* coffReloc = nullptr;
                suc = stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
                CHECK

                coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR64;
                coffReloc->VirtualAddress = rel.section_offset;
                coffReloc->SymbolTableIndex = info->symindex_text; // .text
            }
        }
        suc = stream->write_align(8);
        CHECK
        OBJ_SWITCH(
            coff_section = info->coff_section_debug_line,
            elf_section = info->elf_section_debug_line
        )
        if (coff_section || elf_section){
            OBJ_SWITCH(
                coff_section->PointerToRawData = stream->getWriteHead();,
                elf_section->sh_offset = stream->getWriteHead();
            )
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
                    if(used_functions[j])
                        continue;
                    if(debug->functions[j].funcStart < lowest_address || lowest_address == -1) {
                        lowest_address = debug->functions[j].funcStart;
                        lowest_index = j;
                    }
                }
                if(lowest_index == -1)
                    break; // no function left
                functions_in_order.add(lowest_index);
                used_functions[lowest_index] = true;
            }

            // The code won't generate any line number information if there are no functions.
            // The language allows code in the global scope without specifying a function.
            // This is a bit of problem because it's not really seen as a function.
            // That needs to be ironed out if we want things to work properly here.
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
            stream->write1(1 + 8); // then count of bytes for the whole extended operation
            WRITE_LEB(DW_LNE_set_address) // then opcode and data
                int reloc_pc_addr = stream->getWriteHead() - offset_section;
                stream->write8(0);
                reg_address = 0;


            if(functions_in_order.size()>0) {
                auto& fun = debug->functions[functions_in_order[0]];
                reg_file = fun.fileIndex + 1;
                WRITE_LEB(DW_LNS_set_file)
                WRITE_LEB(reg_file)
            }

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
                    reg_address += dt_code;
                    WRITE_LEB(DW_LNS_advance_pc)
                    WRITE_LEB(dt_code)

                    reg_line += dt_line;
                    WRITE_LEB(DW_LNS_advance_line)
                    WRITE_SLEB(dt_line)

                    WRITE_LEB(DW_LNS_copy)
                }
            };

            // WRITE_LEB(DW_LNS_copy)
            for(int fi : functions_in_order) {
                auto& fun = debug->functions[fi];
                int file_index = fun.fileIndex + 1;
                if(reg_file != file_index) {
                    reg_file = file_index;
                    WRITE_LEB(DW_LNS_set_file)
                    WRITE_LEB(reg_file)
                }

                // TODO: Set column
                add_row(fun.funcStart, fun.entry_line);

                int lastOffset = 0;
                int lastLine = 0;
                for(int i=0;i<fun.lines.size();i++) {
                    auto& line = fun.lines[i];
                    // Ensure that the lines are stored in ascending order
                    Assert(lastOffset <= line.funcOffset);
                    // Assert(lastLine <= line.lineNumber);
                    lastOffset = line.funcOffset;
                    lastLine = line.lineNumber;

                    add_row(line.funcOffset, line.lineNumber);
                }
                // the debugger won't stop at the last instruction because the line
                // number is the same. I was hoping it would stop at the return statement
                // so that you could see the final result of all variables but we need
                // some stuff elsewhere to make that possible.
                // add_row(fun.funcEnd, fun.lines.last().lineNumber);
            }

            int dt = info->program->size() - reg_address;
            if(dt) {
                WRITE_LEB(DW_LNS_advance_pc)
                reg_address += dt;
                WRITE_LEB(dt)
            }

            stream->write1(0); // extended opcode begins with zero
            stream->write1(1); // number of bytes for ext. opcode
            WRITE_LEB(DW_LNE_end_sequence) // actual opcode
            auto section = coff_section;
            section->SizeOfRawData = stream->getWriteHead() - section->PointerToRawData;
            header->unit_length = section->SizeOfRawData - sizeof(LineNumberProgramHeader::unit_length);

            // ###############
            //   RELOCATIONS
            // ###############
            section->PointerToRelocations = stream->getWriteHead();
            section->NumberOfRelocations = 1;

            bool suc = false;
            COFF_Relocation* coffReloc = nullptr;
            suc = stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
            CHECK

            coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR64;
            coffReloc->VirtualAddress = reloc_pc_addr;
            coffReloc->SymbolTableIndex = info->symindex_text; // .text
        }
        suc = stream->write_align(8);
        CHECK
        OBJ_SWITCH(
            coff_section = info->coff_section_debug_aranges,
            elf_section = info->elf_section_debug_aranges
        )
        if (coff_section || elf_section){
            OBJ_SWITCH(
                coff_section->PointerToRawData = stream->getWriteHead();,
                elf_section->sh_offset = stream->getWriteHead();
            )
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
            header->address_size = 8;
            header->segment_size = 0;

            suc = stream->write_align(8);
            CHECK
            
            int reloc_text_start = stream->getWriteHead() - offset_section;
            stream->write8(0);
            stream->write8(info->program->size());

            stream->write8(0);
            stream->write8(0);

            header->unit_length = (stream->getWriteHead() - offset_section) - sizeof(ArangesHeader::unit_length);
            
            OBJ_SWITCH(
                coff_section->SizeOfRawData = stream->getWriteHead() - coff_section->PointerToRawData,
                elf_section->sh_size = stream->getWriteHead() - elf_section->sh_offset;
            )

            // ###############
            //   RELOCATIONS
            // ###############
            auto section = coff_section;
            section->PointerToRelocations = stream->getWriteHead();
            section->NumberOfRelocations = 2;

            {
                bool suc = false;
                COFF_Relocation* coffReloc = nullptr;
                suc = stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
                CHECK

                coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECREL;
                coffReloc->VirtualAddress = reloc_debug_info_offset;
                coffReloc->SymbolTableIndex = info->symindex_debug_info; // .text
            }
            {
                bool suc = false;
                COFF_Relocation* coffReloc = nullptr;
                suc = stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
                CHECK

                coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR64;
                coffReloc->VirtualAddress = reloc_text_start;
                coffReloc->SymbolTableIndex = info->symindex_text; // .text
            }
        }
        #ifndef DISABLE_DEBUG_FRAME
        suc = stream->write_align(8);
        CHECK
        OBJ_SWITCH(
            coff_section = info->coff_section_debug_frame,
            elf_section = info->elf_section_debug_frame
        )
        if (coff_section || elf_section){
            OBJ_SWITCH(
                coff_section->PointerToRawData = stream->getWriteHead();,
                elf_section->sh_offset = stream->getWriteHead();
            )
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
                DW_rbp = 6,
                DW_rsp = 7,
                DW_rip = 16,
            };
            
            {
                CommonInformationEntry* header = nullptr;
                stream->write_late((void**)&header, sizeof(CommonInformationEntry));
                // header->length = don't know yet;
                header->CIE_id = 0xFFFFFFFF; // marks that this is isn't a FDE but a CIE
                header->version = 3; // Specific version for debug_frame (not the DWARF version)
                
                stream->write1(0); // augmentation string, ZERO for now?

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

                // What I have understood from what GCC generates is that the whole CIE should start at an 8-byte alignment
                // and end at an 8-byte alignment. The same goes for the following FDEs
                stream->write_align(8);
                header->length = (stream->getWriteHead() - offset_section) - sizeof(CommonInformationEntry::length);
            }
            struct Reloc {
                int symindex_section;
                u64 sectionOffset;
            };
            DynamicArray<Reloc> relocs{};
            for (int fi=0;fi<debug->functions.size();fi++) {
                auto& fun = debug->functions[fi];
                Assert(stream->getWriteHead() % 8 == 0);
                int offset_fde_start = stream->getWriteHead();

                FrameDescriptionEntry* header = nullptr;
                stream->write_late((void**)&header, sizeof(FrameDescriptionEntry));
                // header->length = don't know yet;
                relocs.add({info->symindex_debug_frame, offset_fde_start - offset_section  + (u64)&header->CIE_pointer - (u64)header });
                header->CIE_pointer = 0; // common information entry can be found at offset 0 in the section, may not be true in the future
                
                relocs.add({info->symindex_text, offset_fde_start - offset_section  + (u64)&header->initial_location - (u64)header });
                header->initial_location = fun.funcStart;
                header->address_range = fun.funcEnd - fun.funcStart;

                // instructions, based on what g++ generates and a little from DWARF specification
                stream->write1(DW_CFA_advance_loc4);
                stream->write4(1);

                stream->write1(DW_CFA_def_cfa_offset);
                WRITE_LEB(16)

                stream->write1(DW_CFA_offset(DW_rbp));
                WRITE_LEB(2) // * data_alignment_factor

                stream->write1(DW_CFA_advance_loc4);
                stream->write4(3);

                stream->write1(DW_CFA_def_cfa_register);
                WRITE_LEB(DW_rbp)

                stream->write1(DW_CFA_advance_loc4);
                stream->write4(fun.funcEnd - fun.funcStart - 1 - 4);

                stream->write1(DW_CFA_restore(DW_rbp));

                stream->write1(DW_CFA_def_cfa);
                WRITE_LEB(DW_rsp)
                WRITE_LEB(8)
                
                // potential padding
                stream->write_align(8);
                header->length = (stream->getWriteHead() - offset_fde_start) - sizeof(FrameDescriptionEntry::length);
            }
            OBJ_SWITCH(
                coff_section->SizeOfRawData = stream->getWriteHead() - coff_section->PointerToRawData,
                elf_section->sh_size = stream->getWriteHead() - elf_section->sh_offset;
            )

            // ################
            //   RELOCATIONS
            // ################
            auto section = coff_section;
            section->PointerToRelocations = stream->getWriteHead();
            section->NumberOfRelocations = 0;

            for(int i=0;i<relocs.size();i++) {
                auto& rel = relocs[i];
                section->NumberOfRelocations++;
                bool suc = false;
                COFF_Relocation* coffReloc = nullptr;
                suc = stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
                CHECK
                if(rel.symindex_section == info->symindex_text) {
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR64;
                } else {
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECREL;
                }
                coffReloc->VirtualAddress = rel.sectionOffset;
                coffReloc->SymbolTableIndex = rel.symindex_section;
            }
        }
        #endif
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