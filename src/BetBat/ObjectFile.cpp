#include "BetBat/ObjectFile.h"

#include "BetBat/DWARF.h"
#include "BetBat/ELF.h"
#include "BetBat/COFF.h"

bool ObjectFile::WriteFile(ObjectFileType objType, const std::string& path, Program_x64* program, u32 from, u32 to) {
    ObjectFile objectFile{};
    objectFile.init(objType);

    auto section_text = objectFile.createSection(".text", FLAG_CODE, 16);
    SectionNr section_data = -1;
    if (program->globalSize!=0){
        section_data = objectFile.createSection(".data", FLAG_NONE, 8);
    }

    if(to - from > 0) {
        auto stream = objectFile.getStreamFromSection(section_text);
        if(to > program->size())
            to = program->size();
        stream->write(program->text + from, to - from);
    }
    
    for(int i=0;i<program->dataRelocations.size();i++){
        auto& dataRelocation = program->dataRelocations[i];
        objectFile.addRelocation(section_text, dataRelocation.textOffset, section_data, dataRelocation.dataOffset);
    }

    for(int i=0;i<program->namedUndefinedRelocations.size();i++){
        auto& namedRelocation = program->namedUndefinedRelocations[i];

        int sym = objectFile.findSymbol(namedRelocation.name);
        if(sym == -1)
            sym = objectFile.addSymbol(SYM_FUNCTION, namedRelocation.name, 0, 0);

        objectFile.addRelocation(section_text, RELOC_REL32, namedRelocation.textOffset, sym);
    }

    for(int i=0;i<program->exportedSymbols.size();i++) {
        auto& sym = program->exportedSymbols[i];
        objectFile.addSymbol(SYM_FUNCTION, sym.name, section_text, sym.textOffset);
    }

    if(program->globalSize != 0) {
        auto stream = objectFile.getStreamFromSection(section_data);
        stream->write(program->globalData, program->size());
    }


    if(program->debugInformation) {
        dwarf::ProvideSections(&objectFile, program);
    }
    // if(program->debugInformation) {
    //     log::out << "Printing debug information\n";
    //     program->debugInformation->print();
    // }

    return objectFile.writeFile(path);
}
bool ObjectFile::writeFile_coff(const std::string& path) {
    using namespace coff;
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
    sectionHeaders.resize(_sections.size());
    for(int i=0;i<_sections.size();i++){
        auto& sheader = sectionHeaders[i];
        auto& section = _sections[i];

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
            // I don't trust my understanding of strncpy
            // strncpy(sheader->Name, section->name.c_str(), sizeof(sheader->Name));
        }
        sheader->NumberOfLineNumbers = 0;
        sheader->PointerToLineNumbers = 0;
        sheader->VirtualAddress = 0;
        sheader->VirtualSize = 0;
        sheader->Characteristics = (Section_Flags)(0);
        
        if(section->flags & FLAG_READ_ONLY)
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | IMAGE_SCN_MEM_READ);
        else
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA);
        if(section->flags & FLAG_CODE)
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
        if(section->flags & FLAG_DEBUG)
            sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | IMAGE_SCN_MEM_DISCARDABLE | IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);

        // Assert(IMAGE_SCN_ALIGN_8192BYTES/IMAGE_SCN_ALIGN_1BYTES == 16);
        sheader->Characteristics = (Section_Flags)(sheader->Characteristics
            | (IMAGE_SCN_ALIGN_1BYTES * (u32)(1 + log2(section->alignment))));
    }

    // #######################
    //    SECTION DATA and RELOCATIONS
    // ###############3
    
    DynamicArray<std::unordered_map<i32, i32>> sections_dataSymbolMap;
    sections_dataSymbolMap.resize(_sections.size());

    for(int si=0;si<_sections.size();si++) {
        auto& sheader = sectionHeaders[si];
        auto& section = _sections[si];

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
            
            if(rel.type == RELOC_PC32) {
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
            } else {
                if(rel.type == RELOC_REL32)
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_REL32;
                else if(rel.type == RELOC_ADDR64)
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_ADDR64;
                else if(rel.type == RELOC_SECREL)
                    coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECREL;
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

    suc = obj_stream->write(&_strings_offset, sizeof(u32)); // nocheckin, does the offset have the right size, should it include offset integer itself?
    CHECK

    u32 cur_offset = 0;
    for(int i=0;i<_strings.size();i++){
        auto& str = _strings[i];
        suc = obj_stream->write(str.c_str(), str.length() + 1);
        CHECK
        cur_offset += str.length() + 1;
    }
    Assert(_strings_offset == cur_offset);

    auto file = engone::FileOpen(path, 0, engone::FILE_ALWAYS_CREATE);
    Assert(file);
    ByteStream::Iterator iter{};
    while(obj_stream->iterate(iter)) {
        engone::FileWrite(file, (void*)iter.ptr, iter.size);
    }
    engone::FileClose(file);

    // TODO: Return true or false?

    #undef CHECK
    return true;
}
bool ObjectFile::writeFile_elf(const std::string& path) {
    Assert(false);
    return true;
}

SectionNr ObjectFile::createSection(const std::string& name, ObjectFile::SectionFlags flags, u32 alignment) {
    auto ptr = new Section(); // nocheckin, use allocator instead of new
    _sections.add(ptr);

    ptr->number = _sections.size();
    ptr->name = name;
    ptr->flags = flags;
    ptr->alignment = alignment;
    ptr->symbolIndex = addSymbol(SYM_SECTION, name, ptr->number, 0);

    return ptr->number;
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
void ObjectFile::addRelocation(SectionNr sectionNr, ObjectFile::RelocationType type, u32 offset, u32 symbolIndex) {
    auto& sec = _sections[sectionNr - 1];
    sec->relocations.add({});
    auto& rel = sec->relocations.last();
    rel.type = type;
    rel.offset = offset;
    rel.symbolIndex = symbolIndex;
}
void ObjectFile::addRelocation(SectionNr sectionNr, u32 offset, SectionNr sectionNr2, u32 offset2) {
    auto& sec = _sections[sectionNr - 1];
    sec->relocations.add({});
    auto& rel = sec->relocations.last();
    rel.type = RELOC_PC32;
    rel.offset = offset;
    rel.sectionNr = sectionNr2;
    rel.offsetIntoSection = offset2;
}
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
        delete section; // TODO: Don't use delete keyword
    }
    _sections.cleanup();
}