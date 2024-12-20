#include "BetBat/COFF.h"
#include "Engone/Logger.h"
#include "BetBat/Util/Utility.h"

#include "BetBat/PDB.h"

#include <time.h>

// included for the Path struct
#include "BetBat/Compiler.h"
#include "BetBat/DWARF.h"

namespace coff {
    
    #define SWITCH_START(TYPE) const char* ToString(TYPE flags);\
        engone::Logger& operator<<(engone::Logger& logger, TYPE flags){return logger << ToString(flags);}\
        const char* ToString(TYPE flags){ switch(flags){
    #define CASE(X) case X: return #X;
    #define SWITCH_END default: { }} return "UNKNOWN"; }
    // #define SWITCH_END default: {Assert(false);}} return "Undefined machine"; }
    
    #define LOG_START(TYPE) engone::Logger& operator<<(engone::Logger& logger, TYPE flags){ std::string out="";
    #define IFOR(X) if(flags&X) out += #X ", ";
    #define LOG_END if(out.empty()) return logger << "No flags?"; else return logger << out;}

    SWITCH_START(Machine_Flags)
        CASE(IMAGE_FILE_MACHINE_AMD64)
    SWITCH_END
    LOG_START(COFF_Header_Flags)
        IFOR(IMAGE_FILE_RELOCS_STRIPPED)
        IFOR(IMAGE_FILE_EXECUTABLE_IMAGE)
        IFOR(IMAGE_FILE_LINE_NUMS_STRIPPED)
        IFOR(IMAGE_FILE_LOCAL_SYMS_STRIPPED)
        IFOR(IMAGE_FILE_AGGRESSIVE_WS_TRIM)
        IFOR(IMAGE_FILE_LARGE_ADDRESS_AWARE)
        IFOR(IMAGE_FILE_BYTES_REVERSED_LO)
        IFOR(IMAGE_FILE_32BIT_MACHINE)
        IFOR(IMAGE_FILE_DEBUG_STRIPPED)
        IFOR(IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP)
        IFOR(IMAGE_FILE_NET_RUN_FROM_SWAP)
        IFOR(IMAGE_FILE_SYSTEM)
        IFOR(IMAGE_FILE_DLL)
        IFOR(IMAGE_FILE_UP_SYSTEM_ONLY)
        IFOR(IMAGE_FILE_BYTES_REVERSED_HI)
    LOG_END
    LOG_START(Section_Flags)
        IFOR(IMAGE_SCN_CNT_CODE)
        IFOR(IMAGE_SCN_CNT_INITIALIZED_DATA)
        IFOR(IMAGE_SCN_CNT_UNINITIALIZED_DATA)
        IFOR(IMAGE_SCN_LNK_OTHER)
        IFOR(IMAGE_SCN_LNK_INFO)
        IFOR(IMAGE_SCN_LNK_REMOVE)
        IFOR(IMAGE_SCN_LNK_COMDAT)
        IFOR(IMAGE_SCN_ALIGN_1BYTES)
        IFOR(IMAGE_SCN_ALIGN_2BYTES)
        IFOR(IMAGE_SCN_ALIGN_4BYTES)
        IFOR(IMAGE_SCN_ALIGN_8BYTES)
        IFOR(IMAGE_SCN_ALIGN_16BYTES)
        IFOR(IMAGE_SCN_LNK_NRELOC_OVFL)
        IFOR(IMAGE_SCN_MEM_DISCARDABLE)
        IFOR(IMAGE_SCN_MEM_NOT_CACHED)
        IFOR(IMAGE_SCN_MEM_NOT_PAGED)
        IFOR(IMAGE_SCN_MEM_SHARED)
        IFOR(IMAGE_SCN_MEM_EXECUTE)
        IFOR(IMAGE_SCN_MEM_READ)
        IFOR(IMAGE_SCN_MEM_WRITE)
    LOG_END
    SWITCH_START(Type_Indicator)
        CASE(IMAGE_REL_AMD64_ABSOLUTE)
        CASE(IMAGE_REL_AMD64_ADDR64)
        CASE(IMAGE_REL_AMD64_ADDR32)
        CASE(IMAGE_REL_AMD64_ADDR32NB)
        CASE(IMAGE_REL_AMD64_REL32)
        CASE(IMAGE_REL_AMD64_REL32_1)
        CASE(IMAGE_REL_AMD64_REL32_2)
        CASE(IMAGE_REL_AMD64_REL32_3)
        CASE(IMAGE_REL_AMD64_REL32_4)
        CASE(IMAGE_REL_AMD64_REL32_5)
        CASE(IMAGE_REL_AMD64_SECTION)
        CASE(IMAGE_REL_AMD64_SECREL)
        CASE(IMAGE_REL_AMD64_SECREL7)
        CASE(IMAGE_REL_AMD64_TOKEN)
        CASE(IMAGE_REL_AMD64_SREL32)
        CASE(IMAGE_REL_AMD64_PAIR)
        CASE(IMAGE_REL_AMD64_SSPAN32)
        // case 17: return "R_X86_64_32S"; // I got this from compiling assembly with as and doing objdump -r on .o file, This was probably a bug
    SWITCH_END

    SWITCH_START(Storage_Class)
        CASE(IMAGE_SYM_CLASS_END_OF_FUNCTION)
        CASE(IMAGE_SYM_CLASS_NULL)
        CASE(IMAGE_SYM_CLASS_AUTOMATIC)
        CASE(IMAGE_SYM_CLASS_EXTERNAL)
        CASE(IMAGE_SYM_CLASS_STATIC)
        CASE(IMAGE_SYM_CLASS_REGISTER)
        CASE(IMAGE_SYM_CLASS_EXTERNAL_DEF)
        CASE(IMAGE_SYM_CLASS_LABEL)
        CASE(IMAGE_SYM_CLASS_UNDEFINED_LABEL)
        CASE(IMAGE_SYM_CLASS_MEMBER_OF_STRUCT)
        CASE(IMAGE_SYM_CLASS_ARGUMENT)
        CASE(IMAGE_SYM_CLASS_STRUCT_TAG)
        CASE(IMAGE_SYM_CLASS_MEMBER_OF_UNION)
        CASE(IMAGE_SYM_CLASS_UNION_TAG)
        CASE(IMAGE_SYM_CLASS_TYPE_DEFINITION)
        CASE(IMAGE_SYM_CLASS_UNDEFINED_STATIC)
        CASE(IMAGE_SYM_CLASS_ENUM_TAG)
        CASE(IMAGE_SYM_CLASS_MEMBER_OF_ENUM)
        CASE(IMAGE_SYM_CLASS_REGISTER_PARAM)
        CASE(IMAGE_SYM_CLASS_BIT_FIELD)
        CASE(IMAGE_SYM_CLASS_BLOCK)
        CASE(IMAGE_SYM_CLASS_FUNCTION)
        CASE(IMAGE_SYM_CLASS_END_OF_STRUCT)
        CASE(IMAGE_SYM_CLASS_FILE)
        CASE(IMAGE_SYM_CLASS_SECTION)
        CASE(IMAGE_SYM_CLASS_WEAK_EXTERNAL)
        CASE(IMAGE_SYM_CLASS_CLR_TOKEN)
    SWITCH_END

    #undef CASE
    #undef SWITCH_START
    #undef SWITCH_END
    #undef LOG_START
    #undef IFOR
    #undef LOG_END 
}

void FileCOFF::Destroy(FileCOFF* objectFile){
    objectFile->~FileCOFF();
    // engone::Free(objectFile,sizeof(FileCOFF));
    TRACK_FREE(objectFile,FileCOFF);
}
FileCOFF* FileCOFF::DeconstructFile(const std::string& path, bool silent) {
    using namespace engone;
    using namespace coff;
    u64 fileSize=0;
    // std::string filename = "obj_test.o";
    // std::string filename = "obj_test.obj";
    auto file = FileOpen(path,FILE_READ_ONLY,&fileSize);
    if(!file)
        return nullptr;
    u8* filedata = TRACK_ARRAY_ALLOC(u8, fileSize);

    FileRead(file,filedata,fileSize);
    FileClose(file);
    if(!silent)
        log::out << "Read file "<<path << ", size: "<<fileSize<<"\n";

    FileCOFF* objectFile = TRACK_ALLOC(FileCOFF);
    new(objectFile)FileCOFF();
    objectFile->_rawFileData = filedata;
    objectFile->fileSize = fileSize;

    u64 fileOffset = 0;

    if(0x3c + 4 < fileSize) {
        u32 signature_offset = *(u32*)(filedata + 0x3c);
        if(signature_offset + 4 < fileSize) {
            u32 signature = *(u32*)(filedata + signature_offset);
            if(signature == 0x00004550) {
                if(!silent)
                    log::out << log::YELLOW << "WARNING: "<<path<<" is an executable which the deconstruct function can't fully handle.\n";
                // "PE\0\0"
                fileOffset += signature_offset + 4;
            }
        }
    }

    Assert(fileSize-fileOffset>=COFF_File_Header::SIZE);
    COFF_File_Header* coffHeader = (COFF_File_Header*)(filedata + fileOffset);
    fileOffset += COFF_File_Header::SIZE;

    objectFile->header = coffHeader;

    if(!silent){
        #define LOGIT(X) log::out << " "#X": "<< baselog->X<<"\n";
        #define STRIT(X) #X": "<< baselog->X<<"\n";
        {
            auto baselog = coffHeader;
            log::out << log::LIME << "COFF HEADER\n";
            LOGIT(Machine)
            LOGIT(NumberOfSections)
            LOGIT(TimeDateStamp)
            LOGIT(PointerToSymbolTable)
            LOGIT(NumberOfSymbols)
            LOGIT(SizeOfOptionalHeader)
            LOGIT(Characteristics)
        }
    }

    fileOffset += coffHeader->SizeOfOptionalHeader;
    if(coffHeader->SizeOfOptionalHeader!=0){
        if(!silent)
            log::out << log::YELLOW<<"Ignoring optional header\n";
    }
    u64 stringTableOffset = coffHeader->PointerToSymbolTable+coffHeader->NumberOfSymbols*Symbol_Record::SIZE;
    Assert(fileSize>=stringTableOffset+sizeof(u32));
    char* stringTablePointer = nullptr;
    u32 stringTableSize = 0;

    if(coffHeader->PointerToSymbolTable != 0) {
        stringTablePointer = (char*)filedata + stringTableOffset;
        stringTableSize = *(u32*)(stringTablePointer);
        
        objectFile->stringTableSize = stringTableSize;
        objectFile->stringTableData = stringTablePointer;
    }

    Section_Header* sectionHeaders_start = (Section_Header*)(filedata + fileOffset);
    #define GET_SECTION(I) (Section_Header*)((u8*)sectionHeaders_start + (I) * Section_Header::SIZE)

    auto log_section_name = [&](Section_Header* section) {
        auto baselog = section;
        if(baselog->Name[0]=='/'){
            u32 offset = 0;
            for(int i=1;i<8;i++) {
                if(baselog->Name[i]==0)
                    break;
                Assert(baselog->Name[i]>='0'&&baselog->Name[i]<='9');
                offset = offset*10 + baselog->Name[i] - '0';
            }
            
            if(!silent)
                log::out <<((char*)stringTablePointer + offset);
        } else {
            for(int j=0;j<8;j++) {
                if(0==baselog->Name[j])
                    break; 
                if(!silent)
                    log::out << baselog->Name[j];
            }
        }
    };
    
    auto log_symbol_name = [&](Symbol_Record* symbol) {
        auto baselog = symbol;
        if(baselog->Name.zero==0){
            log::out << ((char*)stringTablePointer + baselog->Name.offset);
        } else {
            for(int i=0;i<8;i++) {
                if(0==baselog->Name.ShortName[i])
                    break;
                log::out << baselog->Name.ShortName[i];
            }
        }
    };

    for(int i=0;i<coffHeader->NumberOfSections;i++){
        Assert(fileSize-fileOffset>=Section_Header::SIZE);
        Section_Header* sectionHeader = (Section_Header*)(filedata + fileOffset);
        fileOffset += Section_Header::SIZE;

        objectFile->sections.add(sectionHeader);

        {
            auto baselog = sectionHeader;
            // LOGIT(Name)
            if(!silent) {
                log::out << log::LIME<<"SECTION HEADER: ";
                log_section_name(baselog);
                log::out << "\n";
            }

            if(!silent) {
                LOGIT(VirtualSize)
                LOGIT(VirtualAddress)
                LOGIT(SizeOfRawData)
                LOGIT(PointerToRawData)
                LOGIT(PointerToRelocations)
                LOGIT(PointerToLineNumbers)
                LOGIT(NumberOfRelocations)
                LOGIT(NumberOfLineNumbers)
                LOGIT(Characteristics)
            }
        }
        if(objectFile->getSectionName(i) == ".debug$S") {
            u8* data = objectFile->_rawFileData + sectionHeader->PointerToRawData;
            DeconstructDebugSymbols(data, sectionHeader->SizeOfRawData);
        }
        if(objectFile->getSectionName(i) == ".debug$T") {
            u8* data = objectFile->_rawFileData + sectionHeader->PointerToRawData;
            // DeconstructDebugTypes(data, sectionHeader->SizeOfRawData);
        }
        if(objectFile->getSectionName(i) == ".pdata") {
            u8* data = objectFile->_rawFileData + sectionHeader->PointerToRawData;
            DeconstructPData(data, sectionHeader->SizeOfRawData);
        }
        if(objectFile->getSectionName(i) == ".xdata") {
            u8* data = objectFile->_rawFileData + sectionHeader->PointerToRawData;
            DeconstructXData(data, sectionHeader->SizeOfRawData);
        }
        bool is_text = false;
        if (objectFile->getSectionName(i).compare(0, 5, ".text") == 0) {
          is_text = true;
        //   u8 *data = objectFile->_rawFileData + sectionHeader->PointerToRawData;
        }
        for(int i=0;i<sectionHeader->NumberOfRelocations;i++){
            COFF_Relocation* relocation = (COFF_Relocation*)(objectFile->_rawFileData + sectionHeader->PointerToRelocations + i*COFF_Relocation::SIZE);
            
            if(is_text) {
                objectFile->text_relocations.add(relocation);
            }
            if(!silent) {
                auto baselog = relocation;
                log::out << log::LIME << " relocation "<<i<<"\n";
                log::out << "  " STRIT(VirtualAddress)
                log::out << "  " STRIT(SymbolTableIndex)
                log::out << "  " STRIT(Type)
                
                u32 symbolOffset = coffHeader->PointerToSymbolTable + baselog->SymbolTableIndex * Symbol_Record::SIZE;
                Symbol_Record* symbolRecord = (Symbol_Record*)(filedata + symbolOffset);

                {
                    log::out << log::GOLD <<"  Symbol info\n";
                    auto baselog = symbolRecord;
                    log::out << "   Name: ";
                    log_symbol_name(baselog);
                    log::out << "\n";
                    log::out << "   " STRIT(Value)
                    log::out << "   SectionNumber: "<< baselog->SectionNumber;
                    log::out.flush();
                    if(baselog->SectionNumber > 0) {
                        log::out << " (";
                        auto section = GET_SECTION(baselog->SectionNumber-1);
                        log_section_name(section);
                        log::out << ")\n";
                    } else{
                        log::out << "\n";
                    }

                    log::out << "   " STRIT(Type)
                    log::out << "   " STRIT(StorageClass)
                    log::out << "   " STRIT(NumberOfAuxSymbols)
                }
            }
        }
    }
    if(!silent)
        log::out << log::LIME<<"Symbol table\n";
    int skipAux = 0;
    Storage_Class previousStorageClass = (Storage_Class)0;
    for(int i=0;i<coffHeader->NumberOfSymbols;i++){
        u32 symbolOffset = coffHeader->PointerToSymbolTable + i * Symbol_Record::SIZE;
        Assert(fileSize>=symbolOffset+Symbol_Record::SIZE);
        Symbol_Record* symbolRecord = (Symbol_Record*)(filedata + symbolOffset);

        objectFile->symbols.add(symbolRecord);
        if(skipAux==0){
            if(!silent) {
                log::out << log::LIME << "Symbol "<<i<<"\n";
                auto baselog = symbolRecord;
                log::out << " Name: ";
                log_symbol_name(baselog);
                log::out << "\n";
                LOGIT(Value)
                log::out << " SectionNumber: " << baselog->SectionNumber;
                log::out.flush();
                if(baselog->SectionNumber > 0) {
                    log::out << " (";
                    auto section = GET_SECTION(baselog->SectionNumber-1);
                    log_section_name(section);
                    log::out << ")\n";
                } else {
                    log::out << "\n";
                }
                LOGIT(Type)
                LOGIT(StorageClass)
                LOGIT(NumberOfAuxSymbols)
            }
            previousStorageClass = symbolRecord->StorageClass;
            skipAux+=symbolRecord->NumberOfAuxSymbols; // skip them for now
        } else {
            if (previousStorageClass==IMAGE_SYM_CLASS_STATIC){
                auto baselog = (Aux_Format_5*)symbolRecord;
                if(!silent){
                    log::out << log::LIME << " Auxilary Format 5 (index "<<i<<")\n";
                    log::out << "  " STRIT(Length)
                    log::out << "  " STRIT(NumberOfRelocations)
                    log::out << "  " STRIT(NumberOfLineNumbers)
                    log::out << "  " STRIT(CheckSum)
                    log::out << "  " STRIT(Number)
                    log::out << "  " STRIT(Selection)
                }
            }
            skipAux--;
        }
    }
    if(!silent){
        log::out << log::LIME<<"String table ";
        log::out << log::LIME<<" ("<<stringTableSize<<" bytes)\n";
        for(int i=4;i<stringTableSize;i++){
            if(*(u8*)(stringTablePointer+i)==0)
                log::out << " ";
            else
                log::out << *(char*)(stringTablePointer+i);
        }
        if(stringTableSize > 4)
            log::out << "\n";
    }
    // $unwind$main $pdata$m
    // log::out << (stringTableOffset + stringTableSize) <<"\n";
    #undef LOGIT

    return objectFile;
}

std::string FileCOFF::getSectionName(int sectionIndex){
    Assert(sections.size()>sectionIndex);
    auto* section = sections[sectionIndex];

    std::string name="";
    if(section->Name[0]=='/'){
        u32 offset = 0;
        for(int i=1;8;i++) {
            if(section->Name[i]==0)
                break;
            Assert(section->Name[i]>='0'&&section->Name[i]<='9');
            offset = offset*10 + section->Name[i] - '0';
        }
        name = ((char*)stringTableData + offset);
    } else {
        for(int i=0;i<8;i++) {
            if(0==section->Name[i])
                break; 
            name += section->Name[i];
        }
    }
    return name;
}
std::string FileCOFF::getSymbolName(int symbolIndex) {
    Assert(symbols.size() > symbolIndex);
    auto symbol = symbols[symbolIndex];
    std::string name = "";
    if (symbol->Name.zero == 0) {
        u32 offset = symbol->Name.offset;
        name = ((char *)stringTableData + offset);
    } else {
        for (int i = 0; i < 8; i++) {
        if (0 == symbol->Name.ShortName[i])
            break;
        name += symbol->Name.ShortName[i];
        }
    }
    return name;
}
u64 AlignOffset(u64 offset, u64 alignment){
        return offset;
    u64 diff = (offset % alignment);
    if(diff == 0)
        return offset;

    u64 newOffset = offset + alignment - offset % alignment;
    engone::log::out << "align "<<offset << "->"<<newOffset<<"\n";
    return newOffset;
}
void FileCOFF::writeFile(const std::string& path) {
    using namespace engone;
    using namespace coff;

    //-- Calculate sizes
    
    int finalSize = COFF_File_Header::SIZE;

    DynamicArray<u32> sectionIndices;
    DynamicArray<int> sectionSwaps;

    // Filter sections
    for(int i=0;i<sections.size();i++){
        auto section = sections[i];
        std::string name = getSectionName(i);
        // if(name == ".text$mn"){
        // .drectve isn't necessary if you give the linker this flag /DEFAULTLIB:LIBCMT
        // That is what .drective section contains. and /DEFAULTLIB:OLDNAMES but the linker can run without it?
        if(
            // false
            name == ".drectve" 
            || 
            name == ".chks64" 
            || 
            name == ".debug$S"
            || 
            name == ".xdata"
            ||
            name == ".pdata"
        ){
            sectionSwaps.add(-1);
        } else { 
            sectionSwaps.add(sectionIndices.size());
            sectionIndices.add(i);

            finalSize += Section_Header::SIZE + section->SizeOfRawData + 
                section->NumberOfRelocations * COFF_Relocation::SIZE;
        }
    }
    
    // for(int i=0;i<sectionIndices.size();i++){
    //     auto section = sections[sectionIndices[i]];
    //     std::string name = getSectionName(sectionIndices[i]);
        
        // if(name.length()>8){
        //     finalSize += name.length(); // stringTableSize is added later which includes this
        // }
    // }

    #define STR_EQUAL(strA,lenA,strB) (lenA==strlen(strB) && !strncmp(strA,strB,lenA))

    DynamicArray<u32> symbolIndices;
    DynamicArray<int> symbolSwaps;
  
    for(int i=0;i<header->NumberOfSymbols;i++){
        // u32 symbolOffset = header->PointerToSymbolTable + i * Symbol_Record::SIZE;
        // Assert(fileSize>=symbolOffset+Symbol_Record::SIZE);
        Symbol_Record* symbolRecord = (Symbol_Record*)(_rawFileData + header->PointerToSymbolTable + i * Symbol_Record::SIZE);

        char* str = nullptr;
        int len = 0;
        if(symbolRecord->Name.zero==0){
            str = (char*)stringTableData + symbolRecord->Name.offset;
            len = strlen(str);
        } else {
            str = symbolRecord->Name.ShortName;
            for(int i=0;i<8;i++) {
                if(0==symbolRecord->Name.ShortName[i])
                    break;
                len++;
            }
        }
        
        bool remove = false;
        int sectionIndex = symbolRecord->SectionNumber-1;
        if(sectionIndex>=0){
            if(sectionIndex<sectionSwaps.size()){
                int newIndex = sectionSwaps[sectionIndex];
                if(newIndex<0){
                    remove=true;
                } else {
                }
            } else {
                remove=true;
            }
        }
        if(STR_EQUAL(str,len,"@vol.md")) remove = true;
        if(STR_EQUAL(str,len,"@feat.00")) remove = true;
        if(STR_EQUAL(str,len,"@comp.id")) remove = true;
        if(STR_EQUAL(str,len,".text$mn")) remove = true;
        if(STR_EQUAL(str,len,"$LN3")) remove = true;
        if(STR_EQUAL(str,len,".xdata")) remove = true;
        if(STR_EQUAL(str,len,".pdata")) remove = true;
        if(STR_EQUAL(str,len,".data")) remove = true;
        if(STR_EQUAL(str,len,"$pdata$main")) remove = true;

        // if(STR_EQUAL(str,len,"main")) remove = true;
        // if(STR_EQUAL(str,len,"ok")) remove = true;

        if(remove){
            symbolSwaps.add(-1);
            if(symbolRecord->StorageClass == IMAGE_SYM_CLASS_STATIC){
                for(int j=0;j<symbolRecord->NumberOfAuxSymbols;j++){
                    i++;
                    symbolSwaps.add(-1);
                }
            }
        } else {
            symbolSwaps.add(symbolIndices.size());
            symbolIndices.add(i);
            finalSize += Symbol_Record::SIZE;
            if(symbolRecord->StorageClass == IMAGE_SYM_CLASS_STATIC){
                for(int j=0;j<symbolRecord->NumberOfAuxSymbols;j++){
                    finalSize += Symbol_Record::SIZE;
                    i++;
                    symbolSwaps.add(symbolIndices.size());
                    symbolIndices.add(i);
                }
            }
        }
    }

    // TODO: remove unnecessary strings
    finalSize += stringTableSize;

    // log::out << "Final Size: "<<finalSize<<"\n";
    
    // finalSize += 16 * (2 * sectionIndices.size()); // potential alignment for raw data and relocations in eaach section

    // log::out << "Final Size (aligned): "<<finalSize<<"\n";

    u8* outData = TRACK_ARRAY_ALLOC(u8,finalSize);
    defer { TRACK_ARRAY_FREE(outData,u8,finalSize); };

    Assert((u64)outData % 8 == 0);

    u64 fileOffset = 0;

    COFF_File_Header* newHeader = (COFF_File_Header*)(outData + fileOffset);
    fileOffset += COFF_File_Header::SIZE;
    memcpy(newHeader, header, COFF_File_Header::SIZE);

    newHeader->NumberOfSections = sectionIndices.size();
    newHeader->NumberOfSymbols = symbolIndices.size();
    newHeader->SizeOfOptionalHeader = 0;

    DynamicArray<Section_Header*> newSections;
    for(int i=0;i<sectionIndices.size();i++){
        auto* section = sections[sectionIndices[i]];
        
        // fileOffset = AlignOffset(fileOffset, 16); // NOTE: 16-byte alignment isn't always necessary.
        auto* newSection = (Section_Header*)(outData + fileOffset);
        fileOffset += Section_Header::SIZE;
        memcpy(newSection, section, Section_Header::SIZE);

        std::string name = getSectionName(sectionIndices[i]);
        if(name == ".text$mn"){
            strcpy(newSection->Name,".text");
        }

        newSections.add(newSection);
    }

    for(int i=0;i<newSections.size();i++){
        auto* section = sections[sectionIndices[i]];
        auto* newSection = newSections[i];
        
        if(newSection->SizeOfRawData!=0){
            // fileOffset = AlignOffset(fileOffset, 16);
            memcpy(outData + fileOffset, _rawFileData + section->PointerToRawData, section->SizeOfRawData);
            newSection->PointerToRawData = fileOffset;
            fileOffset += newSection->SizeOfRawData;
        }
        if(newSection->NumberOfRelocations!=0){
            // fileOffset = AlignOffset(fileOffset, 16);
            newSection->PointerToRelocations = fileOffset;
            for (int j=0;j<newSection->NumberOfRelocations;j++) {
                COFF_Relocation* relocation = (COFF_Relocation*)(_rawFileData + section->PointerToRelocations + j * COFF_Relocation::SIZE);
                COFF_Relocation* newRelocation = (COFF_Relocation*)(outData + fileOffset);
                fileOffset += COFF_Relocation::SIZE;
                memcpy(newRelocation, relocation, COFF_Relocation::SIZE);
                newRelocation->SymbolTableIndex = symbolSwaps[newRelocation->SymbolTableIndex];

            }
        }
    }

    // not aligned? if it needs don't forget to add extra bytes to final size
    // fileOffset = AlignOffset(fileOffset, 16);
    // memcpy(outData + fileOffset, _rawFileData + header->PointerToSymbolTable, header->NumberOfSymbols * Symbol_Record::SIZE);
    newHeader->PointerToSymbolTable = fileOffset;
    // fileOffset += sec * Symbol_Record::SIZE;

    for(int i=0;i<symbolIndices.size();i++){
        Symbol_Record* symbol  = symbols[symbolIndices[i]];

        Symbol_Record* newSymbol  = (Symbol_Record*)(outData + fileOffset);
        fileOffset += Symbol_Record::SIZE;
        memcpy(newSymbol, symbol, Section_Header::SIZE);

        int sectionIndex = newSymbol->SectionNumber-1;
        if(sectionIndex>=0){
            // MAGE_SYM_UNDEFINED = 0 // The symbol record is not yet assigned a section. A value of zero indicates that a reference to an external symbol is defined elsewhere. A value of non-zero is a common symbol with a size that is specified by the value.
            // IMAGE_SYM_ABSOLUTE = -1 // The symbol has an absolute (non-relocatable) value and is not an address.
            // IMAGE_SYM_DEBUG = -2 // ?
            // if(sectionIndex<sectionSwaps.size()){
            int newNumber = sectionSwaps[sectionIndex] + 1;
            Assert(newNumber>=1);
            newSymbol->SectionNumber = newNumber;
            // } else {
            //     // section was deleted
            // }
        }
        if(newSymbol->StorageClass == IMAGE_SYM_CLASS_STATIC){
            for(int j=0;j<newSymbol->NumberOfAuxSymbols;j++){
                i++;
                Aux_Format_5* auxSymbol = (Aux_Format_5*)symbols[symbolIndices[i]];
                Aux_Format_5* newAuxSymbol  = (Aux_Format_5*)(outData + fileOffset);
                fileOffset += Symbol_Record::SIZE;
                memcpy(newAuxSymbol, auxSymbol, Symbol_Record::SIZE);
                // symbolSwaps.add(i-decr);
                // symbolIndices.add(i);
            }
        }
    }

    // u64 stringTableOffset = header->PointerToSymbolTable+header->NumberOfSymbols*Symbol_Record::SIZE;
    // char* stringTablePointer = (char*)_rawFileData + stringTableOffset;
    // u32 newstringTableSize = *(u32*)(stringTablePointer);

    // fileOffset = AlignOffset(fileOffset, 16);
    memcpy(outData + fileOffset, stringTableData, stringTableSize);
    // header->PointerToSymbolTable = fileOffset;
    fileOffset += stringTableSize;

    auto file = engone::FileOpen(path,FILE_CLEAR_AND_WRITE);
    if(!file) {
        log::out << "failed creating\n";
        return;
    }
    engone::FileWrite(file, outData, fileOffset);
    // FileWrite(file, _rawFileData, fileSize);
    engone::FileClose(file);
}

bool FileCOFF::WriteFile(const std::string& path, Program* program, u32 from, u32 to){
    using namespace engone;
    using namespace coff;
    Assert(program);

    // function needs some fixing (MAKE SURE YOU FULLY FIX IT WHEN YOU DECIDE TO!)
    // perhaps this function isn't needed (ObjectFile::writeFile_coff) does something similar
    // BUT some old print asm function USED to create an object file and then do dumpbin/objdump on that object file.
    Assert(false); // function needs to be reworked
    #ifdef gone

    if(to==-1){
        to = program->size();
    }

    ByteStream* obj_stream = ByteStream::Create(nullptr);
    defer {
        if(obj_stream) {
            ByteStream::Destroy(obj_stream, nullptr);
            obj_stream = nullptr;
        }
    };
    bool suc = true;
    #define CHECK Assert(suc);
    
    // TODO: Improve the memory estimation
    u64 estimation = 2000 + program->size() + program->dataRelocations.size() * 18
        + program->namedUndefinedRelocations.size() * 30 + program->globalSize;
    if(program->debugInformation) {
        estimation += program->debugInformation->files.size() * 200 + program->debugInformation->functions.size() * 100;
        for(int i=0;i<program->debugInformation->functions.size();i++) {
            auto& fun = program->debugInformation->functions[i];
            estimation += 20 + fun.localVariables.size() * 30 + fun.lines.size() * 30;
        }
    }
    obj_stream->reserve(estimation);
    
    /*#########################################
        We begin with the header and sections
    ############################################*/
    COFF_File_Header* header = nullptr;
    suc = obj_stream->write_late((void**)&header, COFF_File_Header::SIZE);
    CHECK
    
    DynamicArray<std::string> stringsForTable;
    u32 stringTableOffset = 4; // 4 because the integer for the string table size is included
    
    header->Machine = (Machine_Flags)IMAGE_FILE_MACHINE_AMD64;
    header->Characteristics = (COFF_Header_Flags)0;
    header->NumberOfSections = 0; // incremented later
    header->NumberOfSymbols = 0; // incremented later
    header->SizeOfOptionalHeader = 0;
    header->TimeDateStamp = time(nullptr);
    
    Section_Header* textSection = nullptr;
    int textSectionNumber = ++header->NumberOfSections;
    suc = obj_stream->write_late((void**)&textSection, Section_Header::SIZE);
    CHECK
    
    strcpy(textSection->Name,".text");
    textSection->NumberOfLineNumbers = 0;
    textSection->PointerToLineNumbers = 0;
    textSection->VirtualAddress = 0;
    textSection->VirtualSize = 0;
    textSection->Characteristics = (Section_Flags)(
        IMAGE_SCN_CNT_CODE |
        IMAGE_SCN_ALIGN_16BYTES |
        IMAGE_SCN_MEM_EXECUTE |
        IMAGE_SCN_MEM_READ);
      
    
    int dataSectionNumber = -1;
    Section_Header* dataSection = nullptr;
    if (program->globalData && program->globalSize!=0){
        dataSectionNumber = ++header->NumberOfSections;
        suc = obj_stream->write_late((void**)&dataSection, Section_Header::SIZE);
        CHECK
        
        strcpy(dataSection->Name,".data");
        dataSection->NumberOfLineNumbers = 0;
        dataSection->PointerToLineNumbers = 0;
        dataSection->NumberOfRelocations = 0;
        dataSection->PointerToRelocations = 0;
        dataSection->VirtualAddress = 0;
        dataSection->VirtualSize = 0;
        dataSection->Characteristics = (Section_Flags)(
            IMAGE_SCN_CNT_INITIALIZED_DATA |
            IMAGE_SCN_ALIGN_8BYTES |
            IMAGE_SCN_MEM_WRITE |
            IMAGE_SCN_MEM_READ);
    }
    
    // For DWARF debug
    dwarf::DWARFInfo dwarfInfo{};
    dwarfInfo.debug = program->debugInformation;
    dwarfInfo.stringTable = &stringsForTable;
    dwarfInfo.stringTableOffset = &stringTableOffset;
    dwarfInfo.coff_header = header;
    dwarfInfo.stream = obj_stream;
    
    // For PDB debug
    int debugSSectionNumber = -1;
    int debugTSectionNumber = -1;
    Section_Header* debugSSection = nullptr;
    Section_Header* debugTSection = nullptr;
    
    if(program->debugInformation) {
        #ifdef USE_DWARF_AS_DEBUG
        
        // dwarf::ProvideSections(&dwarfInfo, DWARF_OBJ_COFF);
        
        #else
        debugSSectionNumber = ++header->NumberOfSections;
        suc = obj_stream->write_late((void**)&debugSSectionNumber, Section_Header::SIZE);
        CHECK

        strcpy(debugSSection->Name,".debug$S");
        debugSSection->NumberOfLineNumbers = 0;
        debugSSection->PointerToLineNumbers = 0;
        debugSSection->NumberOfRelocations = 0;
        debugSSection->PointerToRelocations = 0;
        debugSSection->VirtualAddress = 0;
        debugSSection->VirtualSize = 0;
        debugSSection->Characteristics = (Section_Flags)(
            IMAGE_SCN_CNT_INITIALIZED_DATA |
            IMAGE_SCN_ALIGN_1BYTES |
            IMAGE_SCN_MEM_DISCARDABLE |
            IMAGE_SCN_MEM_READ);

        debugTSectionNumber = ++header->NumberOfSections;
        suc = obj_stream->write_late((void**)&debugTSection, Section_Header::SIZE);
        CHECK
        
        strcpy(debugTSection->Name,".debug$T");
        debugTSection->NumberOfLineNumbers = 0;
        debugTSection->PointerToLineNumbers = 0;
        debugTSection->NumberOfRelocations = 0;
        debugTSection->PointerToRelocations = 0;
        debugTSection->VirtualAddress = 0;
        debugTSection->VirtualSize = 0;
        debugTSection->Characteristics = (Section_Flags)(
            IMAGE_SCN_CNT_INITIALIZED_DATA |
            IMAGE_SCN_ALIGN_1BYTES |
            IMAGE_SCN_MEM_DISCARDABLE |
            IMAGE_SCN_MEM_READ);
        #endif
    }

    /* #####################
     WRITE DATA TO SECTIONS
    ##################### */
    u32 programSize = to - from;
    
    textSection->SizeOfRawData = programSize;
    textSection->PointerToRawData = obj_stream->getWriteHead();
    if(programSize !=0 ){
        Assert(program->text);
        suc = obj_stream->write(program->text + from, textSection->SizeOfRawData);
        CHECK
    }

    u32 totalSymbols = 0;
    std::unordered_map<i32, i32> dataSymbolMap;
    DynamicArray<i32> dataSymbols;

    std::unordered_map<std::string, u32> namedSymbolMap; // named as in the name is the important part
    DynamicArray<std::string> namedSymbols;

    auto add_or_get_symbol = [&](const std::string& name) {
        auto pair = namedSymbolMap.find(name);
        if(pair == namedSymbolMap.end()){
            namedSymbolMap[name] = totalSymbols;
            namedSymbols.add(name);
            totalSymbols++;
            return (totalSymbols - 1);
        } else {
            return pair->second;
        }
    };

    textSection->NumberOfRelocations = 0;
    textSection->PointerToRelocations = obj_stream->getWriteHead();
    if(program->dataRelocations.size()!=0){
        textSection->NumberOfRelocations += program->dataRelocations.size();

        for(int i=0;i<program->dataRelocations.size();i++){
            auto& dataRelocation = program->dataRelocations[i];
            
            COFF_Relocation* coffReloc = nullptr;
            suc = obj_stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
            CHECK
            
            coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_REL32;
            coffReloc->VirtualAddress = dataRelocation.textOffset;

            auto pair = dataSymbolMap.find(dataRelocation.dataOffset);
            if(pair == dataSymbolMap.end()){
                coffReloc->SymbolTableIndex = totalSymbols;
                dataSymbolMap[dataRelocation.dataOffset] = totalSymbols;
                dataSymbols.add(dataRelocation.dataOffset);
                totalSymbols++;
            } else {
                coffReloc->SymbolTableIndex = pair->second;
            }
        }
    }
    if(program->namedUndefinedRelocations.size()!=0){
        textSection->NumberOfRelocations += program->namedUndefinedRelocations.size();

        for(int i=0;i<program->namedUndefinedRelocations.size();i++){
            auto& namedRelocation = program->namedUndefinedRelocations[i];
            
            COFF_Relocation* coffReloc = nullptr;
            suc = obj_stream->write_late((void**)&coffReloc, COFF_Relocation::SIZE);
            CHECK
            
            coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_REL32;
            coffReloc->VirtualAddress = namedRelocation.textOffset;
            
            coffReloc->SymbolTableIndex = add_or_get_symbol(namedRelocation.name);

            // auto pair = namedSymbolMap.find(namedRelocation.name);
            // if(pair == namedSymbolMap.end()){
            //     coffReloc->SymbolTableIndex = totalSymbols;
            //     namedSymbolMap[namedRelocation.name] = totalSymbols;
            //     namedSymbols.add(namedRelocation.name);
            //     totalSymbols++;
            // } else {
            //     coffReloc->SymbolTableIndex = pair->second;
            // }
        }
    }

    if (dataSection){
        dataSection->SizeOfRawData = program->globalSize;
        dataSection->PointerToRawData = obj_stream->getWriteHead();
        if(dataSection->SizeOfRawData !=0){
            Assert(program->globalData);
            suc = obj_stream->write(program->globalData, dataSection->SizeOfRawData);
            CHECK
            // log::out << "Global data: "<<program->globalSize<<"\n";
        }
    }

    struct FuncSymbol {
        std::string name;
        // u32 sectionNumber = 0;
        u32 address = 0;
        u32 symbolIndex = 0;
    };
    DynamicArray<FuncSymbol> funcSymbols;
    for(int i=0;i<program->exportedSymbols.size();i++) {
        auto& sym = program->exportedSymbols[i];
        FuncSymbol tmp{};
        tmp.name = sym.name;
        tmp.address = sym.textOffset;
        tmp.symbolIndex = totalSymbols++;
        funcSymbols.add(tmp);
    }
    bool hasMain = false;
    FOR (funcSymbols) {
        if(it.name == "main") {
            hasMain = true;
            break;
        }
    }
    if(!hasMain){
        FuncSymbol sym{};
        sym.name = "main";
        sym.address = 0;
        sym.symbolIndex = totalSymbols++;
        // sym.sectionNumber = textSectionNumber;
        funcSymbols.add(sym);
    }

    DynamicArray<coff::SectionSymbol> sectionSymbols;
    dwarfInfo.sectionSymbols = &sectionSymbols;

    {
        SectionSymbol tmp{};
        tmp.name = ".text";
        tmp.symbolIndex = totalSymbols++;
        dwarfInfo.symindex_text = tmp.symbolIndex;
        tmp.sectionNumber = textSectionNumber;
        sectionSymbols.add(tmp);
        if(program->debugInformation) {
            tmp.name = ".debug_info";
            tmp.symbolIndex = totalSymbols++;
            dwarfInfo.symindex_debug_info = tmp.symbolIndex;
            tmp.sectionNumber = dwarfInfo.number_debug_info;
            sectionSymbols.add(tmp);

            tmp.name = ".debug_line";
            tmp.symbolIndex = totalSymbols++;
            dwarfInfo.symindex_debug_line = tmp.symbolIndex;
            tmp.sectionNumber = dwarfInfo.number_debug_line;
            sectionSymbols.add(tmp);
            
            tmp.name = ".debug_abbrev";
            tmp.symbolIndex = totalSymbols++;
            dwarfInfo.symindex_debug_abbrev = tmp.symbolIndex;
            tmp.sectionNumber = dwarfInfo.number_debug_abbrev;
            sectionSymbols.add(tmp);
            
            tmp.name = ".debug_frame";
            tmp.symbolIndex = totalSymbols++;
            dwarfInfo.symindex_debug_frame = tmp.symbolIndex;
            tmp.sectionNumber = dwarfInfo.number_debug_frame;
            sectionSymbols.add(tmp);
        }
    }
    dwarfInfo.program = program;

    if(program->debugInformation) {
        #ifdef USE_DWARF_AS_DEBUG
        
        // dwarf::ProvideSectionData(&dwarfInfo, DWARF_OBJ_ELF);
        
        #else
        DebugInformation* di = program->debugInformation;
        #undef WRITE
        #undef ALIGN4
        #define WRITE(TYPE, EXPR) *(TYPE*)(outData + outOffset) = (EXPR); outOffset += sizeof(TYPE);
        #define ALIGN4 if((outOffset & 3) != 0)  outOffset += 4 - outOffset & 3;
        
        /*
            Debug types
        */
        TypeInformation typeInformation{};
        typeInformation.functionTypeIndices.resize(di->functions.size());
        typeInformation.age = 0;
        memset(typeInformation.guid, 0x12, sizeof(typeInformation.guid));
        Path objpath = path;
        objpath = objpath.getAbsolute();
        Path pdbpath = path;
        pdbpath = pdbpath.getDirectory().text + pdbpath.getFileName(true).text + ".pdb";
        pdbpath = pdbpath.getAbsolute();
        bool result = WritePDBFile(pdbpath.text.c_str(), di, &typeInformation);
        if(!result) {
            Assert(result);
            return false;
        }
        if(debugTSection) {
            debugTSection->PointerToRawData = outOffset;
            WRITE(u32, CV_SIGNATURE_C13);
            u32 nextTypeIndex = 0x1000; // below 0x1000 are predefined types
            
            u32 TYPESERVER_id = nextTypeIndex++;
            {
                u16* typeLength = (u16*)(outData + outOffset);
                outOffset += 2;
                WRITE(LeafType, LF_TYPESERVER2); // leaf type

                memcpy(outData + outOffset, typeInformation.guid, sizeof(typeInformation.guid));
                outOffset += 16;
                WRITE(u32, typeInformation.age);

                strcpy((char*)outData + outOffset, pdbpath.text.c_str());
                outOffset += pdbpath.text.length() + 1;
                
                *typeLength = outOffset - sizeof(*typeLength) - ((u64)typeLength - (u64)outData);
            }
            debugTSection->SizeOfRawData = outOffset - debugTSection->PointerToRawData;
        }
        /*
            Debug symbols
        */
        if(debugSSection) {
            funcSymbols.reserve(program->debugInformation->functions.size());
            ALIGN4
            debugSSection->PointerToRawData = outOffset;
            
            WRITE(u32, CV_SIGNATURE_C13);
            
            // exported symbols and functions from debug information collides here
            // we have to fix this up. The code is broken.
            Assert(false); 
            Assert(funcSymbols.size() == 0);
            // IMPORTANT: functions can't be overloaded
            for(int i=0;i<di->functions.size();i++){
                auto& fun = di->functions[i];
                FuncSymbol sym;
                sym.name = fun.name;
                sym.address = fun.funcStart;
                sym.symbolIndex = totalSymbols++;
                // sym.sectionNumber = textSectionNumber;
                funcSymbols.add(sym);
            }

            struct OffSegReloc {
                u32 off_address = 0;
                u32 seg_address = 0;
                u32 symbolIndex = 0;
            };
            DynamicArray<OffSegReloc> offSegRelocations;

            QuickArray<u32> fileIds;
            fileIds.resize(di->files.size());
            QuickArray<u32> fileStringIndex;
            fileStringIndex.resize(di->files.size());
            { // string table
                ALIGN4
                *(SubSectionType*)(outData + outOffset) = SubSectionType::DEBUG_S_STRINGTABLE;
                outOffset += 4;
                u32* sectionLength = (u32*)(outData + outOffset);
                outOffset += 4;
                u32 sectionStartOffset = outOffset;

                WRITE(char, 0); // empty string, C object files has them
                
                for(int i=0;i<di->files.size();i++) {
                    auto& file = di->files[i];
                    fileStringIndex[i] = outOffset - sectionStartOffset;
                    Path apath = file;
                    std::string absPath = apath.getAbsolute().text;
                    strcpy((char*)outData + outOffset, absPath.c_str());
                    outOffset += absPath.length() + 1;
                }
                *sectionLength = outOffset - sectionStartOffset;
            }
            { // files
                ALIGN4
                *(SubSectionType*)(outData + outOffset) = SubSectionType::DEBUG_S_FILECHKSMS;
                outOffset += 4;
                u32* sectionLength = (u32*)(outData + outOffset);
                outOffset += 4;
                u32 startOffset = outOffset;
                for(int i=0;i<di->files.size();i++) {
                    auto& file = di->files[i];
                    u32 fileid = outOffset - startOffset;
                    fileIds[i] = fileid;

                    WRITE(u32, fileStringIndex[i]); // index into string table
                    WRITE(u8, 0); // byte count for checksum
                    WRITE(u8, CHKSUM_TYPE_NONE); // checksum type
                    
                    ALIGN4
                }
                *sectionLength = outOffset - sizeof(*sectionLength) - ((u64)sectionLength - (u64)outData);;
            }
            { // symbols
                ALIGN4
                *(SubSectionType*)(outData + outOffset) = SubSectionType::DEBUG_S_SYMBOLS;
                outOffset += 4;
                u32* sectionLength = (u32*)(outData + outOffset);
                outOffset += 4;
                u32 sectionStartOffset = outOffset;

                { // OBJNAME record
                    u16* recordLength = (u16*)(outData + outOffset);
                    outOffset += 2;
                    *(RecordType*)(outData + outOffset) = RecordType::S_OBJNAME;
                    outOffset += 2;

                    *(u32*)(outData + outOffset) = 0; // signature, usually 0 for some reason.
                    outOffset += 4;

                    strcpy((char*)outData + outOffset, objpath.text.c_str());
                    outOffset += objpath.text.length() + 1;

                    *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                }
                { // COMPILE3 record
                    u16* recordLength = (u16*)(outData + outOffset);
                    outOffset += 2;
                    *(RecordType*)(outData + outOffset) = RecordType::S_COMPILE3;
                    outOffset += 2;

                    Record_COMPILE3* comp = (Record_COMPILE3*)(outData + outOffset);
                    outOffset += sizeof(Record_COMPILE3) - 2;

                    memset(comp, 0, sizeof(Record_COMPILE3));
                    comp->machine = CV_CFL_X64;
                    comp->verFEMinor = 1;
                    comp->verMinor = 1;
                    comp->flags.iLanguage = 0xFF;
                    // comp->flags.iLanguage = CV_CFL_C; // TEMPORARY:
                    
                    const char* name = "BTB Compiler";
                    strcpy((char*)outData + outOffset, name);
                    outOffset += strlen(name) + 1;

                    *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                }
                for(int i=0;i<di->functions.size();i++) {
                    if(i>2)
                        continue;
                        
                    // ALIGN4
                    auto& fun = di->functions[i];
                    {
                        u16* recordLength = (u16*)(outData + outOffset);
                        outOffset += 2;
                        *(RecordType*)(outData + outOffset) = RecordType::S_GPROC32;
                        outOffset += 2;

                        WRITE(u32, 0) // parent
                        WRITE(u32, 0) // end
                        WRITE(u32, 0) // next
                        WRITE(u32, fun.funcEnd - fun.funcStart) // len
                        // WRITE(u32, fun.lineStart) // dbgstart
                        // WRITE(u32, fun.codeEnd) // dbgend
                        WRITE(u32, fun.lineStart - fun.funcStart) // dbgstart
                        WRITE(u32, fun.codeEnd - fun.funcStart) // dbgend
                        WRITE(u32, typeInformation.functionTypeIndices[i]) // typeind

                        OffSegReloc reloc;
                        reloc.symbolIndex = funcSymbols[i].symbolIndex;
                        reloc.off_address = outOffset - debugSSection->PointerToRawData;
                        WRITE(u32, 0) // off
                        reloc.seg_address = outOffset - debugSSection->PointerToRawData;
                        WRITE(u16, 0) // seg
                        offSegRelocations.add(reloc);
                        
                        WRITE(u8, 0) // flags, I THINK ZERO WILL WORK BUT MAYBE NOT?

                        strcpy((char*)outData + outOffset, fun.name.c_str());
                        outOffset += fun.name.length() + 1;

                        *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                    }
                    // {
                    //     u16* recordLength = (u16*)(outData + outOffset);
                    //     outOffset += 2;
                    //     *(RecordType*)(outData + outOffset) = RecordType::S_FRAMEPROC;
                    //     outOffset += 2;

                    //     WRITE(u32, 0) // size of frame
                    //     WRITE(u32, 0) // padding in frame
                    //     WRITE(u32, 0) // padding offset
                    //     WRITE(u32, 0) // bytes for saved registers
                    //     WRITE(u32, 0) // exception handler offset
                    //     WRITE(u16, 0) // excpetion handler id
                    //     WRITE(u32, 0) // flags
                        
                    //     // These might need to bes in the flags but I am not sure.
                    //     // u32 encodedLocalBasePointer : 2;  // record function's local pointer explicitly.
                    //     // u32 encodedParamBasePointer : 2;  // record function's parameter pointer explicitly.

                    //     *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                    // }
                    // for(int j=0;j<fun.localVariables.size();j++){
                    //     auto& var = fun.localVariables[j];
                    //     {
                    //         u16* recordLength = (u16*)(outData + outOffset);
                    //         outOffset += 2;
                    //         *(RecordType*)(outData + outOffset) = RecordType::S_REGREL32;
                    //         outOffset += 2;

                    //         WRITE(u32, var.frameOffset); // offset in frame
                    //         // TODO: Don't always use int as type
                    //         WRITE(u32, T_INT4); // type
                    //         WRITE(u16, CV_AMD64_RBP); // Object files from C uses AMD64 instead of x64

                    //         strcpy((char*)outData + outOffset, var.name.c_str());
                    //         outOffset += var.name.length() + 1;

                    //         *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                    //     }
                    // }
                    {
                        u16* recordLength = (u16*)(outData + outOffset);
                        outOffset += 2;
                        *(RecordType*)(outData + outOffset) = RecordType::S_PROC_ID_END;
                        outOffset += 2;

                        *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                    }
                }

                *sectionLength = outOffset - sectionStartOffset;
            }
            { // lines
                ALIGN4
                for(int i=0;i<di->functions.size();i++) {
                    auto& fun = di->functions[i];
                    *(SubSectionType*)(outData + outOffset) = SubSectionType::DEBUG_S_LINES;
                    outOffset += 4;
                    u32* sectionLength = (u32*)(outData + outOffset);
                    outOffset += 4;

                    OffSegReloc reloc;
                    reloc.symbolIndex = funcSymbols[i].symbolIndex;
                    reloc.off_address = outOffset - debugSSection->PointerToRawData;
                    WRITE(u32, 0) // off
                    reloc.seg_address = outOffset - debugSSection->PointerToRawData;
                    WRITE(u16, 0) // seg
                    offSegRelocations.add(reloc);
                    WRITE(u16, 0); // flags, you can specify columns if you want; we don't.
                    WRITE(u32, fun.funcEnd - fun.funcStart); // size of this block or something?

                    // TODO: Support columns? CV_LINES_HAVE_COLUMNS

                    WRITE(u32, fileIds[fun.fileIndex]);
                    WRITE(u32, fun.lines.size());
                    WRITE(u32, 12 + fun.lines.size() * sizeof(CV_Line_t)); // file block

                    for (int j=0;j<fun.lines.size();j++) {
                        auto& line = fun.lines[j];
                        // line.
                        CV_Line_t data;
                        data.fStatement = 1;
                        if(j == 0)
                            data.offset = 0;
                        else
                            data.offset = line.funcOffset - fun.funcStart; // TODO: This is not right
                        data.linenumStart = line.lineNumber;
                        data.deltaLineEnd = 0;
                        WRITE(CV_Line_t, data);
                    }
                    *sectionLength = outOffset - sizeof(*sectionLength) - ((u64)sectionLength - (u64)outData);;
                }
            }

            ALIGN4

            CHECK
            debugSSection->SizeOfRawData = outOffset - debugSSection->PointerToRawData;

            if(offSegRelocations.size()!=0){
                debugSSection->PointerToRelocations = outOffset;
                debugSSection->NumberOfRelocations = offSegRelocations.size() * 2;

                for(int i=0;i<offSegRelocations.size();i++){
                    auto& reloc = offSegRelocations[i];
                    {
                        COFF_Relocation* coffReloc = (COFF_Relocation*)(outData + outOffset);
                        outOffset += COFF_Relocation::SIZE;
                        CHECK
                        
                        coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECREL;
                        coffReloc->VirtualAddress = reloc.off_address;

                        coffReloc->SymbolTableIndex = reloc.symbolIndex;
                    }
                    {
                        COFF_Relocation* coffReloc = (COFF_Relocation*)(outData + outOffset);
                        outOffset += COFF_Relocation::SIZE;
                        CHECK
                        
                        coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_SECTION;
                        coffReloc->VirtualAddress = reloc.seg_address;

                        coffReloc->SymbolTableIndex = reloc.symbolIndex;
                    }
                }
    
            }
        }
        #undef WRITE
        #endif
    }

    // Assert(false); // fix debug relocations for functions!
    {
        int writeHead = obj_stream->getWriteHead();
        // Assert((writeHead & 3) == 0);
        if((writeHead & 3) != 0) {
            suc = obj_stream->write_late(nullptr,  4 - (writeHead & 3));
            CHECK
        }
    }


    header->PointerToSymbolTable = obj_stream->getWriteHead();
    Assert(dataSymbols.size() == 0 || dataSectionNumber > 0); // ensure that dataSection exists if we have data symbols
    for (int i=0;i<dataSymbols.size();i++) {
        header->NumberOfSymbols++; //incremented when adding relocations
        Symbol_Record* symbol = nullptr;
        suc = obj_stream->write_late((void**)&symbol, Symbol_Record::SIZE);
        CHECK
        
        int len = sprintf(symbol->Name.ShortName,"$d%u",i);
        Assert(len <= 8);
        symbol->SectionNumber = dataSectionNumber;
        symbol->Value = dataSymbols[i];
        symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        // symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_STATIC;
        symbol->Type = 0;
        symbol->NumberOfAuxSymbols = 0;
    }
    
    for (int i=0;i<namedSymbols.size();i++) {
        header->NumberOfSymbols++; //incremented when adding relocations
        Symbol_Record* symbol = nullptr;
        suc = obj_stream->write_late((void**)&symbol, Symbol_Record::SIZE);
        CHECK
        
        auto& name = namedSymbols[i];
        if(name.length()<=8) {
            strcpy(symbol->Name.ShortName, name.c_str());
        } else {
            symbol->Name.zero = 0; // NOTE: is this okay if it's not aligned? is it gonna be really slow?
            symbol->Name.offset = stringTableOffset;
            stringsForTable.add(name);
            stringTableOffset += name.length() + 1; // null termination included
        }
        symbol->SectionNumber = 0; // doesn't belong to a known section
        symbol->Value = 0; // unknown for named symbols
        // symbol->Type 
        
        symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        symbol->Type = 32; // should perhaps be a function?
        // symbol->Type = IMAGE_SYM_DTYPE_FUNCTION;
        symbol->NumberOfAuxSymbols = 0;
    }

    for (int i=0;i<funcSymbols.size();i++){
        auto& fsymbol = funcSymbols[i];
        Assert(fsymbol.symbolIndex == header->NumberOfSymbols);
        header->NumberOfSymbols++; // incremented e
        Symbol_Record* symbol = nullptr;
        suc = obj_stream->write_late((void**)&symbol, Symbol_Record::SIZE);
        CHECK
        auto& name = fsymbol.name;
        if(name.length()<=8) {
            strcpy(symbol->Name.ShortName, name.c_str());
        } else {
            symbol->Name.zero = 0; // NOTE: is this okay if it's not aligned? is it gonna be really slow?
            symbol->Name.offset = stringTableOffset;
            stringsForTable.add(name);
            stringTableOffset += name.length() + 1; // null termination included
        }
        // strcpy(symbol->Name.ShortName, fsymbol.name.c_str());
        symbol->SectionNumber = textSectionNumber;
        symbol->Value = fsymbol.address; // address of function
        symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        symbol->Type = 32; // IMAGE_SYM_DTYPE_FUNCTION is a macro which evaluates to 2
        symbol->NumberOfAuxSymbols = 0;
    }
    for (int i=0;i<sectionSymbols.size();i++){
        auto& sym = sectionSymbols[i];
        Assert(sym.symbolIndex == header->NumberOfSymbols);
        header->NumberOfSymbols++; // incremented e
        Symbol_Record* symbol = nullptr;
        suc = obj_stream->write_late((void**)&symbol, Symbol_Record::SIZE);
        CHECK
        
        auto& name = sym.name;
        if(name.length()<=8) {
            strcpy(symbol->Name.ShortName, name.c_str());
        } else {
            symbol->Name.zero = 0; // NOTE: is this okay if it's not aligned? is it gonna be really slow?
            symbol->Name.offset = stringTableOffset;
            stringsForTable.add(name);
            stringTableOffset += name.length() + 1; // null termination included
        }
        // strcpy(symbol->Name.ShortName, sym.name.c_str());
        symbol->SectionNumber = sym.sectionNumber;
        symbol->Value = 0; // section has no value
        symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_STATIC;
        symbol->Type = 0; // IMAGE_SYM_DTYPE_FUNCTION is a macro which evaluates to 2
        symbol->NumberOfAuxSymbols = 0;
    }

    Assert(totalSymbols == header->NumberOfSymbols);

    suc = obj_stream->write(&stringTableOffset, sizeof(u32));
    CHECK
    
    for(int i=0;i<stringsForTable.size();i++) {
        auto& string = stringsForTable[i];
        // log::out << string << "\n";
        Assert(string.length() > 8); // string does not need to be in string table if it's length is less or equal to 8
        suc = obj_stream->write(string.c_str(), string.length() + 1);
        CHECK
    }
    // for (int i=0;i<namedSymbols.size();i++) {
    //     auto& name = namedSymbols[i];
    //     if(name.length()<=8) {
            
    //     } else {
    //         suc = obj_stream->write(name.c_str(), name.length() + 1);
    //         CHECK
    //     }
    // }

    void* contiguous_ptr = nullptr;
    u32 total_size = 0;
    // suc = obj_stream->finalize(&contiguous_ptr, &total_size); CHECK
    
    // NOTE: It is possible to finalize the byte stream but that will cause a massive allocation and many dealloactions.
    //  Iterating through the allocations with obj_stream->iterate but call FileWrite multiple times may be better.

    auto file = engone::FileOpen(path, FILE_CLEAR_AND_WRITE);
    Assert(file);
    
    if(contiguous_ptr) {
        engone::FileWrite(file, contiguous_ptr, total_size);
    } else {
        ByteStream::Iterator iter{};
        while(obj_stream->iterate(iter)) {
            engone::FileWrite(file, (void*)iter.ptr, iter.size);
        }
    }

    // if(program->debugInformation) {
    //     log::out << "Printing debug information\n";
    //     program->debugInformation->print();
    // }
    
    engone::FileClose(file);
    
    ByteStream::Destroy(obj_stream, nullptr); // not necessary since we use defer up top
    obj_stream = nullptr;
    
    #undef CHECK
    #endif
    return true;
}


void DeconstructPData(u8* buffer, u32 size) {
    using namespace engone;
    log::out << "PData " << size<<"\n";
    auto entries = (coff::RUNTIME_FUNCTION*)buffer;
    int entry_length = size / sizeof(*entries);
    for(int i=0;i<entry_length;i++){
        auto entry = entries + i;
        auto number = i + 1;
        log::out << log::LIME << " Entry "<<number<<"\n";
        log::out << "  Start addr: "<<entry->StartAddress<<"\n";
        log::out << "  End addr: "<<entry->EndAddress<<"\n";
        log::out << "  Unwind info: "<<entry->UnwindInfoAddress<<"\n";
    }
}
void DeconstructXData(u8* buffer, u32 size) {
    using namespace engone;
    log::out << "XData " << size<<"\n";

    int head = 0;
    int count = 0;
    int skipped_bytes = 0;
    while(head < size) {
        auto entry = (coff::UNWIND_INFO*)(buffer + head);

        if(entry->Version != 1 || (
            entry->Flags != coff::UNW_FLAG_CHAININFO &&
            entry->Flags != coff::UNW_FLAG_NHANDLER &&
            entry->Flags != coff::UNW_FLAG_EHANDLER &&
            entry->Flags != coff::UNW_FLAG_UHANDLER
        )) {
            // log::out << log::RED << "Version in unwind info should be 1.\n";
            head++;
            skipped_bytes++;
            continue;
        }
        if((head & 0x3) != 0) {
            head++;
            skipped_bytes++;
            continue;
        }

        // Assert((head & 0x3) == 0);
        if(skipped_bytes > 0) {
            log::out << log::RED << "Skipped " << skipped_bytes<<"\n";
            skipped_bytes = 0;
        }
        head += sizeof(*entry);

        count++;
        log::out << " Unwind info "<<count<<"\n";


        log::out << "  Version: "<<entry->Version<<"\n";
        log::out << "  Flags: ";
        if(entry->Flags == 0)
            log::out << "none";
        else {
            if(entry->Flags & coff::UNW_FLAG_EHANDLER)
                log::out << "ehandler ";
            if(entry->Flags & coff::UNW_FLAG_UHANDLER)
                log::out << "uhandler ";
            if(entry->Flags == coff::UNW_FLAG_CHAININFO)
                log::out << "chaininfo ";
            if((entry->Flags & (coff::UNW_FLAG_EHANDLER | coff::UNW_FLAG_UHANDLER)) && (entry->Flags & coff::UNW_FLAG_CHAININFO)) {
                log::out << log::RED << "XDATA IS INVALID, or parsed incorrectly. UNW_FLAG_CHAININFO cannot coexist with other flags.\n";
                return;
            }
        }
        log::out << "\n";

        log::out << "  Size of prolog: "<<entry->SizeOfProlog<<"\n";
        log::out << "  Count unwind codes: "<<entry->CountOfUnwindCodes<<"\n";
        log::out << "  Frame reg: "<<entry->FrameRegister<<"\n";
        log::out << "  Frame reg offset: "<<entry->FrameRegisterOffset<<"\n";

        auto codes = (coff::UNWIND_CODE*)(buffer + head);
        head += entry->CountOfUnwindCodes * sizeof(*codes);
        for(int j=0;j<entry->CountOfUnwindCodes;j++) {
            auto code = codes + j;
            log::out << "  Unwind code " << j<<"\n";
            log::out << "   Offset: "<<code->OffsetInProlog<<"\n";
            log::out << "   UnwindOpCode: ";
            int extra_nodes = 0;
            switch(code->UnwindOperationCode) {
                #undef CASE
                #define CASE(X, N) case coff::X: log::out << #X; extra_nodes = (N)-1; break;
                CASE(UWOP_PUSH_NONVOL, 1)
                CASE(UWOP_ALLOC_LARGE, code->OperationInfo == 0 ? 2 : 3)
                CASE(UWOP_ALLOC_SMALL, 1)
                CASE(UWOP_SET_FPREG, 1)
                CASE(UWOP_SAVE_NONVOL, 2)
                CASE(UWOP_SAVE_NONVOL_FAR, 3)
                CASE(UWOP_SAVE_XMM128, 2)
                CASE(UWOP_SAVE_XMM128_FAR, 3)
                CASE(UWOP_PUSH_MACHFRAME, 1)
                #undef CASE
            }
            log::out << "\n";
            log::out << "   OperationInfo: "<<code->OperationInfo<<"\n";

            if (extra_nodes > 0) {
                int value = 0;
                if(extra_nodes == 1)
                    value = *(u16*)(codes + j + 1);
                if(extra_nodes == 2)
                    value = *(u32*)(codes + j + 1);

                log::out << "  Extra node: " << value<<"\n";
                
                j += extra_nodes;
            }
        }
        
        // Align to 4 bytes if number of unwind codes is uneven.
        if(head & 0x3)
            head += 4 - (head & 0x3);

        // TODO: Decode chain info
        Assert((entry->Flags & coff::UNW_FLAG_CHAININFO) == 0);

        if(entry->Flags & (coff::UNW_FLAG_EHANDLER | coff::UNW_FLAG_UHANDLER)) {
            auto address_of_handler = *(u32*)(buffer + head);
            head += sizeof(u32);
            log::out << " EHandler: "<<address_of_handler<<"\n";

            // NOTE: Language specific handler data

            Assert(("DeconstructXData in COFF.cpp is not up to date with ObjectFile.cpp when decoding handler data",false));

            // SPECIFIC TO BTB LANGUAGE!
            int len = *(u32*)(buffer + head);
            head += 4; // sizeof TryBlock
            head += len * 16; // sizeof TryBlock
        }
    }
}