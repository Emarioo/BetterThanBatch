#include "BetBat/Dwarf.h"

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
            section->Characteristics = (Section_Flags)(
                IMAGE_SCN_CNT_INITIALIZED_DATA |
                IMAGE_SCN_MEM_DISCARDABLE |
                IMAGE_SCN_ALIGN_1BYTES |
                IMAGE_SCN_MEM_READ);
        }
        
        // 1001  1000 0111  0110 0101
        
    }
    
    void ProvideSectionData(DWARFInfo* info) {
        using namespace engone;
        using namespace COFF_Format;
        
        auto stream = info->stream;
        
        #define WRITE_LEB(X) \
            stream->write_unknown((void**)&ptr, 16); \
            written = ULEB128_encode(ptr, 16, 1); \
            stream->wrote_unknown(written);
            
        
        Section_Header* section = nullptr;
        if (section = info->section_debug_info){
            section->PointerToRawData = stream->getWriteHead();
            
            CompilationUnitHeader* comp = nullptr;
            stream->write_late((void**)&comp, CompilationUnitHeader::SIZE);
            // comp->unit_length = don't know yet;
            comp->version = 3;
            comp->address_size = 8;
            comp->debug_abbrev_offset = 0; // 0 since we only have one compilation unit
            
            u8* ptr = nullptr;
            int written = 0;
            
            // debugging entries
            
            WRITE_LEB(1) // abbrev code
            
            stream->write("BTB Compiler 0.2.0");
            // stream->write1(0); // language
            stream->write("unknown.btb"); // source file
            stream->write("project/src"); // project dir
            stream->write8(0); // start address of code
            stream->write8(0); // end address of text code
            stream->write4(0); // statement list?
            // attributes
            
            section->SizeOfRawData = info->stream->getWriteHead() - section->PointerToRawData;
            comp->unit_length = section->SizeOfRawData - sizeof(CompilationUnitHeader::unit_length);
        }
        
        #define WRITE_FORM(ATTR,FORM) \
            stream->write_unknown((void**)&ptr, 16); \
            written = ULEB128_encode(ptr, 16, ATTR); \
            stream->wrote_unknown(written);\
            stream->write_unknown((void**)&ptr, 16); \
            written = ULEB128_encode(ptr, 16, FORM); \
            stream->wrote_unknown(written);
            
        if (section = info->section_debug_abbrev){
            section->PointerToRawData = stream->getWriteHead();
            
            u8* ptr = nullptr;
            int written = 0;
            
            WRITE_LEB(1) // code
            WRITE_LEB(DW_TAG_compile_unit) // tag
            
            u8 hasChildren = DW_CHILDREN_yes;
            stream->write(&hasChildren, sizeof(hasChildren));
            
            // attributes
            WRITE_FORM(DW_AT_producer,  DW_FORM_string)
            // WRITE_FORM(DW_AT_language,  DW_FORM_data1)
            WRITE_FORM(DW_AT_name,      DW_FORM_string)
            WRITE_FORM(DW_AT_comp_dir,  DW_FORM_string)
            WRITE_FORM(DW_AT_low_pc,    DW_FORM_addr)
            WRITE_FORM(DW_AT_high_pc,   DW_FORM_addr)
            WRITE_FORM(DW_AT_stmt_list, DW_FORM_data4)
            
            WRITE_LEB(0) WRITE_LEB(0) // end attributes for abbreviation
            
            // Zero termination for abbreviation section?
            
            // more abbreviations
            
            section->SizeOfRawData = stream->getWriteHead() - section->PointerToRawData;
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
            // log::out << X <<" -> "<<fin<<"\n"; \
            // for(int i=0;i<written;i++) {\
            //     log::out << " "<<i<<": "<<buffer[i]<<"\n";\
            // }\
        
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