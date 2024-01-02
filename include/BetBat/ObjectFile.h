/*
    An abstraction for specific object file formats
*/
#pragma once

#include "BetBat/x64_Converter.h"

enum ObjectFileType {
    OBJ_NONE,
    OBJ_ELF,
    OBJ_COFF,
};

typedef u32 SectionNr; // 0 = invalid/null
struct ObjectFile {
    enum SectionFlags : u32 {
        FLAG_NONE            = 0x0,
        FLAG_READ_ONLY       = 0x1,
        FLAG_CODE            = 0x2 | FLAG_READ_ONLY,
        FLAG_DEBUG           = 0x4 | FLAG_READ_ONLY,
        // FLAG_EMPTY, whether section has initialized data or not?
    };
    enum RelocationType : u16 {
        RELOC_ADDR64,
        RELOC_SECREL,
        RELOC_REL32,

        RELOC_PC32,
        RELOC_PLT32,
        RELOC_32,
        RELOC_64,
    };
    struct Relocation {
        RelocationType type;
        u32 offset = 0;
        union {
            int symbolIndex;
            SectionNr sectionNr;
        };
        u32 offsetIntoSection = 0; // called addend in ELF
    };
    struct Section {
        SectionNr number = 0;
        std::string name;
        u32 flags;
        u32 alignment;
        int symbolIndex = -1;

        ByteStream stream{ nullptr }; // TODO: Don't use heap
        DynamicArray<Relocation> relocations;
    };
    enum SymbolType : u32 {
        SYM_SECTION,
        SYM_FUNCTION,
        SYM_DATA, // data/object/value in the section?
    };
    struct Symbol {
        SymbolType type;
        std::string name;
        SectionNr sectionNr;
        u32 offset = 0; // offset/value
    };
    
    void cleanup();
    void init(ObjectFileType objType) {
        _objType = objType;
        if(objType == OBJ_ELF)
            addString(""); // ELF wants an empty string first
        else if(objType == OBJ_COFF)
            _strings_offset = 4; // COFF includes a 4-byte string table offset integer
    }

    SectionNr createSection(const std::string& name, SectionFlags flags = FLAG_NONE, u32 alignment = 8);
    // void deleteSection(int index); // deleteSection might be a bad idea if you use the number of the section somewhere and then delete it.
    ByteStream* getStreamFromSection(SectionNr nr) {
        return &_sections[nr - 1]->stream;
    }
    // returns -1 if there is no symbol
    int getSectionSymbol(SectionNr nr);
    // adds string to string table, returns the offset into the table
    u32 addString(const std::string& string) {
        u32 off = _strings_offset;
        _strings.add(string);
        _strings_offset += string.length() + 1;
        return off;
    }
    // returns index of symbol
    int addSymbol(SymbolType type, const std::string& name, SectionNr sectionNumber, u32 offset);
    // returns -1 if not found
    int findSymbol(const std::string& name);
    void addRelocation(SectionNr sectionNr, RelocationType type, u32 offset, u32 symbolIndex);
    void addRelocation(SectionNr sectionNr, u32 offset, SectionNr sectionNr2, u32 offset2);
    // writes file based on objType from init
    bool writeFile(const std::string& path);

    static bool WriteFile(ObjectFileType objType, const std::string& path, Program_x64* program, u32 from = 0, u32 to = (u32)-1);

private:
    // ELF and COFF has some trickery with the string table which can't just be
    // handled at the end. ELF needs an empty string at first and COFF includes
    // a 4-byte offset for all strings. This must be handled in the init function
    // and if an ObjectFile has been initialized with ELF, you can't just write 
    // COFF. Not easily at least. There may be other cases which make it hard to just switch between COFF and ELF.

    bool writeFile_coff(const std::string& path);
    bool writeFile_elf(const std::string& path);

    DynamicArray<Section*> _sections;
    DynamicArray<std::string> _strings; // for string table
    u32 _strings_offset = 0;
    DynamicArray<Symbol> _symbols;
    std::unordered_map<std::string, int> _symbolMap;
    ObjectFileType _objType = OBJ_NONE;
};