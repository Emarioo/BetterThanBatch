#include "BetBat/ObjectFile.h"

#include "BetBat/DWARF.h"
#include "BetBat/ELF.h"
#include "BetBat/COFF.h"

bool ObjectFile::WriteFile(ObjectFileType objType, const std::string& path, X64Program* program, Compiler* compiler, u32 from, u32 to) {
    ZoneScopedC(tracy::Color::Blue4);
    ObjectFile objectFile{};
    objectFile.init(objType);

    bool suc = false;
    #define CHECK Assert(suc);

    auto section_text = objectFile.createSection(".text", FLAG_CODE, 16);
    SectionNr section_data = -1;
    if (program->globalSize!=0){
        section_data = objectFile.createSection(".data", FLAG_NONE, 8);
    }
    if(program->debugInformation) {
        // We need to do this first because it adds more sections.
        // Sections have symbols that should be local with the ELF formats.
        // All local symbols MUST be added first (that's how elf works).
        dwarf::ProvideSections(&objectFile, program, compiler);
    }

    DynamicArray<u32> tinyprog_offsets;
    auto text_stream = objectFile.getStreamFromSection(section_text);
    if(to - from > 0) {
        Assert(from == 0 && to == -1);
        tinyprog_offsets.resize(program->tinyPrograms.size());
        {
            // main function should be first in the text section
            int i = program->index_of_main;
            auto p = program->tinyPrograms[i];
            tinyprog_offsets[i] = text_stream->getWriteHead();
            text_stream->write(p->text, p->head);
        }
        
        bool messaged = false;
        for(int i=0;i<program->tinyPrograms.size(); i++) {
            auto p = program->tinyPrograms[i];
            if(i == program->index_of_main)
                continue;

            tinyprog_offsets[i] = text_stream->getWriteHead();
            text_stream->write(p->text, p->head);

            if(text_stream->getWriteHead() > 0x4000'0000 && !messaged) {
                messaged = true;
                engone::log::out << engone::log::RED << "ABOUT TO REACH 4 GB LIMIT IN TEXT SECTION. THE COMPILER ASSUMES THAT 32-bit INTEGERS IS ENOUGH.\n";
            }
        }
        
        // Old code
        // if(to > program->size())
        //     to = program->size();
        // suc = stream->write(program->text + from, to - from);
        // CHECK
    }
    
    for(int i=0;i<program->dataRelocations.size();i++){
        auto& rel = program->dataRelocations[i];
        u32 real_offset = tinyprog_offsets[rel.tinyprog_index] + rel.textOffset;
        objectFile.addRelocation_data(section_text, real_offset, section_data, rel.dataOffset);
    }
    // for(int i=0;i<program->ptrDataRelocations.size();i++){
    //     auto& rel = program->ptrDataRelocations[i];
    //     objectFile.addRelocation_ptr(section_data, rel.referer_dataOffset, section_data, rel.target_dataOffset);
    // }

    for(int i=0;i<program->namedUndefinedRelocations.size();i++){
        auto& namedRelocation = program->namedUndefinedRelocations[i];

        int sym = objectFile.findSymbol(namedRelocation.name);
        if(sym == -1)
            sym = objectFile.addSymbol(SYM_FUNCTION, namedRelocation.name, 0, 0);

        u32 real_offset = tinyprog_offsets[namedRelocation.tinyprog_index] + namedRelocation.textOffset;
        objectFile.addRelocation(section_text, RELOCA_REL32, real_offset, sym, 0);
    }

    for(int i=0;i<program->internalFuncRelocations.size();i++){
        auto& rel = program->internalFuncRelocations[i];

        u32 from_real_offset = tinyprog_offsets[rel.from_tinyprog_index] + rel.textOffset;
        u32 to_real_offset = tinyprog_offsets[rel.to_tinyprog_index];
        
        text_stream->write_at<u32>(from_real_offset, to_real_offset - from_real_offset - 4);
    }

    for(int i=0;i<program->exportedSymbols.size();i++) {
        auto& sym = program->exportedSymbols[i];

        // Assert(false); // text offset was broken in rewrite 0.2.1, we use text offset + offset of tiny program
        // u32 real_offset = tinyprog_offsets[sym.tinyprog_index] + sym.textOffset;
        u32 real_offset = tinyprog_offsets[sym.tinyprog_index];
        objectFile.addSymbol(SYM_FUNCTION, sym.name, section_text, real_offset);
    }

    if(program->globalSize != 0) {
        auto stream = objectFile.getStreamFromSection(section_data);
        suc = stream->write(program->globalData, program->globalSize);
        CHECK

        if(program->globalSize > 8000'0000) {
            engone::log::out << engone::log::RED << "THE COMPILER CANNOT HANDLE GLOBAL DATA OF MORE THAN 2 GB (maybe 4 GB, haven't checked thoroughly)\n";
        }
    }

    // if(program->debugInformation) {
    //     log::out << "Printing debug information\n";
    //     program->debugInformation->print();
    // }

    return objectFile.writeFile(path);
}
bool ObjectFile::writeFile_coff(const std::string& path) {
    using namespace coff;
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
            | IMAGE_SCN_MEM_READ);
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

    // #######################
    //    SECTION DATA and RELOCATIONS
    // ###############3
    
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
        } else if(sym.type == SYM_FUNCTION) {
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        } else if(sym.type == SYM_DATA) {
            symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_STATIC;
        } else {
            Assert(false); // TODO: global variables
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
    Assert(file);
    ByteStream::Iterator iter{};
    while(obj_stream->iterate(iter)) {
        engone::FileWrite(file, (void*)iter.ptr, iter.size);
    }
    engone::FileClose(file);

    #undef CHECK
    return true;
}
bool ObjectFile::writeFile_elf(const std::string& path) {
    using namespace elf;
    ZoneScopedC(tracy::Color::Blue4);
    ByteStream stream{nullptr};
    ByteStream* obj_stream = &stream;
    u32 estimation = 0x100000;
    obj_stream->reserve(estimation);

    bool suc = true;
    #define CHECK Assert(suc);

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

    DynamicArray<Elf64_Shdr*> sectionHeaders{};
    sectionHeaders.resize(_sections.size() + 1);
    for(int i=0;i<_sections.size();i++){
        auto& section = _sections[i];
        auto& sheader = sectionHeaders[section->number];
        Assert(section->number == header->e_shnum);

        ++header->e_shnum;
        suc = obj_stream->write_late((void**)&sheader, sizeof(*sheader));
        CHECK
        memset(sheader, 0, sizeof(*sheader));
        
        sheader->sh_name = 0; // set in shstrtab section further down
        sheader->sh_type = SHT_PROGBITS;
        sheader->sh_flags = 0; // set below
        sheader->sh_addr = 0; // always zero for object files
        sheader->sh_offset = 0; // place where data lies, LATER
        sheader->sh_size = 0; // size of data, LATER
        sheader->sh_link = 0; // not used
        sheader->sh_info = 0; // not used
        sheader->sh_addralign = section->alignment; // check if alignment is 2**x
        sheader->sh_entsize = 0;

        if(section->flags == FLAG_READ_ONLY)
            sheader->sh_flags = 0;
        else if(section->flags == FLAG_DEBUG)
            sheader->sh_flags = 0;
        else if(section->flags == FLAG_CODE)
            sheader->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
        else
            sheader->sh_flags = SHF_ALLOC | SHF_WRITE;
    }

    Elf64_Shdr* s_sectionStringTable = nullptr;
    { // Section name string table
        header->e_shstrndx = header->e_shnum;
        header->e_shnum++;
        Elf64_Shdr* sheader = nullptr;
        suc = obj_stream->write_late((void**)&sheader, sizeof(*sheader));
        CHECK
        memset(sheader, 0, sizeof(*sheader));
        s_sectionStringTable = sheader;
        
        sheader->sh_name = 0; // set in shstrtab section further down
        sheader->sh_type = SHT_STRTAB;
        sheader->sh_flags = 0; // should be none
        sheader->sh_addr = 0; // always zero for object files
        sheader->sh_offset = 0; // place where data lies, LATER
        sheader->sh_size = 0; // size of data, LATER
        sheader->sh_link = 0; 
        sheader->sh_info = 0;  
        sheader->sh_addralign = 1;
        sheader->sh_entsize = 0;
    }
    
    int ind_strtab = header->e_shnum;
    Elf64_Shdr* s_stringTable = nullptr;
    { // String table (for symbols)
        header->e_shnum++;
        Elf64_Shdr* sheader = nullptr;
        suc = obj_stream->write_late((void**)&sheader, sizeof(*sheader));
        CHECK
        memset(sheader, 0, sizeof(*sheader));
        s_stringTable = sheader;
        
        sheader->sh_name = 0; // set in shstrtab section further down
        sheader->sh_type = SHT_STRTAB;
        sheader->sh_flags = 0; // should be none
        sheader->sh_addr = 0; // always zero for object files
        sheader->sh_offset = 0; // place where data lies, LATER
        sheader->sh_size = 0; // size of data, LATER
        sheader->sh_link = 0; // not used
        sheader->sh_info = 0; // not used
        sheader->sh_addralign = 1;
        sheader->sh_entsize = 0;
    }

    int ind_symtab = header->e_shnum;
    Elf64_Shdr* s_symtab = nullptr;
    { // Symbol table
        header->e_shnum++;
        Elf64_Shdr* sheader = nullptr;
        suc = obj_stream->write_late((void**)&sheader, sizeof(*sheader));
        CHECK
        memset(sheader, 0, sizeof(*sheader));
        s_symtab = sheader;
        
        sheader->sh_name = 0; // set in shstrtab section further down
        sheader->sh_type = SHT_SYMTAB;
        sheader->sh_flags = SHF_ALLOC;
        sheader->sh_addr = 0; // always zero for object files
        sheader->sh_offset = 0; // place where data lies, LATER
        sheader->sh_size = 0; // size of data, LATER
        sheader->sh_link = ind_strtab; // Index to string table section, names of symbols should be put there
        sheader->sh_info = 0; // Index of first non-local symbol (or the number of local symbols). Local symbols must come first.
        sheader->sh_addralign = 8;
        sheader->sh_entsize = sizeof(Elf64_Sym);
    }

    DynamicArray<Elf64_Shdr*> reloca_sections;
    reloca_sections.resize(_sections.size() + 1); // we may not use all
    for(int i=0;i<_sections.size();i++) {
        auto& section = _sections[i];
        auto& sheader = reloca_sections[section->number];
        if(section->relocations.size() == 0)  continue;

        header->e_shnum++;
        suc = obj_stream->write_late((void**)&sheader, sizeof(*sheader));
        CHECK
        memset(sheader, 0, sizeof(*sheader));
        
        sheader->sh_name = 0; // set in shstrtab section further down
        sheader->sh_type = SHT_RELA;
        sheader->sh_flags = 0; // IMPORTANT: what flags?
        sheader->sh_addr = 0; // always zero for object files
        sheader->sh_offset = 0; // place where data lies, LATER
        sheader->sh_size = 0; // size of data, LATER
        sheader->sh_link = ind_symtab; // index to symtab section
        sheader->sh_info = section->number; // index to section where relocations should be applied
        sheader->sh_addralign = 8;
        sheader->sh_entsize = sizeof(Elf64_Rela);
    }

    // DynamicArray<Elf64_Shdr*> reloc_sections;
    // reloc_sections.resize(_sections.size() + 1); // we may not use all
    // for(int i=0;i<_sections.size();i++) {
    //     auto& section = _sections[i];
    //     auto& sheader = reloc_sections[section->number];
    //     if(section->relocations.size() == 0)  continue;

    //     header->e_shnum++;
    //     suc = obj_stream->write_late((void**)&sheader, sizeof(*sheader));
    //     CHECK
    //     memset(sheader, 0, sizeof(*sheader));
        
    //     sheader->sh_name = 0; // set in shstrtab section further down
    //     sheader->sh_type = SHT_REL;
    //     sheader->sh_flags = 0; // IMPORTANT: what flags?
    //     sheader->sh_addr = 0; // always zero for object files
    //     sheader->sh_offset = 0; // place where data lies, LATER
    //     sheader->sh_size = 0; // size of data, LATER
    //     sheader->sh_link = ind_symtab; // index to symtab section
    //     sheader->sh_info = section->number; // index to section where relocations should be applied
    //     sheader->sh_addralign = 8;
    //     sheader->sh_entsize = sizeof(Elf64_Rel);
    // }

    // ###################
    //  SECTION DATA
    // ####################
    {
        obj_stream->write_align(s_sectionStringTable->sh_addralign);
        s_sectionStringTable->sh_offset = obj_stream->getWriteHead();
        suc = obj_stream->write1('\0'); // table begins with null string
        CHECK
        for(int i=0;i<_sections.size();i++){
            auto& section = _sections[i];
            auto& sheader = sectionHeaders[section->number];

            sheader->sh_name = obj_stream->getWriteHead() - s_sectionStringTable->sh_offset;
            suc = obj_stream->write(section->name.c_str(),section->name.length() + 1);
            CHECK

            Elf64_Shdr* rheader = nullptr;
            // auto rheader = reloc_sections[section->number];
            // if(rheader) {
            //     rheader->sh_name = obj_stream->getWriteHead() - s_sectionStringTable->sh_offset;
            //     std::string nom = section->name+".rel";
            //     suc = obj_stream->write(nom.c_str(),nom.length() + 1);
            //     CHECK
            // }
            rheader = reloca_sections[section->number];
            if(rheader) {
                rheader->sh_name = obj_stream->getWriteHead() - s_sectionStringTable->sh_offset;
                std::string nom = section->name+".rela";
                suc = obj_stream->write(nom.c_str(),nom.length() + 1);
                CHECK
            }
        }
        s_sectionStringTable->sh_size = obj_stream->getWriteHead() - s_sectionStringTable->sh_offset;
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(s_sectionStringTable->sh_offset + s_sectionStringTable->sh_size-1) == 0);
    }

    for(int si=0;si<_sections.size();si++){
        auto& section = _sections[si];
        auto& sheader = sectionHeaders[section->number];

        obj_stream->write_align(sheader->sh_addralign);
        sheader->sh_offset = obj_stream->getWriteHead();

        obj_stream->steal_from(&section->stream);
        // void* ptr=nullptr;
        // obj_stream->write_late(&ptr, 8);
        
        sheader->sh_size = obj_stream->getWriteHead() - sheader->sh_offset;
        // engone::log::out << section->name<<" ["<<NumberToHex(sheader->sh_offset,true) << "+"<<NumberToHex(sheader->sh_offset+sheader->sh_size,true)<<", "<<NumberToHex(sheader->sh_size,true)<<"]\n";

        Elf64_Shdr* rheader = nullptr;
        rheader = reloca_sections[section->number];
        if(rheader) {
            obj_stream->write_align(rheader->sh_addralign);
            rheader->sh_offset = obj_stream->getWriteHead();

            for(int ri=0;ri<section->relocations.size();ri++) {
                auto& myrel = section->relocations[ri];

                Elf64_Rela* rel = nullptr;
                suc = obj_stream->write_late((void**)&rel, sizeof(*rel));
                CHECK
                memset(rel, 0, sizeof(*rel));

                if(myrel.type == RELOCA_PC32) {
                    rel->r_offset = myrel.offset;
                    int symindex = getSectionSymbol(myrel.sectionNr);
                    rel->r_info = ELF64_R_INFO(symindex, R_X86_64_PC32);
                    rel->r_addend = myrel.offsetIntoSection;
                    rel->r_addend += -4;
                } else if (myrel.type == RELOCA_PLT32) {
                    rel->r_offset = myrel.offset;
                    rel->r_info = ELF64_R_INFO(myrel.symbolIndex,R_X86_64_PLT32);
                    rel->r_addend = -4;
                    // rel->r_addend = -4 + myrel.addend;
                } else if(myrel.type == RELOCA_32) {
                    rel->r_offset = myrel.offset;
                    rel->r_info = ELF64_R_INFO(myrel.symbolIndex,R_X86_64_32);
                    rel->r_addend = 0;
                    // rel->r_addend = -4 + myrel.addend;
                } else if (myrel.type == RELOCA_64) {
                    rel->r_offset = myrel.offset;
                    rel->r_info = ELF64_R_INFO(myrel.symbolIndex,R_X86_64_64);
                    // rel->r_addend = -8;
                    rel->r_addend = myrel.addend;
                } else {
                    Assert(("Missing relocation implementation",false));
                }
            }
            rheader->sh_size = obj_stream->getWriteHead() - rheader->sh_offset;
        }
        // rheader = reloc_sections[section->number];
        // if(rheader) {
        //     obj_stream->write_align(rheader->sh_addralign);
        //     rheader->sh_offset = obj_stream->getWriteHead();

        //     for(int ri=0;ri<section->relocations.size();ri++) {
        //         auto& myrel = section->relocations[ri];

        //         if(myrel.type == RELOC_PC64) {
        //             Elf64_Rel* rel = nullptr;
        //             suc = obj_stream->write_late((void**)&rel, sizeof(*rel));
        //             CHECK
        //             memset(rel, 0, sizeof(*rel));

        //             // if(myrel.type == RELOC_PC32) {
        //             //     rel->r_offset = myrel.offset;
        //             //     int symindex = getSectionSymbol(myrel.sectionNr);
        //             //     rel->r_info = ELF64_R_INFO(symindex, R_X86_64_PC32);
        //             //     rel->r_addend = myrel.offsetIntoSection;
        //             //     rel->r_addend -= 4;
        //             // } else if (myrel.type == RELOC_PLT32){
        //             //     rel->r_offset = myrel.offset;
        //             //     rel->r_info = ELF64_R_INFO(myrel.symbolIndex,R_X86_64_PLT32);
        //             //     rel->r_addend = -4;
        //             // } else 
        //             if (myrel.type == RELOC_PC64){
        //                 rel->r_offset = myrel.offset;
        //                 rel->r_info = ELF64_R_INFO(myrel.symbolIndex,R_X86_64_64);
        //                 // rel->r_info = ELF64_R_INFO(myrel.symbolIndex,R_X86_64_PC64);
        //             }
        //         }
        //     }
        //     rheader->sh_size = obj_stream->getWriteHead() - rheader->sh_offset;
        // }
    }

    {
        auto sheader = s_symtab;
        obj_stream->write_align(sheader->sh_addralign);
        sheader->sh_offset = obj_stream->getWriteHead();
        sheader->sh_info = 1; // the first null symbol is local, rest is global
        
        int bind = STB_LOCAL;
        Assert(_symbols[0].type == SYM_EMPTY); // ELF wants null symbol first
        for(int i=0;i<_symbols.size();i++) {
            auto& mysym = _symbols[i];

            Elf64_Sym* sym = nullptr;
            suc = obj_stream->write_late((void**)&sym, sizeof(*sym));
            CHECK
            memset(sym, 0, sizeof(*sym));
            
            if(mysym.type == SYM_EMPTY) {
                // memset(sym, 0, sizeof(*sym));
            } else {
                sym->st_name = addString(mysym.name);
                sym->st_other = 0; // always zero
                sym->st_size = 0;
                sym->st_shndx = mysym.sectionNr;
                sym->st_value = mysym.offset;

                if (mysym.type == SYM_SECTION){
                    sym->st_info = ELF64_ST_INFO(bind, STT_SECTION);
                    if(bind == STB_LOCAL) {
                        sheader->sh_info = i + 1;
                    }
                } else {
                    bind = STB_GLOBAL;
                    if(mysym.type == SYM_FUNCTION) {
                        sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
                    } else if(mysym.type == SYM_DATA) {
                        Assert(false);
                        // use PC32 relocation if possible
                        // addRelocation(section, off, section2, off2)
                        // sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
                    }
                }
            }
        }
        
        sheader->sh_size = obj_stream->getWriteHead() - sheader->sh_offset;
    }
    {
        auto sheader = s_stringTable;
        obj_stream->write_align(sheader->sh_addralign);
        sheader->sh_offset = obj_stream->getWriteHead();
        
        Assert(_strings[0] == ""); // ELF wants null string first
        for(int i=0;i<_strings.size();i++) {
            std::string& str = _strings[i];
            suc = obj_stream->write(str.c_str(), str.length() + 1);
            CHECK
        }
        
        sheader->sh_size = obj_stream->getWriteHead() - sheader->sh_offset;
        // make sure we didn't miscalculate something
        Assert(sheader->sh_size == _strings_offset);
        // make sure that \0 is at the end of the table
        Assert(obj_stream->read1(sheader->sh_offset + sheader->sh_size-1) == 0);
    }

    auto file = engone::FileOpen(path, engone::FILE_CLEAR_AND_WRITE);
    if(!file)  return false;
    ByteStream::Iterator iter{};
    while(obj_stream->iterate(iter)) {
        engone::FileWrite(file, (void*)iter.ptr, iter.size);
    }
    engone::FileClose(file);

    #undef CHECK
    return true;
}

SectionNr ObjectFile::createSection(const std::string& name, ObjectFile::SectionFlags flags, u32 alignment) {
    auto ptr = TRACK_ALLOC(Section);
    new(ptr) Section(); // nocheckin, use allocator instead of new
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
int ObjectFile::addSymbol(ObjectFile::SymbolType type, const std::string& name, SectionNr sectionNr, u32 offset) {
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
bool ObjectFile::writeFile(const std::string& path) {
    switch(_objType) {
    case OBJ_COFF:
        return writeFile_coff(path);
    case OBJ_ELF:
        return writeFile_elf(path);
    default:
        Assert(false);
    }
    return false;
}
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