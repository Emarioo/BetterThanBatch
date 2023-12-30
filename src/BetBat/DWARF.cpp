#include "BetBat/DWARF.h"

namespace dwarf {
    void ProvideSections(DWARFInfo* info) {
        using namespace engone;
        using namespace COFF_Format;
        Assert(info->stream && info->header && info->debug);
        
        bool suc = false;
        #define CHECK Assert(suc);
        
        {
            info->number_debug_info = ++info->header->NumberOfSections;
            Section_Header* section = nullptr;
            suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
            CHECK
        
            info->section_debug_info = section;
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
            info->number_debug_abbrev = ++info->header->NumberOfSections;
            Section_Header* section = nullptr;
            suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
            CHECK
        
            info->section_debug_abbrev = section;
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
            info->number_debug_line = ++info->header->NumberOfSections;
            Section_Header* section = nullptr;
            suc = info->stream->write_late((void**)&section, Section_Header::SIZE);
            CHECK
        
            info->section_debug_line = section;
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
    }
    
    void ProvideSectionData(DWARFInfo* info) {
        using namespace engone;
        using namespace COFF_Format;
        
        auto stream = info->stream;
        Section_Header* section = nullptr;
        auto debug = info->debug;

        // Note that X may be index++
        // X must therefore only be used once. Use a temporary variable if you need it more times.
        #define WRITE_LEB(X) \
            stream->write_unknown((void**)&ptr, 16); \
            written = ULEB128_encode(ptr, 16, X); \
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
        int abbrev_type = 0;
        int abbrev_param = 0;

        section = info->section_debug_abbrev;
        if (section){
            section->PointerToRawData = stream->getWriteHead();
            
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
            WRITE_FORM(DW_AT_decl_file,    DW_FORM_data1)
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
            WRITE_FORM(DW_AT_decl_file,    DW_FORM_data1)
            WRITE_FORM(DW_AT_decl_line,    DW_FORM_data2)
            WRITE_FORM(DW_AT_decl_column,  DW_FORM_data2)
            WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            WRITE_FORM(DW_AT_location,     DW_FORM_block1)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation

            abbrev_type = nextAbbrevCode++;
            WRITE_LEB(abbrev_type) // code
            WRITE_LEB(DW_TAG_base_type) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_byte_size,    DW_FORM_data1)
            WRITE_FORM(DW_AT_encoding,     DW_FORM_data1)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            abbrev_param = nextAbbrevCode++;
            WRITE_LEB(abbrev_param) // code
            WRITE_LEB(DW_TAG_formal_parameter) // tag
            stream->write1(DW_CHILDREN_no);

            WRITE_FORM(DW_AT_name,         DW_FORM_string)
            WRITE_FORM(DW_AT_decl_file,    DW_FORM_data1)
            WRITE_FORM(DW_AT_decl_line,    DW_FORM_data1)
            WRITE_FORM(DW_AT_decl_column,  DW_FORM_data1)
            WRITE_FORM(DW_AT_type,         DW_FORM_ref4)
            WRITE_FORM(DW_AT_location,     DW_FORM_block1)
            WRITE_LEB(0) // value
            WRITE_LEB(0) // end attributes for abbreviation
            
            WRITE_LEB(0) // zero terminate abbreviation section
            
            section->SizeOfRawData = stream->getWriteHead() - section->PointerToRawData;
        }

        section = info->section_debug_info;
        if (section){
            section->PointerToRawData = stream->getWriteHead();
            int offset_section = stream->getWriteHead();

            CompilationUnitHeader* comp = nullptr;
            stream->write_late((void**)&comp, sizeof(CompilationUnitHeader));
            // comp->unit_length = don't know yet;
            comp->version = 3;
            comp->address_size = 8;
            comp->debug_abbrev_offset = 0; // 0 since we only have one compilation unit
            
            u8* ptr = nullptr;
            int written = 0;
            
            // debugging entries

            WRITE_LEB(abbrev_compUnit) // abbrev code
            
            stream->write("BTB Compiler 0.2.0");
            // stream->write1(0); // language
            stream->write("unknown.btb"); // source file
            stream->write("project/src"); // project dir
            stream->write8(0); // start address of code
            stream->write8(0); // end address of text code
            stream->write4(0); // statement list?

            for(int i=0;i<debug->functions.size();i++) {
                auto func = debug->functions[i];

                WRITE_LEB(abbrev_func) // abbrev code
                bool global_func = false;
                WRITE_LEB(global_func) // external or not
                stream->write(func.name.c_str(), func.name.length() + 1); // name

                Assert(func.lines.size() > 0);
                int line = func.lines[0].lineNumber;

                Assert(func.fileIndex + 1 < 0x100);
                stream->write1((u8)(func.fileIndex + 1)); // file
                Assert(line < 0x10000);
                stream->write2((u16)(line)); // line
                stream->write2((u16)(0)); // column

                // Skipping 
                // nocheckin, we need to set get and set the address here later when we know where the type is.
                // stream->write4((u32)(0)); // reference to type in this section

                stream->write8((u64)(func.funcStart)); // pc low
                stream->write8((u64)(func.funcEnd)); // pc high

                stream->write1((u8)(1)); // frame base, begins with block length
                stream->write1((u8)(DW_OP_call_frame_cfa)); // block content
                // nocheckin, set sibling address
                int offset_lastSiblingData = stream->getWriteHead();
                stream->write4((u8)0); // sibling
                
                // func.lines
                // for(int j=0;j<) {
                // }

                WRITE_LEB(0) // end entry

                // nocheckin, TODO: We may need to add relocations in the section for these
                stream->write_at<u32>(offset_lastSiblingData, stream->getWriteHead() - offset_section);
            }

            // TODO: Types

            WRITE_LEB(0) // end compile unit

            section->SizeOfRawData = info->stream->getWriteHead() - section->PointerToRawData;
            comp->unit_length = info->stream->getWriteHead() - offset_section - sizeof(CompilationUnitHeader::unit_length);
        }

        section = info->section_debug_line;
        if (section){
            section->PointerToRawData = stream->getWriteHead();
            
            u8* ptr = nullptr;
            int written = 0;
            
            LineNumberProgramHeader* header = nullptr;
            stream->write_late((void**)&header, sizeof(LineNumberProgramHeader));
            // comp->unit_length = don't know yet;
            header->version = 3;
            // header->header_length = don't know yet;
            header->minimum_instruction_length = 1;
            header->default_is_stmt = 1;
            header->line_base = 0; // affects special opcodes
            header->line_range = 1; // affects special opcodes
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
                std::string dir = TrimLastFile(file);

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
                // nocheckin, we must ensure that the file can be found in include directories
                std::string no_dir = TrimDir(file);
                stream->write(no_dir.c_str());
                WRITE_LEB(1 + file_dir_indices[i]) // index into include_directories starting from 1, index as 0 represents the current working directory
                WRITE_LEB(0) // file last modified
                WRITE_LEB(0) // file size
            }
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

            // START GENERATING OPERATIONS
            int reg_file = -1;
            int reg_address = 0;
            int reg_column = 1;
            int reg_line = 1;
            WRITE_LEB(DW_LNS_set_column)
                WRITE_LEB(1) // column one for now
                reg_column = 1;

            stream->write1(0); // extended opcode uses a zero at first
            stream->write1(1 + 1); // then count of bytes for the whole extended operation
            WRITE_LEB(DW_LNE_set_address) // then opcode and data
                stream->write1(0); // noecheckin, TODO: Should this use a relocation?
                reg_address = 0;

            if(functions_in_order.size()>0) {
                auto& fun = debug->functions[functions_in_order[0]];
                reg_file = fun.fileIndex + 1;
                WRITE_LEB(DW_LNS_set_file)
                WRITE_LEB(reg_file)
            }

            WRITE_LEB(DW_LNS_copy)
            for(int fi : functions_in_order) {
                auto& fun = debug->functions[fi];
                int file_index = fun.fileIndex + 1;
                if(reg_file != file_index) {
                    reg_file = file_index;
                    WRITE_LEB(DW_LNS_set_file)
                    WRITE_LEB(reg_file)
                }

                // TODO: Set column

                int lastOffset = 0;
                int lastLine = 0;
                for(int i=0;i<fun.lines.size();i++) {
                    auto& line = fun.lines[i];
                    // Ensure that the lines are stored in ascending order
                    Assert(lastOffset <= line.funcOffset && lastLine <= line.lineNumber);
                    lastOffset = line.funcOffset;
                    lastLine = line.lineNumber;

                    WRITE_LEB(DW_LNS_advance_pc)
                    int dt = line.funcOffset - reg_address;
                    // int dt = fun.funcStart + line.funcOffset - reg_address;
                    reg_address += dt;
                    WRITE_LEB(dt)

                    WRITE_LEB(DW_LNS_advance_line)
                    dt = line.lineNumber - reg_line;
                    reg_line += dt;
                    WRITE_LEB(dt)
                }
            }

            stream->write1(0);
            stream->write1(1);
            WRITE_LEB(DW_LNE_end_sequence)


            header->header_length = (stream->getWriteHead() - section->PointerToRawData) - sizeof(LineNumberProgramHeader::unit_length) - sizeof(LineNumberProgramHeader::version);
            
            // TODO: Relocations?
            
            section->SizeOfRawData = stream->getWriteHead() - section->PointerToRawData;
            header->unit_length = section->SizeOfRawData - sizeof(LineNumberProgramHeader::unit_length);
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
    void LEB128_test() {
        using namespace engone;
        u8 buffer[32];
        int errors = 0;
        int written = 0;
        u64 fin = 0;
        
        // test isn't very good. it just encodes and decodes which doesn't mean they encode and decode properly.
        // encode and decode can both be wrong and still produce the correct result since both of them are wrong.
        #define TEST(X) written = ULEB128_encode(buffer, sizeof(buffer), X);\
            fin = ULEB128_decode(buffer, sizeof(buffer));\
            if(X != fin) { \
                errors++; \
                engone::log::out << engone::log::RED << "FAILED: "<< X << " -> "<<fin<<"\n"; \
            }
            // log::out << X <<" -> "<<fin<<"\n"; 
            // for(int i=0;i<written;i++) {
            //     log::out << " "<<i<<": "<<buffer[i]<<"\n";
            // }
        
        TEST(2)
        TEST(127)
        TEST(128)
        TEST(129)
        TEST(130)
        TEST(12857)
            
        if(errors == 0){
            engone::log::out << engone::log::GREEN << "LEB128 tests succeeded\n";
        }
    }
}