/*
    An abstraction for specific object file formats
*/
#pragma once

#include "BetBat/x64_gen.h"

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
        FLAG_CODE            = 0x2,
        FLAG_DEBUG           = 0x4,
        // FLAG_EMPTY, whether section has initialized data or not?
    };
    // Relocation types depend on object file format.
    // I don't have a good grasp of the simularites between
    // them so the types are rather scuffed.
    // I can't make ELF relocations work with existing values at the place of relocation.
    // It just ignores it or is off by some negative number. I DON'T UNDERSTAND.
    // So you must specify your wanted offset value at the relocation with COFF
    // but also pass it as an argument of ELF.
    enum RelocationType : u16 {
        // RELOC_PTR,

        // Windows x64, COFF
        RELOCA_ADDR64,
        RELOCA_ADDR32NB,
        RELOCA_SECREL,
        RELOCA_REL32, // used when refering to external functions

        // Unix x64, ELF
        // (RELOC = adds value to the relocation address, RELOCA = sets value + an addend to relocation address)
        RELOCA_64 = RELOCA_ADDR64,
        RELOCA_32 = RELOCA_SECREL,
        RELOCA_PLT32 = RELOCA_REL32, // used when refering to external functions
        RELOCA_PC32, // used when refering to data (in .data or .rodata)
    };
    /*
        What actions do we want relocations to do?
        - Relocation to external function
        - Relocation to data in another section (refer to .data in .text, text offset, data offset, addend)
        - Some relocation to a section SECREL?
        - Absolute relocation to some section? with some addend.
    */
    struct Relocation {
        RelocationType type;
        u32 offset = 0;
        union {
            int symbolIndex;
            SectionNr sectionNr;
        };
        union {
            u32 addend;
            u32 offsetIntoSection; // called addend in ELF
        };
    };
    struct Section {
        void cleanup() { this->~Section(); }
        SectionNr number = 0;
        std::string name;
        SectionFlags flags; // it's more of a section type
        u32 alignment;
        int symbolIndex = -1;

        ByteStream stream{ nullptr }; // TODO: Don't use heap
        DynamicArray<Relocation> relocations;
    };
    enum SymbolType : u32 {
        SYM_SECTION,
        SYM_FUNCTION,
        SYM_DATA, // data/object/value in the section?
        SYM_EMPTY,
        SYM_ABS,
    };
    struct Symbol {
        SymbolType type;
        std::string name;
        SectionNr sectionNr;
        u32 offset = 0; // offset/value
    };
    ~ObjectFile() {
        cleanup();
    }
    void cleanup();
    void init(ObjectFileType objType) {
        _objType = objType;
        if(objType == OBJ_ELF) {
            addString(""); // ELF wants an empty string first
            addSymbol(SYM_EMPTY, "", 0, 0); // ELF wants null symbol
        } else if(objType == OBJ_COFF) {
            _strings_offset = 4; // COFF includes a 4-byte string table offset integer
        }
    }

    SectionNr createSection(const std::string& name, SectionFlags flags = FLAG_NONE, u32 alignment = 8);
    SectionNr findSection(const std::string& name);
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
    void addRelocation(SectionNr sectionNr, RelocationType type, u32 offset, u32 symbolIndex, u32 addend);
    void addRelocation_data(SectionNr sectionNr, u32 offset, SectionNr sectionNr2, u32 offset2);
    // void addRelocation_ptr(SectionNr sectionNr, u32 offset, SectionNr sectionNr2, u32 offset2);
    // writes file based on objType from init
    bool writeFile(const std::string& path);

    static bool WriteFile(ObjectFileType objType, const std::string& path, X64Program* program, Compiler* compiler, u32 from = 0, u32 to = (u32)-1);

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
ObjectFile::SectionFlags operator |(ObjectFile::SectionFlags a, ObjectFile::SectionFlags b);
