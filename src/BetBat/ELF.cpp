#include "BetBat/ELF.h"

#include "BetBat/DWARF.h"

namespace elf {
    
}

bool FileELF::WriteFile(const std::string& name, Program* program, u32 from, u32 to) {
    using namespace elf;
    Assert(program);
    Assert(false);
    #ifdef gone
    if(to==-1){
        to = program->size();
    }
    bool suc = true;
    #define CHECK Assert(suc);

    if(program->debugInformation) {
        engone::log::out << engone::log::RED << "Debug information (DWARF) has not been implemented when writing ELF files\n";
    }
    
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

    // It can be useful to change the name of the data change sometimes.
    // That way, it won't collide with other .data section when linking.
    #define SECTION_NAME_DATA ".data"
    
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
    dwarf::DWARFInfo dwarfInfo{};
    dwarfInfo.program = program;
    dwarfInfo.debug = program->debugInformation;
    
    if(program->debugInformation) {
        // dwarf::ProvideSections(&dwarfInfo, DWARF_OBJ_ELF);
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
        int bad = ADD("")
        s_sectionStringTable->sh_name = ADD(".shstrtab")
        s_stringTable->sh_name = ADD(".strtab")
        s_text->sh_name = ADD(".text")
        s_data->sh_name = ADD(SECTION_NAME_DATA)
        s_symtab->sh_name = ADD(".symtab")
        s_textrela->sh_name = ADD(".rela.text")

        if(program->debugInformation) {
            dwarfInfo.elf_section_debug_info->sh_name = ADD(".debug_info")
            dwarfInfo.elf_section_debug_abbrev->sh_name = ADD(".debug_abbrev")
            dwarfInfo.elf_section_debug_line->sh_name = ADD(".debug_line")
            dwarfInfo.elf_section_debug_aranges->sh_name = ADD(".debug_aranges")
            dwarfInfo.elf_section_debug_frame->sh_name = ADD(".debug_frame")
        }
        
        #undef ADD
        
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(section->sh_offset + section->sh_size-1) == 0);
    }
    
    u32 stringTable_offset = 1;
    DynamicArray<std::string> stringTable_strings{};
    dwarfInfo.stringTable = &stringTable_strings;
    dwarfInfo.stringTableOffset = &stringTable_offset;

    struct Symbol {
        int nameIndex; // index into string table
        int nameArrayIndex; // index into string array
        int sectionIndex;
        int value;
        int type;
        int bind;
    };
    DynamicArray<Symbol> symbols{};
    bool symbols_addedNonLocal = false;
    auto addSymbol = [&](const std::string& name, int section, int value, int bind, int type) {
        
        for(int i=0;i<symbols.size();i++) {
            Symbol& sym = symbols[i];
            if(((sym.nameArrayIndex == -1 && name.length() == 0) || (sym.nameArrayIndex != -1 && stringTable_strings[sym.nameArrayIndex] == name))
            && sym.sectionIndex == section
            && sym.value == value
            && sym.bind == bind
            && sym.type == type)
                return i;
        }

        // can't add locals after non-globals.
        // locals must be first. we can't sort because the index we return would become incorrect.
        if(bind != STB_LOCAL)
            symbols_addedNonLocal = true;
        Assert(bind != STB_LOCAL || !symbols_addedNonLocal);

        symbols.add({});
        Symbol& sym = symbols.last();
        if(name.empty()) {
            sym.nameIndex = 0;
            sym.nameArrayIndex = -1;
        } else {
            sym.nameIndex = stringTable_offset;
            sym.nameArrayIndex = stringTable_strings.size()-1;
            stringTable_strings.add(name);
            stringTable_offset += name.length() + 1;
        }
        
        sym.sectionIndex = section;
        sym.value = value;
        sym.bind = bind;
        sym.type = type;

        return (int)(symbols.size() - 1);
    };
    dwarfInfo.elf_addSymbol = addSymbol;

    // TODO: Add .text symbol
    // int sym_data = addSymbol(".text", ind_data, 0, STB_LOCAL, STT_SECTION);
    addSymbol("", 0, 0, 0, 0); // null symbol
    
    int sym_data = addSymbol(SECTION_NAME_DATA, ind_data, 0, STB_LOCAL, STT_SECTION);

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
        
        // IMPORTANT: THIS TYPE OF RELOCATION IS USED FOR call rel32!
        //  IT WON'T WORK WITH call FAR or ABSOLUTE.
        for(int i=0;i<program->namedUndefinedRelocations.size();i++) {
            X64Program::NamedUndefinedRelocation* nrel = &program->namedUndefinedRelocations[i];
            
            Elf64_Rela* rel = nullptr;
            suc = obj_stream->write_late((void**)&rel, sizeof(*rel));
            CHECK
            memset(rel, 0, sizeof(*rel));

            // int symindex = addSymbol(nrel->name, ind_text, nrel->textOffset, STB_LOCAL, STT_FUNC);
            int symindex = addSymbol(nrel->name, 0, 0, STB_GLOBAL, STT_NOTYPE);
            
            rel->r_offset = nrel->textOffset;
            rel->r_info = ELF64_R_INFO(symindex,R_X86_64_PLT32);
            rel->r_addend = -4;
        }

        for(int i=0;i<program->dataRelocations.size();i++) {
            X64Program::DataRelocation* drel = &program->dataRelocations[i];
            
            Elf64_Rela* rel = nullptr;
            suc = obj_stream->write_late((void**)&rel, sizeof(*rel));
            CHECK
            memset(rel, 0, sizeof(*rel));
            
            rel->r_offset = drel->textOffset;
            rel->r_info = ELF64_R_INFO(sym_data, R_X86_64_PC32);
            rel->r_addend = drel->dataOffset;
            // NOYE: For some reason our relocation to the data section is 4
            // bytes off. This does not happen with COFF. My solution
            // is to offset it by 4 bytes and not worry about it. (couldn't find any good explanation on the internet)
            rel->r_addend -= 4;
        }
        section->sh_size = obj_stream->getWriteHead() - section->sh_offset;
    }
    dwarfInfo.symindex_text = addSymbol(".text", ind_text, 0, STB_GLOBAL, STT_SECTION);
    if(program->debugInformation) {
        dwarfInfo.symindex_debug_info = addSymbol(".debug_info", dwarfInfo.number_debug_info, 0,STB_GLOBAL, STT_SECTION );
        dwarfInfo.symindex_debug_line = addSymbol(".debug_line", dwarfInfo.number_debug_line, 0,STB_GLOBAL, STT_SECTION );
        dwarfInfo.symindex_debug_abbrev = addSymbol(".debug_abbrev", dwarfInfo.number_debug_abbrev, 0,STB_GLOBAL, STT_SECTION );
        dwarfInfo.symindex_debug_frame = addSymbol(".debug_frame", dwarfInfo.number_debug_frame, 0,STB_GLOBAL, STT_SECTION );

        // dwarf::ProvideSectionData(&dwarfInfo, DWARF_OBJ_ELF);
    }
    
    {
        auto section = s_symtab;
        ALIGN(section->sh_addralign)
        section->sh_offset = obj_stream->getWriteHead();
        
        // first symbol consists of only zeros
        // I THOUGHT BUT I GUESS NOT?
        // Elf64_Sym* sym = nullptr;
        // suc = obj_stream->write_late((void**)&sym, sizeof(*sym));
        // CHECK
        // memset(sym, 0, sizeof(*sym));

        // int sym_main = addSymbol("main", ind_text, 0, STB_LOCAL, STT_FUNC);
        int sym_main = -1;
        for(int i=0;i<program->exportedSymbols.size();i++) {
            auto& sym = program->exportedSymbols[i];

            // TODO: We assume that all named symbols are functions. This may not be true in the future.
            if(sym.name == "main") {
                sym_main = addSymbol(sym.name, ind_text, sym.textOffset, STB_GLOBAL, STT_FUNC);
            } else if(sym.name == "WinMain") {
                // TODO: WinMain should be prioritized over main
                sym_main = addSymbol(sym.name, ind_text, sym.textOffset, STB_GLOBAL, STT_FUNC);
            } else {
                addSymbol(sym.name, ind_text, sym.textOffset, STB_GLOBAL, STT_FUNC);
            }
        }
        if(sym_main == -1) {
            sym_main = addSymbol("main", ind_text, 0, STB_GLOBAL, STT_FUNC);
        }

        section->sh_info = 0; // number of local symbols
        // Local symbols should be added first
        for(int i=0;i<symbols.size();i++){
            Symbol& symbol = symbols[i];
            if(symbol.bind != STB_LOCAL)
                continue;

            Elf64_Sym* sym = nullptr;
            suc = obj_stream->write_late((void**)&sym, sizeof(*sym));
            CHECK
            memset(sym, 0, sizeof(*sym));
            
            sym->st_name = symbol.nameIndex;
            sym->st_other = 0; // always zero
            sym->st_shndx = symbol.sectionIndex;
            sym->st_size = 0;
            sym->st_value = symbol.value;
            
            sym->st_info = ELF64_ST_INFO(symbol.bind, symbol.type);

            section->sh_info++; // increment number of local symbols
        }
        // Continue with non-local symbols
        for(int i=0;i<symbols.size();i++){
            Symbol& symbol = symbols[i];
            if(symbol.bind == STB_LOCAL)
                continue;

            Elf64_Sym* sym = nullptr;
            suc = obj_stream->write_late((void**)&sym, sizeof(*sym));
            CHECK
            memset(sym, 0, sizeof(*sym));
            
            sym->st_name = symbol.nameIndex;
            sym->st_other = 0; // always zero
            sym->st_shndx = symbol.sectionIndex;
            sym->st_size = 0;
            sym->st_value = symbol.value;
            
            sym->st_info = ELF64_ST_INFO(symbol.bind, symbol.type);
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
    
    auto file = engone::FileOpen(name, engone::FILE_CLEAR_AND_WRITE);
    if(!file)
        return false;
    suc = engone::FileWrite(file, filedata, filesize);
    CHECK
    engone::FileClose(file);
    
    #endif
    return true;
}


void FileELF::Destroy(FileELF* objectFile){
    objectFile->~FileELF();
    // engone::Free(objectFile,sizeof(FileCOFF));
    TRACK_FREE(objectFile,FileELF);
}
FileELF* FileELF::DeconstructFile(const std::string& path, bool silent) {
    using namespace engone;
    using namespace elf;
    
    u64 fileSize=0;
    auto file = FileOpen(path, FILE_READ_ONLY, &fileSize);
    if(!file)
        return nullptr;
    u8* filedata = TRACK_ARRAY_ALLOC(u8, fileSize);
    FileRead(file,filedata,fileSize);
    FileClose(file);

    FileELF* objfile = TRACK_ALLOC(FileELF);
    new(objfile)FileELF();
    objfile->_rawFileData = filedata;
    objfile->fileSize = fileSize;

    // u64 fileOffset = 0;
    
    // #define CHECK Assert(fileOffset <= fileSize);
    
    objfile->header = (Elf64_Ehdr*)(filedata);
    objfile->sections = (Elf64_Shdr*)(filedata + objfile->header->e_shoff);
    objfile->sections_count = objfile->header->e_shnum;
    
    Elf64_Shdr* sectionStringTable = objfile->sections + objfile->header->e_shstrndx;
    objfile->section_names = (char*)(filedata + sectionStringTable->sh_offset);
    objfile->section_names_size = sectionStringTable->sh_size;
    
    return objfile;
}
int FileELF::sectionIndexByName(const std::string& name) const {
    using namespace elf;
    for(int i=1;i<sections_count;i++) { // first section is empty
        Elf64_Shdr* section = sections + i;
        Assert(section->sh_name < section_names_size);
        const char* str = section_names + section->sh_name;
        if(!strcmp(str, name.c_str()))
            return i;
    }
    return 0;
}
const char* FileELF::nameOfSection(int sectionIndex) const {
    using namespace elf;
    Assert(sectionIndex < sections_count);
    
    Elf64_Shdr* section = sections + sectionIndex;
    Assert(section->sh_name < section_names_size);
    const char* str = section_names + section->sh_name;
    return str;
}
const u8* FileELF::dataOfSection(int sectionIndex, int* out_size) const {
    using namespace elf;
    Assert(sectionIndex < sections_count);
    Assert(out_size);
    
    Elf64_Shdr* section = sections + sectionIndex;
    *out_size = section->sh_size;
    return _rawFileData + section->sh_offset;
}
const elf::Elf64_Rela* FileELF::relaOfSection(int sectionIndex, int* out_count) const {
    using namespace elf;
    Assert(sectionIndex < sections_count);
    Assert(out_count);
    
    Elf64_Shdr* section = sections + sectionIndex;
    const char* str = nameOfSection(sectionIndex);
    char rela_name[100]{0};
    strcpy(rela_name, ".rela");
    Assert(strlen(str) < sizeof(rela_name) - 10); // -10 to prevent "off by one" error
    strcpy(rela_name + 5, str);
    
    for(int i=1;i<sections_count;i++) { // first section is empty
        Elf64_Shdr* rela_section = sections + i;
        const char* str = nameOfSection(i);
        if(!strcmp(rela_name, str)) {
            *out_count = section->sh_size;
            return (Elf64_Rela*)(_rawFileData + rela_section->sh_offset);
        }
    }
    *out_count = 0;
    return nullptr;
}