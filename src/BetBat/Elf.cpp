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
    
    // data relocations not implemented
    Assert(program->dataRelocations.size() == 0);
    
    ByteStream* obj_stream = ByteStream::Create(nullptr);
    defer {
        if(obj_stream) {
            ByteStream::Destroy(obj_stream, nullptr);
            obj_stream = nullptr;
        }
    };
    char nul[8]{0};
    #define ALIGN(N) { Assert((N) > 0); if(obj_stream->getWriteHead() % (N) != 0) obj_stream->write(&nul, (N) - (obj_stream->getWriteHead() % (N))); }
    
    // TODO: Estimate size of obj_stream. See ObjectWriter.cpp
    
    // IMPORTANT TODO: Check alignment of sections and data of sections
    // TODO: Remove unecessary 'section->propery = 0'. sh_name, sh_addr should always be zero at first so there's no need to set them to 0 since we use memset to zero the whole section
    
    /*##############
        HEADER
    ##############*/
    Elf64_Ehdr* header = nullptr;
    suc = obj_stream->write_late((void**)&header, sizeof(*header));
    CHECK
    memset(header, 0, sizeof(*header)); // zero-initialize header for good practise
    
    memset(header->e_ident, 0, EI_NIDENT);
    strcpy((char*)header->e_ident, "\x7f""ELF"); // IMPORTANT: I don't think the magic is correct. See the first image in cheat sheet (link in Elf.h)
    header->e_ident[EI_CLASS] = 2; // 0 = invalid, 1 = 32 bit objects, 2 = 64 bit objects
    header->e_ident[EI_DATA] = 1; // 0 = invalid, 1 = LSB (little endian?), 2 = MSB
    header->e_ident[EI_VERSION] = 1; // should always be EV_CURRENT
    // rest of ident can be zero, elf object files from gcc have zero
    
    header->e_type = ET_REL; // specifies that this is an object file and not an executable or shared/dynamic library
    header->e_machine = EM_X86_64;
    header->e_version = EV_CURRENT;
    header->e_entry = 0; // zero for no entry point
    header->e_phoff = 0; // we don't have program headers
    header->e_shoff = 0; // we do have this
    header->e_flags = 0; // Processor specific flags. I think it can be zero?
    header->e_ehsize = sizeof(Elf64_Ehdr);
    header->e_phentsize = 0; // no need
    header->e_phnum = 0; // program header is optional, we don't need it for object files.
    header->e_shentsize = sizeof(Elf64_Shdr);
    header->e_shnum = 0; // incremented as we add sections below
    header->e_shstrndx = 0; // index of string table section header, LATER
    
    /*##############
        SECTIONS
    ##############*/
    header->e_shoff = obj_stream->getWriteHead();
    
    { // Undefined/null section (SHN_UNDEF). It is mandatory or at least expected.
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
    }
    
    Elf64_Shdr* s_sectionStringTable = nullptr;
    { // Section name string table
        header->e_shstrndx = header->e_shnum;
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_sectionStringTable = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_STRTAB;
        section->sh_flags = 0; // should be none
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 0; 
        section->sh_info = 0;  
        section->sh_addralign = 1;
        section->sh_entsize = 0;
    }
    
    int ind_strtab = header->e_shnum;
    Elf64_Shdr* s_stringTable = nullptr;
    { // String table (for symbols)
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_stringTable = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_STRTAB;
        section->sh_flags = 0; // should be none
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 0; // not used
        section->sh_info = 0; // not used
        section->sh_addralign = 1;
        section->sh_entsize = 0;
    }
    
    int ind_text = header->e_shnum;
    Elf64_Shdr* s_text = nullptr;
    { // Text
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_text = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_PROGBITS;
        section->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 0; // not used
        section->sh_info = 0; // not used
        section->sh_addralign = 1;
        section->sh_entsize = 0;
    }
    
    int ind_data = header->e_shnum;
    Elf64_Shdr* s_data = nullptr;
    { // Data
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_data = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_PROGBITS;
        section->sh_flags = SHF_ALLOC | SHF_WRITE;
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = 0; // not used
        section->sh_info = 0; // not used
        section->sh_addralign = 8;
        section->sh_entsize = 0;
    }
    
    int ind_symtab = header->e_shnum;
    Elf64_Shdr* s_symtab = nullptr;
    { // Symbol table
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_symtab = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_SYMTAB;
        section->sh_flags = SHF_ALLOC;
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = ind_strtab; // Index to string table section, names of symbols should be put there
        section->sh_info = 0; // Index of first non-local symbol (or the number of local symbols). Local symbols must come first.
        section->sh_addralign = 8;
        section->sh_entsize = sizeof(Elf64_Sym);
    }
    
    Elf64_Shdr* s_textrela = nullptr;
    { // Text relocations
        header->e_shnum++;
        Elf64_Shdr* section = nullptr;
        suc = obj_stream->write_late((void**)&section, sizeof(*section));
        CHECK
        memset(section, 0, sizeof(*section));
        s_textrela = section;
        
        section->sh_name = 0; // set in shstrtab section further down
        section->sh_type = SHT_RELA;
        section->sh_flags = 0; // IMPORTANT: what flags?
        section->sh_addr = 0; // always zero for object files
        section->sh_offset = 0; // place where data lies, LATER
        section->sh_size = 0; // size of data, LATER
        section->sh_link = ind_symtab; // index to symtab section
        section->sh_info = ind_text; // index to text section (or the section where relocations should be applied)
        section->sh_addralign = 8;
        section->sh_entsize = sizeof(Elf64_Rela);
    }
    
    /*##########
        Data in sections
    ############*/
    {
        auto section = s_sectionStringTable;
        ALIGN(section->sh_addralign)
        section->sh_offset = obj_stream->getWriteHead();
        
        #define ADD(X) obj_stream->getWriteHead() - section->sh_offset; suc = obj_stream->write(X); CHECK
        
        // table begins with null string
        ADD("")
        s_sectionStringTable->sh_name = ADD(".shstrtab")
        s_stringTable->sh_name = ADD(".strtab")
        s_text->sh_name = ADD(".text")
        s_data->sh_name = ADD(".data")
        s_symtab->sh_name = ADD(".symtab")
        s_textrela->sh_name = ADD(".rela.text")
        
        #undef ADD
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(section->sh_offset + section->sh_size-1) == 0);
    }
    
    int stringTable_offset = 1;
    DynamicArray<std::string> stringTable_strings{};
    
    {
        auto section = s_text;
        ALIGN(section->sh_addralign)
        section->sh_offset = obj_stream->getWriteHead();
        
        obj_stream->write(program->text, program->size());
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
    }
    
    {
        auto section = s_data;
        ALIGN(section->sh_addralign)
        section->sh_offset = obj_stream->getWriteHead();
        
        if(program->globalData && program->globalSize != 0)
            obj_stream->write(program->globalData, program->globalSize);
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
    }
    
    {
        auto section = s_textrela;
        ALIGN(section->sh_addralign)
        section->sh_offset = obj_stream->getWriteHead();
        
        for(int i=0;i<program->namedRelocations.size();i++) {
            NamedRelocation* nrel = &program->namedRelocations[i];
            
            Elf64_Rela* rel = nullptr;
            suc = obj_stream->write_late((void**)&rel, sizeof(*rel));
            CHECK
            memset(rel, 0, sizeof(*rel));
            
            // Mote that the first symbol is main. Hence +1.
            int symindex = i + 1; // IMPORTANT: This only works because we add all namedRelocations as symbols in symtab in the right order. This may not be true in the future.
            
            rel->r_offset = nrel->textOffset;
            rel->r_info = ELF64_R_INFO(symindex,R_X86_64_PLT32);
            rel->r_addend = 0;
        }
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
    }
    
    {
        auto section = s_symtab;
        ALIGN(section->sh_addralign)
        section->sh_offset = obj_stream->getWriteHead();
        
        // first symbol consists of only zeros
        // Elf64_Sym* sym = nullptr;
        // suc = obj_stream->write_late((void**)&sym, sizeof(*sym));
        // CHECK
        // memset(sym, 0, sizeof(*sym));
        
        {
            Elf64_Sym* sym = nullptr;
            suc = obj_stream->write_late((void**)&sym, sizeof(*sym));
            CHECK
            memset(sym, 0, sizeof(*sym));
            
            sym->st_name = stringTable_offset;
            std::string name = "main";
            stringTable_strings.add(name);
            stringTable_offset += name.length() + 1;
            
            // engone::log::out << "index "<<ind_text<<"\n";
            sym->st_other = 0; // always zero
            sym->st_shndx = ind_text;
            sym->st_size = 0;
            sym->st_value = 0; // 0 address meaning start of text section
            sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
        }
        
        section->sh_info = 0; // number of local symbols
        
        for(int i=0;i<program->namedRelocations.size();i++) {
            NamedRelocation* rel = &program->namedRelocations[i];
            
            Elf64_Sym* sym = nullptr;
            suc = obj_stream->write_late((void**)&sym, sizeof(*sym));
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
        ALIGN(section->sh_addralign)
        section->sh_offset = obj_stream->getWriteHead();
        
        obj_stream->write1('\0');
        
        for(int i=0;i<stringTable_strings.size();i++) {
            std::string& str = stringTable_strings[i];
            suc = obj_stream->write(str.c_str(), str.length() + 1);
            CHECK
        }
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
        // make sure we didn't miscalculate something
        Assert(section->sh_size == stringTable_offset);
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(section->sh_offset + section->sh_size-1) == 0);
    }
    
    void* filedata = nullptr;
    u32 filesize = 0;
    suc = obj_stream->finalize(&filedata, &filesize);
    CHECK
    
    auto file = engone::FileOpen(name, nullptr, engone::FILE_ALWAYS_CREATE);
    if(!file)
        return false;
    suc = engone::FileWrite(file, filedata, filesize);
    CHECK
    engone::FileClose(file);
    
    return true;
}