#include "BetBat/Elf.h"

namespace elf {
    
}

bool WriteObjectFile_elf(const std::string& name, Program_x64* program, u32 from, u32 to) {
    using namespace elf;
    Assert(program);
    if(to==-1){
        to = program->size();
    }
    bool suc = true;
    #define CHECK Assert(suc);
    
    ByteStream* obj_stream = ByteStream::Create(nullptr);
    defer {
        if(obj_stream) {
            ByteStream::Destroy(obj_stream, nullptr);
            obj_stream = nullptr;
        }
    };
    // TODO: Estimate size of obj_stream. See ObjectWriter.cpp
    
    // IMPORTANT TODO: Check alignment of sections and data of sections
    
    /*##############
        HEADER
    ##############*/
    Elf64_Ehdr* header = nullptr;
    suc = obj_stream->write_late(&header, sizeof(*header));
    CHECK
    memset(header, 0, sizeof(*header)); // zero-initialize header for good practise
    
    memset(header->e_ident, 0, EI_NIDENT);
    strcpy(header->e_ident, "\x07fELF"); // IMPORTANT: I don't think the magic is correct. See the first image in cheat sheet (link in Elf.h)
    header->e_ident[EI_CLASS] = 2; // 0 = invalid, 1 = 32 bit objects, 2 = 64 bit objects
    header->e_ident[EI_DATA] = 1; // 0 = invalid, 1 = LSB (little endian?), 2 = MSB
    header->e_ident[EI_VERSION] = 1; // should always be EV_CURRENT
    header->e_type = ET_REL; // specifies that this is an object file and not an executable or shared/dynamic library
    header->e_machine = EM_X86_64;
    header->e_version = EV_CURRENT;
    header->e_entry = 0; // zero for no entry point
    header->e_phoff = 0; // we don't have program headers
    header->e_shoff = -1; // we do have this
    header->e_flags = 0; // Processor specific flags. I think it can be zero?
    header->e_ehsize = sizeof(Elf64_Ehdr);
    header->e_phentsize = sizeof(Elf64_Phdr);
    header->e_phnum = 0; // program header is optional, we don't need it for object files.
    header->e_shentsize = sizeof(Elf64_Shdr);
    header->e_shnum = 0; // incremented as we add sections below
    header->e_shstrndx = 1; // index of string table section header
    
    /*##############
        SECTIONS
    ##############*/
    header->e_shoff = obj_stream->getWriteHead();
    
    { // Undefined/null section (SHN_UNDEF). It is mandatory or at least expected.
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late(&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
    }
    
    Elf64_Shdr* s_sectionStringTable = nullptr;
    { // Section name string table
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late(&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_sectionStringTable = section;
        
        section->sh_name = ; // set in shstrtab section further down
        section->sh_type = SHT_STRTAB;
        section->sh_flags = 0; // IMPORTANT: what flags?
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 
        section->sh_info = 
        section->sh_addralign = 1;
        section->sh_entsize = 0;
    }
    
    Elf64_Shdr* s_stringTable = nullptr;
    { // String table (for symbols)
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late(&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_stringTable = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_STRTAB;
        section->sh_flags = 0; // IMPORTANT: what flags?
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 
        section->sh_info = 
        section->sh_addralign = 1;
        section->sh_entsize = 0;
    }
    
    u32 ind_text = header->e_shnum;
    Elf64_Shdr* s_text = nullptr;
    { // Text
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late(&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_text = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_PROGBITS;
        section->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 
        section->sh_info = 
        section->sh_addralign = 1;
        section->sh_entsize = 0
    }
    
    u32 ind_data = header->e_shnum;
    Elf64_Shdr* s_data = nullptr;
    { // Data
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late(&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_data = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_PROGBITS;
        section->sh_flags = SHF_ALLOC | SHF_WRITE;
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 
        section->sh_info = 
        section->sh_addralign = 8;
        section->sh_entsize = 0;
    }
    
    Elf64_Shdr* s_symtab = nullptr;
    { // Symbol table
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late(&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_symtab = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_SYMTAB;
        section->sh_flags = 0; // IMPORTANT: what flags?
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 
        section->sh_info = 
        section->sh_addralign = 1;
        section->sh_entsize = sizeof(Elf64_Sym);
    }
    
    /*##########
        Data in sections
    ############*/
    {
        auto section = s_sectionStringTable;
        section->sh_offset = obj_stream->getWriteHead();
        
        #define ADD(X) obj_stream->getWriteHead(); suc = obj_stream->write(X); CHECK
        
        // table begins with null string
        ADD("")
        s_sectionStringTable->sh_name = ADD(".shstrtab")
        s_stringTable->sh_name = ADD(".strtab")
        s_text->sh_name = ADD(".text")
        s_data->sh_name = ADD(".data")
        s_symtab->sh_name = ADD(".symtab")
        
        #undef ADD
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(section->sh_offset + section->sh_size-1) == 0);
    }
    
    int stringTable_offset = 1;
    DynamicArray<std::string> stringTable_strings{};
    
    {
        auto section = s_text;
        section->sh_offset = obj_stream->getWriteHead();
        
        obj_stream->write(program->text, program->size());
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
    }
    
    {
        auto section = s_data;
        section->sh_offset = obj_stream->getWriteHead();
        
        obj_stream->write(program->globalData, program->globalSize);
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
    }
    
    {
        auto section = s_symtab;
        section->sh_offset = obj_stream->getWriteHead();
        
        // first symbol consists of only zeros
        Elf64_Sym* sym = nullptr;
        bool suc = obj_stream->write_late(&sym, sizeof(*sym));
        CHECK
        memset(sym, 0, sizeof(*sym));
        
        for(int i=0;i<program->namedRelocations.size();i++) {
            NamedRelocation* rel = &program->namedRelocations[i];
            
            Elf64_Sym* sym = nullptr;
            bool suc = obj_stream->write_late(&sym, sizeof(*sym));
            CHECK
            memset(sym, 0, sizeof(*sym));
            
            sym->st_name = stringTable_offset;
            stringTable_strings.add(rel->name);
            stringTable_offset += rel->name.length() + 1;
            
            sym->st_other = 0; // always zero
            sym->st_shndx = ind_text;
            sym->st_size = 0;
            sym->st_value = rel->textOffset;
            sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC); // IMPORTANT: ALL SYMBOLS SHOULD NOT BE GLOBAL NOR FUNCTIONS!
            // sym->st_info = ELF64_ST_INFO(STB_GLOBAL, ); // IMPORTANT: ALL SYMBOLS SHOULD NOT BE GLOBAL!
        }
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
    }
    
    {
        auto section = s_stringTable;
        section->sh_offset = obj_stream->getWriteHead();
        
        obj_stream->write1('\0');
        
        for(int i=0;i<stringTable_strings.size();i++) {
            std::string& str = stringTable_strings[i];
            bool suc = obj_stream->write(string.c_str(), str.length() + 1);
            CHECK
        }
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
        // make sure we didn't miscalculate something
        Assert(section->sh_size == stringTable_offset);
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(section->sh_offset + section->sh_size-1) == 0);
    }
}