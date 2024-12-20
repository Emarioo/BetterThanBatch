/*
    Common mistakes

    If you get "relocation truncated to fit ..." then it may be because you used the wrong type for a symbol.
    For example, using ADDR32NB relocation and IMAGE_SYM_CLASS_EXTERNAL symbol type will not work while IMAGE_SYM_CLASS_LABEL will.
*/

#include "BetBat/ObjectFile.h"

#include "BetBat/DWARF.h"
#include "BetBat/ELF.h"
#include "BetBat/COFF.h"
#include "BetBat/Compiler.h"

ObjectFile::SectionFlags operator |(ObjectFile::SectionFlags a, ObjectFile::SectionFlags b) {
    return (ObjectFile::SectionFlags)((int)a | (int)b);
}

bool ObjectFile::WriteFile(ObjectFileType objType, const std::string& path, Program* program, Compiler* compiler, u32 from, u32 to) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue4);

    ObjectFile objectFile{};
    objectFile.init(objType);

    bool suc = false;
    #define CHECK Assert(suc);
    
    // objectFile.addSymbol(ObjectFile::SYM_FILE, "dev.btb", SHN_ABS, 0);

    auto section_text = objectFile.createSection(".text", FLAG_CODE, compiler->options->target==TARGET_ARM?4:16);
    SectionNr section_data = -1;
    if (program->globalSize!=0){
        section_data = objectFile.createSection(".data", FLAG_WRITE, 8);
    }
    if(program->debugInformation) {
        // We need to do this first because it adds more sections.
        // Sections have symbols that should be local with the ELF formats.
        // All local symbols MUST be added first (that's how elf works).
        dwarf::ProvideSections(&objectFile, program, compiler, false);
    }
    bool has_exceptions = compiler->code_has_exceptions;
    // has_exceptions = true;
    SectionNr section_xdata = -1;
    SectionNr section_pdata = -1;
    SectionNr section_arm_attr = -1;
    SectionNr section_comment = -1;
    SectionNr section_datas = -1;
    SectionNr section_bss = -1;
    // SectionNr section_pdata2 = -1;
    if(has_exceptions) {
        section_xdata = objectFile.createSection(".xdata", FLAG_READ_ONLY, 4);
        section_pdata = objectFile.createSection(".pdata", FLAG_READ_ONLY, 4);
        // section_pdata2 = objectFile.createSection(".pdata", FLAG_NONE | FLAG_READ_ONLY, 4);
    }
    if(compiler->options->target == TARGET_ARM) {
        // section_datas = objectFile.createSection(".data", FLAG_WRITE, 1);
        // section_bss = objectFile.createSection(".bss", FLAG_WRITE, 1);
        
        // this symbol tells objdump that after this offset (text section)
        // we have instructions you can decode, without the symbol the
        // section will be decoded as words
        objectFile.addSymbol(ObjectFile::SYM_LOCAL_NOTYPE, "$a", section_text, 0);
        // objectFile.addSymbol(ObjectFile::SYM_EMPTY, "$d", section_data, 0);
        // section_comment = objectFile.createSection(".comment", FLAG_NONE, 1);
        section_arm_attr = objectFile.createSection(".ARM.attributes", FLAG_NONE, 1);
    }

    DynamicArray<u32> tinyprogram_offsets;
    auto text_stream = objectFile.getStreamFromSection(section_text);
    if(to - from > 0) {
        Assert(from == 0 && to == -1);
        tinyprogram_offsets.resize(program->functionPrograms.size());
        {
            // main function should be first in the text section
            int i = program->index_of_main;
            if(i < 0) {
                // TODO: Check if we're compiling a dll or library.
                //   They don't have entry point.
                //   If we compile exe then we should have entry point.
            } else {
                auto tinyprog = program->functionPrograms[i];
                auto tinycode = compiler->bytecode->tinyBytecodes[i];

                if(tinyprog->head == 0) {
                    MSG_CODE_LOCATION;
                    log::out << engone::log::RED<< "COMPILER BUG:"<<engone::log::NO_COLOR<<" Function '"<<log::GREEN << tinycode->name<<log::NO_COLOR<<"' did not have any instructions (x64/ARM generator did not create any).\n";
                    return false;
                }

                tinyprogram_offsets[i] = text_stream->getWriteHead();
                text_stream->write(tinyprog->text, tinyprog->head);
                
                tinycode->debugFunction->asm_start = tinyprogram_offsets[i];
                tinycode->debugFunction->asm_end = text_stream->getWriteHead();
            }
        }
        
        bool messaged = false;
        for(int i=0;i<program->functionPrograms.size(); i++) {
            if(i == program->index_of_main)
                continue;

            auto tinyprog = program->functionPrograms[i];
            if(!tinyprog)
                continue; // the index (i) may have been a temporary tinycode for compile time, that is why no tinyprogram was created

            tinyprogram_offsets[i] = text_stream->getWriteHead();
            text_stream->write(tinyprog->text, tinyprog->head);
            
            auto tinycode = compiler->bytecode->tinyBytecodes[i];
            tinycode->debugFunction->asm_start = tinyprogram_offsets[i];
            tinycode->debugFunction->asm_end = text_stream->getWriteHead();

            if(text_stream->getWriteHead() > 0x4000'0000 && !messaged) {
                messaged = true;
                MSG_CODE_LOCATION;
                engone::log::out << engone::log::RED << "ABOUT TO REACH 4 GB LIMIT IN TEXT SECTION. THE COMPILER ASSUMES THAT 32-bit INTEGERS IS ENOUGH.\n";
            }
        }
        
        // Old code
        // if(to > program->size())
        //     to = program->size();
        // suc = stream->write(program->text + from, to - from);
        // CHECK
    }

    DynamicArray<int> func_to_unwind{};
    if(has_exceptions) {
        // TinyBytecode* tiny_handler = nullptr;
        // const char* handler_name = "exception_handler"; // TODO: Don't hardcode name of exception handler
        // for(int i=0;i<compiler->bytecode->tinyBytecodes.size();i++) {
        //     auto tc = compiler->bytecode->tinyBytecodes[i];
        //     if(tc->name == handler_name) {
        //         tiny_handler = tc;
        //         break;
        //     }
        // }
        // if(!tiny_handler) {
        //     log::out << log::RED << "Compiler could not find the exception handler when creating sections (in object file) for exceptions. Was the exception handler not exported? Is the handler not named '"<<handler_name<<"'\n";
        //     return false;
        // }
        
        // Is this symbol necessary? exceptions seems to work anyway.
        // objectFile.addSymbol(SymbolType::SYM_ABS, "@feat.00", 0, 0x8000'0000);

        auto xdata_stream = objectFile.getStreamFromSection(section_xdata);
        auto pdata_stream = objectFile.getStreamFromSection(section_pdata);
        // auto pdata2_stream = objectFile.getStreamFromSection(section_pdata2);

        // for each function, create unwindinfo in xdata
        
        DynamicArray<TinyBytecode*> valid_tinycodes{};
        for(auto t : compiler->bytecode->tinyBytecodes) {
            if(program->functionPrograms.size() <= t->index)
                continue;
            auto tinyprog = program->functionPrograms[t->index];
            if(!tinyprog)
                continue; // the index (i) may have been a temporary tinycode for compile time, that is why no tinyprogram was created

            valid_tinycodes.add(t);
        }
        func_to_unwind.resize(valid_tinycodes.size());

        int sym_text_base = objectFile.addSymbol(SYM_LABEL, "$text$base", section_text, 0);

        for(int ci=0;ci<valid_tinycodes.size();ci++) {
            auto tinycode = valid_tinycodes[ci];
            func_to_unwind[ci] = xdata_stream->getWriteHead();

            // log::out << tinycode->name << " : " << tinycode->index<<"\n";

            coff::UNWIND_INFO* unwind_info = nullptr;
            xdata_stream->write_late((void**)&unwind_info, sizeof(*unwind_info));

            unwind_info->Version = 1;
            // unwind_info->Flags = coff::UNW_FLAG_NHANDLER; // no handler
            unwind_info->Flags = coff::UNW_FLAG_EHANDLER; // no handler
            // unwind_info->SizeOfProlog = 1*3 + 6*2; // 1*3 for one 'mov rbp, rsp' and 6*2 for six 'push reg'
            unwind_info->SizeOfProlog = 1*3 + 2*2; // 1*3 for one 'mov rbp, rsp' and 2*2 for six 'push reg'
            unwind_info->FrameRegister = 0;
            unwind_info->FrameRegisterOffset = 0;
            unwind_info->CountOfUnwindCodes = 0;
            // unwind_info->CountOfUnwindCodes = 2;
            // TODO: set values

            // if(unwind_info->CountOfUnwindCodes > 0) {
            //     coff::UNWIND_CODE* codes = nullptr;
            //     xdata_stream->write_late((void**)&codes, unwind_info->CountOfUnwindCodes * sizeof(*codes));
                
            //     const coff::UnwindOpRegister ops[]{
            //         coff::UWOP_RSP,
            //         coff::UWOP_RBP,
            //     };
            //     const int ops_offset[]{
            //         5,
            //         0,
            //     };
            //     int ops_len = sizeof(ops)/sizeof(*ops);
            //     for(int i=0;i<ops_len;i++) {
            //         codes[i].OffsetInProlog = ops_offset[i];
            //         codes[i].UnwindOperationCode = coff::UWOP_PUSH_NONVOL;
            //         codes[i].OperationInfo = ops[i];
            //     }
            // }

            if(unwind_info->Flags & coff::UNW_FLAG_EHANDLER) {
                xdata_stream->write_align(4);

                // int offset = tinyprogram_offsets[tiny_handler->index];
                // xdata_stream->write4(offset);
                xdata_stream->write4(0); // relocation is added further down when exported functions have been added as symbols

                // Language handler data
                /*
                    // ALSO DEFINED IN Exception.btb
                    struct TryBlock {
                        u32 asm_start;
                        u32 asm_end;
                        u32 asm_catch_start;
                        u32 filter_code;
                        i32 frame_offset;
                    };
                    struct HandlerData {
                        u32 length;
                        i32 frame_offset;
                        TryBlock blocks[length];
                    }
                */
                xdata_stream->write4((int)tinycode->try_blocks.size());
                if(tinycode->try_blocks.size() > 0) {
                    xdata_stream->write4(tinycode->try_blocks[0].frame_offset_before_try); // every frame offset is the same
                } else {
                    xdata_stream->write4(0);
                }

                for(int j=0;j<tinycode->try_blocks.size();j++) {
                    auto& block = tinycode->try_blocks[j];

                    int offset = xdata_stream->getWriteHead();
                    int func_start = tinyprogram_offsets[tinycode->index];
                    xdata_stream->write4(func_start + block.asm_start);
                    xdata_stream->write4(func_start + block.asm_end);
                    xdata_stream->write4(func_start + block.asm_catch_start);
                    xdata_stream->write4(block.filter_exception_code);
                    xdata_stream->write4(block.offset_to_exception_info);

                    objectFile.addRelocation(section_xdata, RELOCA_ADDR32NB, offset + 0, sym_text_base, 0);
                    objectFile.addRelocation(section_xdata, RELOCA_ADDR32NB, offset + 4, sym_text_base, 0);
                    objectFile.addRelocation(section_xdata, RELOCA_ADDR32NB, offset + 8, sym_text_base, 0);
                }
            }

            xdata_stream->write_align(4);
        }

        if(valid_tinycodes.size() > 0) {
            coff::RUNTIME_FUNCTION* functions = nullptr;
            pdata_stream->write_late((void**)&functions, valid_tinycodes.size() * sizeof(*functions));

            int eh_number = 0;
            int eh_unwind = 0;

            // base unwind, TODO: In future each function needs there own unwind
            int sym_unwind = objectFile.addSymbol(SYM_EMPTY, "$unwind$base", section_xdata, 0);
            int func_index = 0;
            auto gen_entry = [&](int i) {
                auto tinycode = valid_tinycodes[i];
                auto& func = functions[func_index];
                func_index++;
                memset(&func, 0, sizeof(func));
                if(!tinycode->debugFunction) {
                    return;
                }

                func.StartAddress = tinycode->debugFunction->asm_start;
                func.EndAddress = tinycode->debugFunction->asm_end;
                func.UnwindInfoAddress = func_to_unwind[i];

                #undef ADDR
                #define ADDR(X) ((u64)&func - (u64)functions) + X

                // log::out << tinycode->name << " " << tinycode->debugFunction->asm_start<<"\n";

                objectFile.addRelocation(section_pdata, RELOCA_ADDR32NB, ADDR(0), sym_text_base, 0);
                objectFile.addRelocation(section_pdata, RELOCA_ADDR32NB, ADDR(4), sym_text_base, 0);
                objectFile.addRelocation(section_pdata, RELOCA_ADDR32NB, ADDR(8), sym_unwind, 0);
                #undef ADDR
            };
            // main is first
            gen_entry(program->index_of_main);
            for(int ci=0;ci<valid_tinycodes.size();ci++) {
                if(program->index_of_main == valid_tinycodes[ci]->index)
                    continue;

                gen_entry(ci);
            }
        }

    }
    
    if(section_arm_attr != -1) {
        // Format for .ARM.attributes can be found here:
        //   https://github.com/ARM-software/abi-aa/blob/main/addenda32/addenda32.rst
        auto stream = objectFile.getStreamFromSection(section_arm_attr);
        stream->write1('A'); // format version
        
        // subsections
        int subsection_offset = stream->getWriteHead();
        int subsection_length_off = stream->getWriteHead();
        stream->write4(4); // subsection-length, whole size excluding format version (we don't know the size yet)
        stream->write("aeabi");
        
        // sub-subsections (vendor data)
        int sub_subsection_offset = stream->getWriteHead();
        stream->write1(Tag_File);
        int sub_subsection_length_off = stream->getWriteHead();
        stream->write4(0); // sub-subsection length, whole sub-subsection size
        
        u8* temp_uleb128_ptr;
        int written;
        #define ATTR_N(TAG, NUM) stream->write1(TAG);\
            stream->write_unknown((void**)&temp_uleb128_ptr, 16); \
            written = dwarf::ULEB128_encode(temp_uleb128_ptr, 16, NUM); \
            stream->wrote_unknown(written);
        #define ATTR_S(TAG, STR) stream->write1(TAG); stream->write(STR);
        
        // TODO: How do we pick ARM architecture type?
        ATTR_S(Tag_CPU_name, "4T")
        ATTR_N(Tag_CPU_arch, Arm_v4T)
        ATTR_N(Tag_ARM_ISA_use, true)
        ATTR_N(Tag_THUMB_ISA_use, 1)
        ATTR_N(Tag_ABI_PCS_wchar_t, 4)
        ATTR_N(Tag_ABI_FP_denormal, 1)        // : Needed
        ATTR_N(Tag_ABI_FP_exceptions, 1)      // : Needed
        ATTR_N(Tag_ABI_FP_number_model, 3)    // : IEEE 754
        ATTR_N(Tag_ABI_align_needed, 1)       // : 8-byte
        ATTR_N(Tag_ABI_align8_preserved, 1)       // : 8-byte
        ATTR_N(Tag_ABI_enum_size, 1)          // : small
        ATTR_N(Tag_ABI_optimization_goals, 6) // : Aggressive Debug
        
        #undef ATTR_N
        #undef ATTR_S
        
        stream->write_at<u32>(sub_subsection_length_off, stream->getWriteHead() - sub_subsection_offset);
        stream->write_at<u32>(subsection_length_off, stream->getWriteHead() - subsection_offset);
        
    }
    // if(section_comment != -1) {
    //     auto stream = objectFile.getStreamFromSection(section_comment);
    //     stream->write("GCC: (Arm GNU Toolchain 13.3.Rel1 (Build arm-13.24)) 13.3.1 20240614");
        
    //     objectFile.addRelocation(section_text, RELOC_ARM_V4BX, text_stream->getWriteHead()-4, 0, 0); 
    // }
    
    int symdata = 0;
    if(section_data!=-1)
        symdata = objectFile.getSectionSymbol(section_data);
    for(int i=0;i<program->dataRelocations.size();i++){
        auto& rel = program->dataRelocations[i];
        u32 real_offset = tinyprogram_offsets[rel.tinyprog_index] + rel.textOffset;
        if(compiler->options->target == TARGET_ARM) {
            objectFile.addRelocation(section_text, RELOC_ARM_RELOC_DATA ,real_offset, symdata, 0);
        } else {
            objectFile.addRelocation_data(section_text, real_offset, section_data, rel.dataOffset);
        }
        
    }
    // for(int i=0;i<program->ptrDataRelocations.size();i++){
    //     auto& rel = program->ptrDataRelocations[i];
    //     objectFile.addRelocation_ptr(section_data, rel.referer_dataOffset, section_data, rel.target_dataOffset);
    // }

    for(int i=0;i<program->namedUndefinedRelocations.size();i++){
        auto& namedRelocation = program->namedUndefinedRelocations[i];

        int sym = objectFile.findSymbol(namedRelocation.name);
        if(sym == -1) {
            if(namedRelocation.is_global_var) {
                // IS ZERO AS SYMBOL TYPE FINE?
                sym = objectFile.addSymbol((ObjectFile::SymbolType)SYM_EMPTY, namedRelocation.name, 0, 0);
            } else {
                sym = objectFile.addSymbol(SYM_FUNCTION, namedRelocation.name, 0, 0);
            }
        }

        u32 real_offset = tinyprogram_offsets[namedRelocation.tinyprog_index] + namedRelocation.textOffset;
        if(namedRelocation.is_global_var) {
            if(compiler->options->target == TARGET_WINDOWS_x64) {
                // THIS RELOCATION IS IMPORTANT, IT SHOULD BE REL32, gcc generates it in this exact scenario so we do it to. DON'T TOUCH IT YOU HEARE ME!
                objectFile.addRelocation(section_text, RELOCA_REL32, real_offset, sym, 0);
            } else if (compiler->options->target == TARGET_LINUX_x64) {
                objectFile.addRelocation(section_text, RELOCA_PC32, real_offset, sym, 0);
            } else Assert(false);
        } else
            objectFile.addRelocation(section_text, RELOCA_REL32, real_offset, sym, 0);
    }

    for(int i=0;i<program->internalFuncRelocations.size();i++){
        auto& rel = program->internalFuncRelocations[i];
         
        if(compiler->options->target == TARGET_ARM) {
            u32 from_real_offset = tinyprogram_offsets[rel.from_tinyprog_index] + rel.textOffset;
            u32 to_real_offset = tinyprogram_offsets[rel.to_tinyprog_index];
            int jump_offset = to_real_offset - from_real_offset - 8;
            // bl (immediate) instruction expecteds relative jump offset divided by 4   
            int jump_offset_div = jump_offset/4;
            // jump_offset /= 4;
            if(rel.get_arm_func_address) {
                text_stream->write_at<i32>(from_real_offset, jump_offset + rel.extra_offset - 4);
                // objectFile.addRelocation(section_text, RELOCA_PC32, unwind_offset + 4, index, 0);
            } else {
                text_stream->write_at<u8>(from_real_offset, jump_offset_div&0xff);
                text_stream->write_at<u8>(from_real_offset+1, (jump_offset_div>>8)&0xFF);
                text_stream->write_at<u8>(from_real_offset+2, (jump_offset_div>>16)&0xff);
            }
        } else {
            u32 from_real_offset = tinyprogram_offsets[rel.from_tinyprog_index] + rel.textOffset;
            u32 to_real_offset = tinyprogram_offsets[rel.to_tinyprog_index];
            
            text_stream->write_at<u32>(from_real_offset, to_real_offset - from_real_offset - 4);
        }
    }

    for(int i=0;i<program->exportedSymbols.size();i++) {
        auto& sym = program->exportedSymbols[i];

        // Assert(false); // text offset was broken in rewrite 0.2.1, we use text offset + offset of tiny program
        // u32 real_offset = tinyprogram_offsets[sym.tinyprog_index] + sym.textOffset;
        
        Assert(sym.tinyprog_index != -1); // may happen if function wasn't generated before we exported it or something?
        u32 real_offset = tinyprogram_offsets[sym.tinyprog_index];
        // if(compiler->options->target == TARGET_ARM) {
        //     // Function symbols must have the function size attached to it because disassembling elf files
        //     // with objdump or readelf will show raw data (.word) instead of the instruction mnemonics.
        //     int function_size = program->functionPrograms[sym.tinyprog_index]->head;
        //     objectFile.addSymbol(SYM_FUNCTION, sym.name, section_text, real_offset, function_size);
        // } else {
        objectFile.addSymbol(SYM_FUNCTION, sym.name, section_text, real_offset);
        // }
    }

    if(has_exceptions) {
        int index = objectFile.findSymbol("exception_handler");
        if(index == -1) {
            MSG_CODE_LOCATION;
            log::out << log::RED << "ERROR (compiler bug): The code to compile contains try-catch blocks but the source code does not contain an 'exception_handler'. The default handler exists in Exception.btb. You can define your own by passing the macro DISABLE_DEFAULT_EXCEPTION_HANDLER to the compiler, it will disable the default exception.\n";
            return false;
        }
        for(int i=0;i<func_to_unwind.size();i++) {
            int unwind_offset = func_to_unwind[i];
            // objectFile.addRelocation(section_xdata, RELOCA_ADDR32NB, unwind_offset + 8, index, 0);
            objectFile.addRelocation(section_xdata, RELOCA_ADDR32NB, unwind_offset + 4, index, 0);
        }
    }

    // Useful resource when figuring out how linking and calls work.
    // https://dennisbabkin.com/blog/?t=intricacies-of-microsoft-compilers-part-2-__imp_-and-__imp_load_-prefixes

    if(program->globalSize != 0) {
        auto stream = objectFile.getStreamFromSection(section_data);
        suc = stream->write(program->globalData, program->globalSize);
        CHECK

        if(program->globalSize > 8000'0000) {
            MSG_CODE_LOCATION;
            engone::log::out << engone::log::RED << "THE COMPILER CANNOT HANDLE GLOBAL DATA OF MORE THAN 2 GB\n";
        }
    }

    if(program->debugInformation) {
        // Now we write the debug data
        dwarf::ProvideSections(&objectFile, program, compiler, true);
    }

    // if(program->debugInformation) {
    //     log::out << "Printing debug information\n";
    //     program->debugInformation->print();
    // }
    ObjectFileExtraInfo extra_info{};
    extra_info.target = compiler->options->target;
    bool yes = false;
    if(objType == OBJ_COFF)
        yes = objectFile.writeFile_coff(path, &extra_info);
    else
        yes = objectFile.writeFile_elf(path, &extra_info);
    return yes;
}
bool ObjectFile::writeFile_coff(const std::string& path, ObjectFileExtraInfo* extra_info) {
    using namespace coff;
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue4);
    ByteStream stream{nullptr};
    ByteStream* obj_stream = &stream;
    u32 estimation = 0x100000;
    obj_stream->reserve(estimation);

    bool suc = true;
    #define CHECK Assert(suc);
    
    // ###########################
    //  HEADER and SECTION HEADERS
    // ##############################
    COFF_File_Header* header = nullptr;
    suc = obj_stream->write_late((void**)&header, COFF_File_Header::SIZE);
    CHECK
    header->Machine = (Machine_Flags)IMAGE_FILE_MACHINE_AMD64;
    header->Characteristics = (COFF_Header_Flags)0;
    header->NumberOfSections = 0; // incremented later
    header->NumberOfSymbols = 0; // incremented later
    header->SizeOfOptionalHeader = 0;
    header->TimeDateStamp = time(nullptr);

    DynamicArray<Section_Header*> sectionHeaders{};
    sectionHeaders.resize(_sections.size() + 1);
    for(int i=0;i<_sections.size();i++){
        auto& section = _sections[i];
        auto& sheader = sectionHeaders[section->number];

        ++header->NumberOfSections;
        suc = obj_stream->write_late((void**)&sheader, Section_Header::SIZE);
        CHECK

        if(section->name.length() > 8) {
            u32 off = addString(section->name);
            // bigger number can't fit inside sheader->Name using snprintf because
            // snprintf thinks we need the null terminated byte. PE Format specification
            // says that the zero  byte isn't necessary if the final length is 8.
            Assert(off <= 999999);
            memset(sheader->Name,0,sizeof(sheader->Name));
            int len = snprintf(sheader->Name,sizeof(sheader->Name),"/%u",off);
            Assert(len <= 8);
        } else {
            memcpy(sheader->Name, section->name.c_str(), section->name.length());
            if(section->name.length() < 8)
                sheader->Name[section->name.length()] = '\0';
        }
        sheader->NumberOfLineNumbers = 0;
        sheader->PointerToLineNumbers = 0;
        sheader->VirtualAddress = 0;
        sheader->VirtualSize = 0;
        sheader->Characteristics = (Section_Flags)(0);
        
        if(section->flags == FLAG_READ_ONLY) {
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
        } else if(section->flags == FLAG_CODE) {
            //   CONTENTS, ALLOC, LOAD, RELOC, READONLY, CODE
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics | 0x60500020);
            // | IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE);
            // | IMAGE_SCN_MEM_EXECUTE);
            // | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
        } else if(section->flags == FLAG_DEBUG) {
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | IMAGE_SCN_MEM_DISCARDABLE | IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
        } else { // assume data section
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA);
        }
        // Assert(IMAGE_SCN_ALIGN_8192BYTES/IMAGE_SCN_ALIGN_1BYTES == 16);
        sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | (IMAGE_SCN_ALIGN_1BYTES * (u32)(1 + log2(section->alignment))));
    }

    // #################################
    //    SECTION DATA and RELOCATIONS
    // ##################################
    
    DynamicArray<std::unordered_map<i32, i32>> sections_dataSymbolMap;
    sections_dataSymbolMap.resize(_sections.size() + 1);

    int reloc_ptrs = 0;
    for(int si=0;si<_sections.size();si++) {
        auto& section = _sections[si];
        auto& sheader = sectionHeaders[section->number];

        obj_stream->write_align(section->alignment);

        sheader->PointerToRawData = obj_stream->getWriteHead();
        sheader->SizeOfRawData = section->stream.getWriteHead();
        // TODO: What if stream is empty or something?
        suc = obj_stream->steal_from(&section->stream);
        CHECK

        sheader->PointerToRelocations = obj_stream->getWriteHead();
        sheader->NumberOfRelocations = section->relocations.size();
        
        for(int ri=0;ri<section->relocations.size();ri++){
            auto& rel = section->relocations[ri];
            
            COFF_Relocation* coffReloc = nullptr;
            suc = obj_stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
            CHECK
            
            if(rel.type == RELOCA_PC32) {
                coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_REL32;
                coffReloc->VirtualAddress = rel.offset;

                auto& map = sections_dataSymbolMap[si];
                auto pair = map.find(rel.offsetIntoSection);
                if(pair == map.end()){
                    coffReloc->SymbolTableIndex = addSymbol(SYM_DATA, "$d"+std::to_string(map.size()), rel.sectionNr, rel.offsetIntoSection);
                    map[rel.offsetIntoSection] = coffReloc->SymbolTableIndex;
                } else {
                    coffReloc->SymbolTableIndex = pair->second;
                }
            } 
            // else if(rel.type == RELOC_PTR) {
            //     // TODO: Can we do this without a pointer? Can we write the "addend" to the data section?
            //     coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR64;
            //     coffReloc->VirtualAddress = rel.offset;
            //     coffReloc->SymbolTableIndex = addSymbol(SYM_DATA, "$d"+std::to_string(reloc_ptrs), rel.sectionNr, rel.offsetIntoSection);
            //     reloc_ptrs++;
            // }
            else {
                if(rel.type == RELOCA_REL32)
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_REL32;
                else if(rel.type == RELOCA_ADDR64)
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR64;
                else if(rel.type == RELOCA_ADDR32NB)
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR32NB;
                else if(rel.type == RELOCA_SECREL)
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECREL;
                else
                    Assert(("Missing relocation implementation",false));
                coffReloc->VirtualAddress = rel.offset;
                coffReloc->SymbolTableIndex = rel.symbolIndex;
            }
        }
    }

    // #############################
    //      SYMBOLS and STRINGS
    // #############################
    header->PointerToSymbolTable = obj_stream->getWriteHead();
    header->NumberOfSymbols = _symbols.size();
    for (int i=0;i<_symbols.size();i++) {
        auto& sym = _symbols[i];

        Symbol_Record* symbol = nullptr;
        suc = obj_stream->write_late((void**)&symbol, Symbol_Record::SIZE);
        CHECK
        
        if(sym.name.length() > 8) {
            symbol->Name.zero = 0;
            symbol->Name.offset = addString(sym.name);
        } else {
            memcpy(symbol->Name.ShortName, sym.name.c_str(), sym.name.length());
            if(sym.name.length() < 8)
                symbol->Name.ShortName[sym.name.length()] = '\0';
        }
        symbol->SectionNumber = sym.sectionNr;
        symbol->Value = sym.offset;
        if(sym.type == SYM_SECTION) {
            symbol->Value = 0; // must be zero for sections
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_STATIC;
        } else if (sym.type == SYM_ABS) {
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_STATIC;
        } else if(sym.type == SYM_FUNCTION) {
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        } else if(sym.type == SYM_DATA) {
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_STATIC;
        } else if(sym.type == SYM_LABEL) {
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_LABEL;
        } else {
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
            // Assert(false); // TODO: global variables
        }

        symbol->Type = 0; // safe to ignore?
        symbol->NumberOfAuxSymbols = 0;
    }

    suc = obj_stream->write(&_strings_offset, sizeof(u32));
    CHECK

    u32 cur_offset = 4; // string table offset starts at 4 with COFF
    for(int i=0;i<_strings.size();i++){
        auto& str = _strings[i];
        suc = obj_stream->write(str.c_str(), str.length() + 1);
        CHECK
        cur_offset += str.length() + 1;
    }
    Assert(_strings_offset == cur_offset);

    auto file = engone::FileOpen(path, engone::FILE_CLEAR_AND_WRITE);
    if(!file) {
        MSG_CODE_LOCATION;
        log::out << log::RED << "Cannot open file '"<<path<<"'\n";
        return false;
    }
    defer { engone::FileClose(file); };

    ByteStream::Iterator iter{};
    while(obj_stream->iterate(iter)) {
        bool yes = engone::FileWrite(file, (void*)iter.ptr, iter.size);
        if(!yes) {
            MSG_CODE_LOCATION;
            log::out << log::RED << "Cannot write to file '"<<path<<"'\n";
            return false;
        }
    }

    #undef CHECK
    return true;
}
bool ObjectFile::writeFile_elf(const std::string& path, ObjectFileExtraInfo* extra_info) {
    using namespace elf;
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue4);
    ByteStream stream{nullptr};
    ByteStream* obj_stream = &stream;
    u32 estimation = 0x100000;
    obj_stream->reserve(estimation);

    bool suc = true;
    #define CHECK Assert(suc);

    bool small_elf = extra_info->target == TARGET_ARM;
    int REGISTER_SIZE = extra_info->target == TARGET_ARM ? 4 : 8;
    
    #define ELF_SET(P,M,V) (small_elf ? P##32->M=V : P##64->M=V)
    #define ELF_GET(P,M) (small_elf ? P##32->M : P##64->M )
    
    /*##############
        HEADER
    ##############*/
    Elf64_Ehdr* header64 = nullptr;
    Elf32_Ehdr* header32 = nullptr;
    if (small_elf) {
        suc = obj_stream->write_late((void**)&header32, sizeof(*header32));
        CHECK
        memset(header32, 0, sizeof(*header32)); // zero-initialize header for good practise
    } else {
        suc = obj_stream->write_late((void**)&header64, sizeof(*header64));
        CHECK
        memset(header64, 0, sizeof(*header64)); // zero-initialize header for good practise
    }
    
    memset(ELF_GET(header,e_ident), 0, EI_NIDENT);
    strcpy((char*)ELF_GET(header,e_ident), "\x7f""ELF"); // IMPORTANT: I don't think the magic is correct. See the first image in cheat sheet (link in Elf.h)
    if(small_elf)
        header32->e_ident[EI_CLASS] = 1; // 0 = invalid, 1 = 32 bit objects, 2 = 64 bit objects
    else
        header64->e_ident[EI_CLASS] = 2;
    ELF_SET(header, e_ident[EI_DATA], 1); // 0 = invalid, 1 = LSB (little endian), 2 = MSB (big endian)
    ELF_SET(header, e_ident[EI_VERSION], EV_CURRENT); // should always be EV_CURRENT
    // rest of ident can be zero, elf object files from gcc have zero
    
    ELF_SET(header, e_type, ET_REL); // specifies that this is an object file and not an executable or shared/dynamic library
    if(extra_info->target == TARGET_ARM) {
        header32->e_machine = EM_ARM;
        // Processor specific flags.
        header32->e_flags = 0x500'0000; // Version5 EABI
    } else {
        header64->e_machine = EM_X86_64;
    }
    ELF_SET(header, e_version, EV_CURRENT);
    // header->e_entry = 0; // zero for no entry point
    // header->e_phoff = 0; // we don't have program headers
    // header->e_shoff = 0; // we do have this
    // header->e_flags = 0; // Processor specific flags. I think it can be zero?
    // header->e_phentsize = 0; // no need
    // header->e_phnum = 0; // program header is optional, we don't need it for object files.
    // header->e_shnum = 0; // incremented as we add sections below
    // header->e_shstrndx = 0; // index of string table section header, LATER
    if(small_elf) {
        header32->e_ehsize = sizeof(Elf32_Ehdr);
        header32->e_shentsize = sizeof(Elf32_Shdr);
    } else {
        header64->e_ehsize = sizeof(Elf64_Ehdr);
        header64->e_shentsize = sizeof(Elf64_Shdr);
    }
    
    /*##############
        SECTIONS
    ##############*/
    ELF_SET(header, e_shoff, obj_stream->getWriteHead());
    
    // Undefined/null section (SHN_UNDEF). It is mandatory or at least expected.
    if(small_elf) {
        header32->e_shnum++;
        Elf32_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
    } else {
        header64->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
    }
    
    int ind_symtab = 0; // set later, relocation sections use symtab index and are update later too
    DynamicArray<Elf32_Shdr*> reloca_sections32{};
    DynamicArray<Elf64_Shdr*> reloca_sections64{};
    DynamicArray<Elf32_Shdr*> reloc_sections32{};
    DynamicArray<Elf64_Shdr*> reloc_sections64{};
    DynamicArray<Elf32_Shdr*> sectionHeaders32{};
    DynamicArray<Elf64_Shdr*> sectionHeaders64{};
    if(small_elf) {
        reloc_sections32.resize(_sections.size() + 1);
        reloca_sections32.resize(_sections.size() + 1);
        sectionHeaders32.resize(_sections.size() + 1);
    } else {
        reloc_sections64.resize(_sections.size() + 1);
        reloca_sections64.resize(_sections.size() + 1);
        sectionHeaders64.resize(_sections.size() + 1);
    }
    
    auto has_reloc = [&](Section* s, bool with_addend) {
        for(int i=0;i<s->relocations.size();i++) {
            auto& r = s->relocations[i];
            if(with_addend == ((r.type & RELOCA_MASK) != 0)) {
                return true;
            }
        }
        return false;
    };
    
    std::unordered_map<int,int> section_number_map{};
    
    for(int i=0;i<_sections.size();i++){
        auto& section = _sections[i];
        section_number_map[section->number] = ELF_GET(header,e_shnum);
        // Assert(section->number == ELF_GET(header,e_shnum));
        
        Elf32_Shdr* sheader32 = nullptr;
        Elf64_Shdr* sheader64 = nullptr;
        if(small_elf) {
            // sheader32 = sectionHeaders32[section->number];
            ++header32->e_shnum;
            suc = obj_stream->write_late((void**)&sheader32, sizeof(*sheader32));
            CHECK
            sectionHeaders32[section->number] = sheader32;
            memset(sheader32, 0, sizeof(*sheader32));
        } else {
            // sheader64 = sectionHeaders64[section->number];
            ++header64->e_shnum;
            suc = obj_stream->write_late((void**)&sheader64, sizeof(*sheader64));
            CHECK
            sectionHeaders64[section->number] = sheader64;
            memset(sheader64, 0, sizeof(*sheader64));
        }
        
        // sheader->sh_name = 0; // set in shstrtab section further down
        if(section->name == ".ARM.attributes") {
            ELF_SET(sheader,sh_type, SHT_ARM_ATTRIBUTES);
        } else {
            ELF_SET(sheader,sh_type, SHT_PROGBITS);
        }
        ELF_SET(sheader,sh_addralign, section->alignment); // check if alignment is 2**x
        if(section->name == ".data") {
            ELF_SET(sheader,sh_type, SHT_PROGBITS);
            ELF_SET(sheader,sh_flags, SHF_ALLOC|SHF_WRITE);
        } else if(section->name == ".bss") {
            ELF_SET(sheader,sh_type, SHT_NOBITS);
            ELF_SET(sheader,sh_flags, SHF_ALLOC|SHF_WRITE);
        } else if(section->name == ".comment") {
            ELF_SET(sheader,sh_type, SHT_PROGBITS);
            ELF_SET(sheader,sh_flags, SHF_MERGE|SHF_STRINGS);
        } else {
            // sheader->sh_flags = 0; // set below
            // sheader->sh_addr = 0; // always zero for object files
            // sheader->sh_offset = 0; // place where data lies, LATER
            // sheader->sh_size = 0; // size of data, LATER
            // sheader->sh_link = 0; // not used
            // sheader->sh_info = 0; // not used
            // sheader->sh_entsize = 0;

            if(section->flags == FLAG_READ_ONLY)
                // sheader->sh_flags = 0;
                ;
            else if(section->flags == FLAG_DEBUG)
                // sheader->sh_flags = 0;
                ;
            else if(section->flags == FLAG_CODE)
                ELF_SET(sheader,sh_flags, SHF_ALLOC | SHF_EXECINSTR);
            else if(section->flags == FLAG_WRITE)
                ELF_SET(sheader,sh_flags ,SHF_ALLOC | SHF_WRITE);
        }
        
        
        if(has_reloc(section, true)) {
            Elf32_Shdr* sheader32 = nullptr;
            Elf64_Shdr* sheader64 = nullptr;
            if(small_elf) {
                suc = obj_stream->write_late((void**)&sheader32, sizeof(*sheader32));
                CHECK
                reloca_sections32[section->number] = sheader32;
                memset(sheader32, 0, sizeof(*sheader32));
                sheader32->sh_entsize = sizeof(Elf32_Rela);
            } else {
                suc = obj_stream->write_late((void**)&sheader64, sizeof(*sheader64));
                CHECK
                reloca_sections64[section->number] = sheader64;
                memset(sheader64, 0, sizeof(*sheader64));
                sheader64->sh_entsize = sizeof(Elf64_Rela);
            }
            ELF_GET(header, e_shnum++);
            
            // sheader->sh_name = 0; // set in shstrtab section further down
            ELF_SET(sheader,sh_type, SHT_RELA);
            // sheader->sh_flags = 0; // IMPORTANT: what flags?
            // sheader->sh_addr = 0; // always zero for object files
            // sheader->sh_offset = 0; // place where data lies, LATER
            // sheader->sh_size = 0; // size of data, LATER
            ELF_SET(sheader,sh_link, ind_symtab); // index to symtab section
            ELF_SET(sheader,sh_info, section_number_map[section->number]); // index to section where relocations should be applied
            ELF_SET(sheader,sh_addralign, REGISTER_SIZE);
        }
        if(has_reloc(section, false)) {
            Elf32_Shdr* sheader32 = nullptr;
            Elf64_Shdr* sheader64 = nullptr;
            if(small_elf) {
                suc = obj_stream->write_late((void**)&sheader32, sizeof(*sheader32));
                CHECK
                reloc_sections32[section->number] = sheader32;
                memset(sheader32, 0, sizeof(*sheader32));
                sheader32->sh_entsize = sizeof(Elf32_Rel);
            } else {
                suc = obj_stream->write_late((void**)&sheader64, sizeof(*sheader64));
                CHECK
                reloc_sections64[section->number] = sheader64;
                memset(sheader64, 0, sizeof(*sheader64));
                sheader64->sh_entsize = sizeof(Elf64_Rel);
            }
            ELF_GET(header, e_shnum++);
            
            // sheader->sh_name = 0; // set in shstrtab section further down
            ELF_SET(sheader,sh_type, SHT_REL);
            ELF_SET(sheader,sh_flags, SHF_INFO_LINK);
            // sheader->sh_addr = 0; // always zero for object files
            // sheader->sh_offset = 0; // place where data lies, LATER
            // sheader->sh_size = 0; // size of data, LATER
            ELF_SET(sheader,sh_link, ind_symtab); // index to symtab section
            ELF_SET(sheader,sh_info, section_number_map[section->number]); // index to section where relocations should be applied
            ELF_SET(sheader,sh_addralign, REGISTER_SIZE);
        }
    }
   
    Elf32_Shdr* s_sectionStringTable32 = nullptr;
    Elf64_Shdr* s_sectionStringTable64 = nullptr;
    { // Section name string table
        ELF_SET(header,e_shstrndx, ELF_GET(header,e_shnum));
        ELF_GET(header,e_shnum++);
        Elf32_Shdr* sheader32 = nullptr;
        Elf64_Shdr* sheader64 = nullptr;
        if (small_elf) {
            suc = obj_stream->write_late((void**)&sheader32, sizeof(*sheader32));
            CHECK
            memset(sheader32, 0, sizeof(*sheader32));
            s_sectionStringTable32 = sheader32;
        } else {
            suc = obj_stream->write_late((void**)&sheader64, sizeof(*sheader64));
            CHECK
            memset(sheader64, 0, sizeof(*sheader64));
            s_sectionStringTable64 = sheader64;
        }
        
        // sheader->sh_name = 0; // set in shstrtab section further down
        ELF_SET(sheader,sh_type, SHT_STRTAB);
        // sheader->sh_flags = 0; // should be none
        // sheader->sh_addr = 0; // always zero for object files
        // sheader->sh_offset = 0; // place where data lies, LATER
        // sheader->sh_size = 0; // size of data, LATER
        // sheader->sh_link = 0; 
        // sheader->sh_info = 0;  
        ELF_SET(sheader,sh_addralign, 1);
        // sheader->sh_entsize = 0;
    }
    
    int ind_strtab = ELF_GET(header,e_shnum);
    Elf32_Shdr* s_stringTable32 = nullptr;
    Elf64_Shdr* s_stringTable64 = nullptr;
    { // String table (for symbols)
        ELF_GET(header,e_shnum++);
        Elf32_Shdr* sheader32 = nullptr;
        Elf64_Shdr* sheader64 = nullptr;
        if(small_elf) {
            suc = obj_stream->write_late((void**)&sheader32, sizeof(*sheader32));
            CHECK
            memset(sheader32, 0, sizeof(*sheader32));
            s_stringTable32 = sheader32;
        } else {
            suc = obj_stream->write_late((void**)&sheader64, sizeof(*sheader64));
            CHECK
            memset(sheader64, 0, sizeof(*sheader64));
            s_stringTable64 = sheader64;
        }
        
        // sheader->sh_name = 0; // set in shstrtab section further down
        ELF_SET(sheader,sh_type, SHT_STRTAB);
        // sheader->sh_flags = 0; // should be none
        // sheader->sh_addr = 0; // always zero for object files
        // sheader->sh_offset = 0; // place where data lies, LATER
        // sheader->sh_size = 0; // size of data, LATER
        // sheader->sh_link = 0; // not used
        // sheader->sh_info = 0; // not used
        ELF_SET(sheader,sh_addralign,1);
        // sheader->sh_entsize = 0;
    }

    ind_symtab = ELF_GET(header,e_shnum);
    Elf32_Shdr* s_symtab32 = nullptr;
    Elf64_Shdr* s_symtab64 = nullptr;
    { // Symbol table
        ELF_GET(header,e_shnum++);
        Elf32_Shdr* sheader32 = nullptr;
        Elf64_Shdr* sheader64 = nullptr;
        if(small_elf) {
            suc = obj_stream->write_late((void**)&sheader32, sizeof(*sheader32));
            CHECK
            memset(sheader32, 0, sizeof(*sheader32));
            s_symtab32 = sheader32;
            sheader32->sh_entsize = sizeof(Elf32_Sym);
        } else {
            suc = obj_stream->write_late((void**)&sheader64, sizeof(*sheader64));
            CHECK
            memset(sheader64, 0, sizeof(*sheader64));
            s_symtab64 = sheader64;
            sheader64->sh_entsize = sizeof(Elf64_Sym);
        }
        
        // sheader->sh_name = 0; // set in shstrtab section further down
        ELF_SET(sheader,sh_type, SHT_SYMTAB);
        if(extra_info->target != TARGET_ARM) {
            ELF_SET(sheader,sh_flags, SHF_ALLOC);
        }
        // sheader->sh_addr = 0; // always zero for object files
        // sheader->sh_offset = 0; // place where data lies, LATER
        // sheader->sh_size = 0; // size of data, LATER
        ELF_SET(sheader,sh_link ,ind_strtab); // Index to string table section, names of symbols should be put there
        // sheader->sh_info = 0; // Index of first non-local symbol (or the number of local symbols). Local symbols must come first.
        ELF_SET(sheader,sh_addralign, REGISTER_SIZE);
    }
    
    #define SET_SH_LINK(SECTION_LIST) \
    for(int i=0;i<SECTION_LIST.size();i++) {\
        auto& s = SECTION_LIST[i];\
        if(s) {\
            s->sh_link = ind_symtab;\
        }\
    }
    SET_SH_LINK(reloc_sections32)
    SET_SH_LINK(reloc_sections64)
    SET_SH_LINK(reloca_sections32)
    SET_SH_LINK(reloca_sections64)

    // ###################
    //  SECTION STRING TABLE DATA
    // ####################
    {
        obj_stream->write_align(ELF_GET(s_sectionStringTable,sh_addralign));
        ELF_SET(s_sectionStringTable,sh_offset, obj_stream->getWriteHead());
        suc = obj_stream->write1('\0'); // table begins with null string
        CHECK
        
        if(small_elf) {
            Elf32_Shdr* sheader32 = s_sectionStringTable32;
            sheader32->sh_name = obj_stream->getWriteHead() - s_sectionStringTable32->sh_offset;
            suc = obj_stream->write(".shstrtab");
            CHECK
            sheader32 = s_stringTable32;
            sheader32->sh_name = obj_stream->getWriteHead() - s_sectionStringTable32->sh_offset;
            suc = obj_stream->write(".strtab");
            CHECK
            sheader32 = s_symtab32;
            sheader32->sh_name = obj_stream->getWriteHead() - s_sectionStringTable32->sh_offset;
            suc = obj_stream->write(".symtab");
            CHECK
        } else {
            Elf64_Shdr* sheader64 = s_sectionStringTable64;
            sheader64->sh_name = obj_stream->getWriteHead() - s_sectionStringTable64->sh_offset;
            suc = obj_stream->write(".shstrtab");
            CHECK
            sheader64 = s_stringTable64;
            sheader64->sh_name = obj_stream->getWriteHead() - s_sectionStringTable64->sh_offset;
            suc = obj_stream->write(".strtab");
            CHECK
            sheader64 = s_symtab64;
            sheader64->sh_name = obj_stream->getWriteHead() - s_sectionStringTable64->sh_offset;
            suc = obj_stream->write(".symtab");
            CHECK
        }
        
        for(int i=0;i<_sections.size();i++){
            auto& section = _sections[i];
            Elf32_Shdr* sheader32 = nullptr;
            Elf64_Shdr* sheader64 = nullptr;
            Elf32_Shdr* rheader32 = nullptr;
            Elf64_Shdr* rheader64 = nullptr;
            if(small_elf) {
                sheader32 = sectionHeaders32[section->number];
                sheader32->sh_name = obj_stream->getWriteHead() - s_sectionStringTable32->sh_offset;
                suc = obj_stream->write(section->name.c_str(),section->name.length() + 1);
                CHECK
                rheader32 = reloca_sections32[section->number];
                if(rheader32) {
                    rheader32->sh_name = obj_stream->getWriteHead() - s_sectionStringTable32->sh_offset;
                    // std::string nom = section->name+".rela";
                    std::string nom = ".rela"+section->name;
                    suc = obj_stream->write(nom.c_str(),nom.length() + 1);
                    CHECK
                }
                rheader32 = reloc_sections32[section->number];
                if(rheader32) {
                    rheader32->sh_name = obj_stream->getWriteHead() - s_sectionStringTable32->sh_offset;
                    // std::string nom = section->name+".rela";
                    std::string nom = ".rel"+section->name;
                    suc = obj_stream->write(nom.c_str(),nom.length() + 1);
                    CHECK
                }
            } else {
                sheader64 = sectionHeaders64[section->number];
                sheader64->sh_name = obj_stream->getWriteHead() - s_sectionStringTable64->sh_offset;
                suc = obj_stream->write(section->name.c_str(),section->name.length() + 1);
                CHECK
                rheader64 = reloca_sections64[section->number];
                if(rheader64) {
                    rheader64->sh_name = obj_stream->getWriteHead() - s_sectionStringTable64->sh_offset;
                    // std::string nom = section->name+".rela";
                    std::string nom = ".rela"+section->name;
                    suc = obj_stream->write(nom.c_str(),nom.length() + 1);
                    CHECK
                }
                rheader64 = reloc_sections64[section->number];
                if(rheader64) {
                    rheader64->sh_name = obj_stream->getWriteHead() - s_sectionStringTable64->sh_offset;
                    // std::string nom = section->name+".rela";
                    std::string nom = ".rel"+section->name;
                    suc = obj_stream->write(nom.c_str(),nom.length() + 1);
                    CHECK
                }
            }
        }
        ELF_SET(s_sectionStringTable,sh_size, obj_stream->getWriteHead() - ELF_GET(s_sectionStringTable,sh_offset));
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(ELF_GET(s_sectionStringTable,sh_offset) + ELF_GET(s_sectionStringTable,sh_size-1)) == 0);
    }
    // ###################
    //  SECTION DATA
    // ####################
    for(int si=0;si<_sections.size();si++){
        auto& section = _sections[si];
        Elf32_Shdr* sheader32 = nullptr;
        Elf64_Shdr* sheader64 = nullptr;
        if(small_elf) {
            sheader32 = sectionHeaders32[section->number];
        } else {
            sheader64 = sectionHeaders64[section->number];
        }

        obj_stream->write_align(ELF_GET(sheader,sh_addralign));
        ELF_SET(sheader,sh_offset, obj_stream->getWriteHead());

        obj_stream->steal_from(&section->stream);
        // void* ptr=nullptr;
        // obj_stream->write_late(&ptr, 8);
        
        ELF_SET(sheader,sh_size,obj_stream->getWriteHead() - ELF_GET(sheader,sh_offset));
        // engone::log::out << section->name<<" ["<<NumberToHex(sheader->sh_offset,true) << "+"<<NumberToHex(sheader->sh_offset+sheader->sh_size,true)<<", "<<NumberToHex(sheader->sh_size,true)<<"]\n";

        Elf32_Shdr* rheader32 = nullptr;
        Elf64_Shdr* rheader64 = nullptr;
        if(small_elf) {
            rheader32 = reloca_sections32[section->number];
        } else {
            rheader64 = reloca_sections64[section->number];
        }
        if(rheader32 || rheader64) {
            obj_stream->write_align(ELF_GET(rheader,sh_addralign));
            ELF_SET(rheader,sh_offset, obj_stream->getWriteHead());

            for(int ri=0;ri<section->relocations.size();ri++) {
                auto& myrel = section->relocations[ri];

                if((myrel.type & RELOCA_MASK) == 0) {
                    continue;
                }

                Elf32_Rela* rel32 = nullptr;
                Elf64_Rela* rel64 = nullptr;
                if(small_elf) {
                    suc = obj_stream->write_late((void**)&rel32, sizeof(*rel32));
                    CHECK
                    memset(rel32, 0, sizeof(*rel32));
                } else {
                    suc = obj_stream->write_late((void**)&rel64, sizeof(*rel64));
                    CHECK
                    memset(rel64, 0, sizeof(*rel64));
                }
                ELF_SET(rel,r_offset, myrel.offset);
                int rel_type = 0;
                // TODO: 64-bit ARM
                Assert(extra_info->target != TARGET_AARCH64);
                if(myrel.type == RELOCA_PC32) {
                    // int symindex = getSectionSymbol(myrel.sectionNr);
                    // rel->r_info = ELF64_R_INFO(symindex, R_X86_64_PC32);
                    if(extra_info->target == TARGET_ARM) {
                        rel_type = R_ARM_ABS32;
                    } else {
                        rel_type = R_X86_64_PC32;
                    }
                    ELF_SET(rel,r_addend, myrel.offsetIntoSection);
                    ELF_GET(rel, r_addend += -4);
                } else if (myrel.type == RELOCA_PLT32) {
                    rel_type = R_X86_64_PLT32;
                    ELF_GET(rel, r_addend = -4);
                    // rel->r_addend = -4 + myrel.addend;
                } else if(myrel.type == RELOCA_32) {
                    if(extra_info->target == TARGET_ARM) {
                        rel_type = R_ARM_ABS32;
                    } else {
                        rel_type = R_X86_64_32;
                    }
                    // rel->r_addend = 0;
                    // rel->r_addend = -4 + myrel.addend;
                } else if (myrel.type == RELOCA_64) {
                    if(extra_info->target == TARGET_ARM) {
                        rel_type = R_ARM_ABS32;
                    } else {
                        rel_type = R_X86_64_64;
                    }
                    // rel->r_addend = -8;
                    ELF_SET(rel,r_addend, myrel.addend);
                } else {
                    Assert(("Missing relocation implementation",false));
                }
                if(small_elf) {
                    ELF_SET(rel,r_info, ELF32_R_INFO(myrel.symbolIndex,rel_type));
                } else {
                    ELF_SET(rel,r_info, ELF64_R_INFO(myrel.symbolIndex,rel_type));
                }
            }
            ELF_SET(rheader,sh_size, obj_stream->getWriteHead() - ELF_GET(rheader,sh_offset));
        }
        rheader32 = nullptr;
        rheader64 = nullptr;
        if(small_elf) {
            rheader32 = reloc_sections32[section->number];
        } else {
            rheader64 = reloc_sections64[section->number];
        }
        if(rheader32 || rheader64) {
            obj_stream->write_align(ELF_GET(rheader,sh_addralign));
            ELF_SET(rheader,sh_offset, obj_stream->getWriteHead());

            for(int ri=0;ri<section->relocations.size();ri++) {
                auto& myrel = section->relocations[ri];

                if((myrel.type & RELOCA_MASK) != 0) {
                    continue;
                }

                Elf32_Rel* rel32 = nullptr;
                Elf64_Rel* rel64 = nullptr;
                if(small_elf) {
                    suc = obj_stream->write_late((void**)&rel32, sizeof(*rel32));
                    CHECK
                    memset(rel32, 0, sizeof(*rel32));
                } else {
                    suc = obj_stream->write_late((void**)&rel64, sizeof(*rel64));
                    CHECK
                    memset(rel64, 0, sizeof(*rel64));
                }
                int rel_type = 0;
                if(myrel.type == RELOC_ARM_V4BX) {
                    rel_type = R_ARM_V4BX;
                }
                if(myrel.type == RELOC_ARM_RELOC_DATA) {
                    rel_type = R_ARM_ABS32;   
                }
                ELF_SET(rel, r_offset, myrel.offset);
                if(small_elf) {
                    ELF_SET(rel,r_info, ELF32_R_INFO(myrel.symbolIndex,rel_type));
                } else {
                    ELF_SET(rel,r_info, ELF64_R_INFO(myrel.symbolIndex,rel_type));
                }
            }
            ELF_SET(rheader,sh_size, obj_stream->getWriteHead() - ELF_GET(rheader,sh_offset));
        }
    }

    {
        auto sheader32 = s_symtab32;
        auto sheader64 = s_symtab64;
        
        obj_stream->write_align(ELF_GET(sheader,sh_addralign));
        ELF_SET(sheader,sh_offset, obj_stream->getWriteHead());
        ELF_SET(sheader,sh_info, 1); // the first null symbol is local, rest is global
        
        int bind = STB_LOCAL;
        Assert(_symbols[0].type == SYM_EMPTY); // ELF wants null symbol first
        for(int i=0;i<_symbols.size();i++) {
            auto& mysym = _symbols[i];

            Elf32_Sym* sym32 = nullptr;
            Elf64_Sym* sym64 = nullptr;
            if(small_elf) {
                suc = obj_stream->write_late((void**)&sym32, sizeof(*sym32));
                CHECK
                memset(sym32, 0, sizeof(*sym32));
            } else {
                suc = obj_stream->write_late((void**)&sym64, sizeof(*sym64));
                CHECK
                memset(sym64, 0, sizeof(*sym64));
            }
            
            #define ELFX_ST_INFO(A,B) (small_elf ? ELF32_ST_INFO(A,B) : ELF64_ST_INFO(A,B))
            
            // engone::log::out << "sym " << mysym.name << "\n";
            if(mysym.type == SYM_EMPTY) {
                ELF_SET(sym,st_name, addString(mysym.name));
                // sym->st_other = 0; // always zero
                // sym->st_size = 0;
                ELF_SET(sym,st_shndx, section_number_map[mysym.sectionNr]);
                ELF_SET(sym,st_value, mysym.offset);

                ELF_SET(sym,st_info, ELFX_ST_INFO(STB_GLOBAL, STT_NOTYPE));
            } else if(mysym.type == SYM_LOCAL_NOTYPE) {
                ELF_SET(sym,st_name, addString(mysym.name));
                // sym->st_other = 0; // always zero
                // sym->st_size = 0;
                ELF_SET(sym,st_shndx, section_number_map[mysym.sectionNr]);
                ELF_SET(sym,st_value, mysym.offset);

                ELF_SET(sym,st_info, ELFX_ST_INFO(STB_LOCAL, STT_NOTYPE));
            } else {
                ELF_SET(sym,st_name, addString(mysym.name));
                // sym->st_other = 0; // always zero
                // sym->st_size = 0;
                ELF_SET(sym,st_shndx, section_number_map[mysym.sectionNr]);
                ELF_SET(sym,st_value, mysym.offset);

                if (mysym.type == SYM_SECTION){
                    ELF_SET(sym,st_info, ELFX_ST_INFO(bind, STT_SECTION));
                    if(bind == STB_LOCAL) {
                        ELF_SET(sheader,sh_info, i + 1);
                    }
                } else if (mysym.type == SYM_FILE){
                    Assert(bind == STB_LOCAL);
                    ELF_SET(sym,st_info, ELFX_ST_INFO(bind, STT_FILE));
                    if(bind == STB_LOCAL) {
                        ELF_SET(sheader,sh_info, i + 1);
                    }
                } else {
                    bind = STB_GLOBAL;
                    if(mysym.type == SYM_FUNCTION) {
                        ELF_SET(sym,st_info, ELFX_ST_INFO(STB_GLOBAL, STT_FUNC));
                        ELF_SET(sym,st_size, mysym.extra_value);
                    } else if(mysym.type == SYM_DATA) {
                        Assert(false);
                        // use PC32 relocation if possible
                        // addRelocation(section, off, section2, off2)
                        // sym->st_info = ELFX_ST_INFO(STB_GLOBAL, STT_OBJECT);
                    } else {
                        ELF_SET(sym,st_info, ELFX_ST_INFO(STB_GLOBAL, STT_NOTYPE));
                    }
                }
            }
        }
        
        ELF_SET(sheader,sh_size, obj_stream->getWriteHead() - ELF_GET(sheader,sh_offset));
    }
    {
        auto sheader32 = s_stringTable32;
        auto sheader64 = s_stringTable64;
        
        obj_stream->write_align(ELF_GET(sheader,sh_addralign));
        ELF_SET(sheader,sh_offset, obj_stream->getWriteHead());
        
        Assert(_strings[0] == ""); // ELF wants null string first
        for(int i=0;i<_strings.size();i++) {
            std::string& str = _strings[i];
            suc = obj_stream->write(str.c_str(), str.length() + 1);
            CHECK
        }
        
        ELF_SET(sheader,sh_size, obj_stream->getWriteHead() - ELF_GET(sheader,sh_offset));
        // make sure we didn't miscalculate something
        Assert(ELF_GET(sheader,sh_size) == _strings_offset);
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(ELF_GET(sheader,sh_offset) + ELF_GET(sheader,sh_size)-1) == 0);
    }

    auto file = engone::FileOpen(path, engone::FILE_CLEAR_AND_WRITE);
    defer { engone::FileClose(file); };

    if(!file) {
        MSG_CODE_LOCATION;
        log::out << log::RED << "Cannot open file '"<<path<<"'\n";
        return false;
    }
    ByteStream::Iterator iter{};
    while(obj_stream->iterate(iter)) {
        bool yes = engone::FileWrite(file, (void*)iter.ptr, iter.size);
        if(!yes) {
            MSG_CODE_LOCATION;
            log::out << log::RED << "Cannot write to file '"<<path<<"'\n";
            return false;
        }
    }

    #undef CHECK
    return true;
}

SectionNr ObjectFile::createSection(const std::string& name, ObjectFile::SectionFlags flags, u32 alignment) {
    auto ptr = TRACK_ALLOC(Section);
    new(ptr) Section(); // TODO: use allocator instead of new
    _sections.add(ptr);

    ptr->number = _sections.size();
    ptr->name = name;
    ptr->flags = flags;
    ptr->alignment = alignment;
    ptr->symbolIndex = addSymbol(SYM_SECTION, name, ptr->number, 0);

    return ptr->number;
}
SectionNr ObjectFile::findSection(const std::string& name) {
    for(int i=0;i<_sections.size();i++) {
        auto& section = _sections[i];
        if(section->name == name) {
            return section->number;
        }
    }
    return 0;
}
int ObjectFile::addSymbol(ObjectFile::SymbolType type, const std::string& name, SectionNr sectionNr, u32 offset, u32 extra_value) {
    if(type == SYM_SECTION) {
        auto& sec = _sections[sectionNr - 1];
        if(sec->symbolIndex != -1 && sec->name == name)
            return sec->symbolIndex;
    }
    int symIndex = _symbols.size();
    auto pair = _symbolMap.find(name);
    if(pair == _symbolMap.end()) {
        _symbolMap[name] = symIndex;
    } else {
        Assert(("Tried to add a symbol with a taken name",false));
        return pair->second;
    }

    _symbols.add({});
    auto& sym = _symbols.last();
    sym.type = type;
    sym.name = name;
    sym.sectionNr = sectionNr;
    sym.offset = offset;
    sym.extra_value = extra_value;
    if(type == SYM_SECTION) {
        auto& sec = _sections[sectionNr - 1];
        if(sec->name == name)
            sec->symbolIndex = symIndex;
    }

    return symIndex;
}
int ObjectFile::findSymbol(const std::string& name) {
    auto pair = _symbolMap.find(name);
    if(pair == _symbolMap.end())
        return -1;
    return pair->second;
}
int ObjectFile::getSectionSymbol(SectionNr nr) {
    return _sections[nr - 1]->symbolIndex;
}
void ObjectFile::addRelocation(SectionNr sectionNr, ObjectFile::RelocationType type, u32 offset, u32 symbolIndex, u32 addend) {
    auto& sec = _sections[sectionNr - 1];
    sec->relocations.add({});
    auto& rel = sec->relocations.last();
    rel.type = type;
    rel.offset = offset;
    rel.symbolIndex = symbolIndex;
    rel.addend = addend;
}
void ObjectFile::addRelocation_data(SectionNr sectionNr, u32 offset, SectionNr sectionNr2, u32 offset2) {
    auto& sec = _sections[sectionNr - 1];
    sec->relocations.add({});
    auto& rel = sec->relocations.last();
    rel.type = RELOCA_PC32;
    rel.offset = offset;
    rel.sectionNr = sectionNr2;
    rel.offsetIntoSection = offset2;
}
// void ObjectFile::addRelocation_ptr(SectionNr sectionNr, u32 offset, SectionNr sectionNr2, u32 offset2) {
//     auto& sec = _sections[sectionNr - 1];
//     sec->relocations.add({});
//     auto& rel = sec->relocations.last();
//     rel.type = RELOC_PTR;
//     rel.offset = offset;
//     rel.sectionNr = sectionNr2;
//     rel.offsetIntoSection = offset2;
// }
void ObjectFile::cleanup() {
    _objType = OBJ_NONE;
    _strings_offset = 0;
    _strings.cleanup();
    _symbols.cleanup();
    _symbolMap.clear();
    for(int i = 0; i < _sections.size();i++) {
        auto& section = _sections[i];
        section->cleanup();
        TRACK_FREE(section,Section);
        // delete section; // TODO: Don't use delete keyword
    }
    _sections.cleanup();
}