#include "BetBat/ObjectWriter.h"
#include "Engone/Logger.h"
#include "BetBat/Util/Utility.h"

#include "BetBat/PDBWriter.h"

#include <time.h>

namespace COFF_Format {
    #define SWITCH_START(TYPE) const char* ToString(TYPE flags);\
        engone::Logger& operator<<(engone::Logger& logger, TYPE flags){return logger << ToString(flags);}\
        const char* ToString(TYPE flags){ switch(flags){
    #define CASE(X) case X: return #X;
    #define SWITCH_END } return "Undefined machine"; }
    
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

void ObjectFile::Destroy(ObjectFile* objectFile){
    objectFile->~ObjectFile();
    // engone::Free(objectFile,sizeof(ObjectFile));
    TRACK_FREE(objectFile,ObjectFile);
}
ObjectFile* ObjectFile::DeconstructFile(const std::string& path, bool silent) {
    using namespace engone;
    using namespace COFF_Format;
    u64 fileSize=0;
    // std::string filename = "obj_test.o";
    // std::string filename = "obj_test.obj";
    auto file = FileOpen(path,&fileSize,FILE_ONLY_READ);
    if(!file)
        return nullptr;
    u8* filedata = TRACK_ARRAY_ALLOC(u8, fileSize);
    // u8* filedata = (u8*)engone::Allocate(fileSize);
    // defer {engone::Free(filedata,fileSize); };

    FileRead(file,filedata,fileSize);
    FileClose(file);
    if(!silent)
        log::out << "Read file "<<path << ", size: "<<fileSize<<"\n";

    // ObjectFile* objectFile = (ObjectFile*)engone::Allocate(sizeof(ObjectFile));
    ObjectFile* objectFile = TRACK_ALLOC(ObjectFile);
    new(objectFile)ObjectFile();
    objectFile->_rawFileData = filedata;
    objectFile->fileSize = fileSize;

    u64 fileOffset = 0;

    Assert(fileSize-fileOffset>=COFF_File_Header::SIZE);
    COFF_File_Header* coffHeader = (COFF_File_Header*)filedata;
    fileOffset = COFF_File_Header::SIZE;

    objectFile->header = coffHeader;

    if(!silent){
        #define LOGIT(X) log::out << " "#X": "<< baselog->X<<"\n";
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
    char* stringTablePointer = (char*)filedata + stringTableOffset;
    u32 stringTableSize = *(u32*)(stringTablePointer);

    objectFile->stringTableSize = stringTableSize;
    objectFile->stringTableData = stringTablePointer;

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
            }
            if(baselog->Name[0]=='/'){
                u32 offset = 0;
                for(int i=1;8;i++) {
                    if(baselog->Name[i]==0)
                        break;
                    Assert(baselog->Name[i]>='0'&&baselog->Name[i]<='9');
                    offset = offset*10 + baselog->Name[i] - '0';
                }
                
                if(!silent)
                    log::out <<((char*)stringTablePointer + offset)<<"\n";
            } else {
                for(int j=0;j<8;j++) {
                    if(0==baselog->Name[j])
                        break; 
                    if(!silent)
                        log::out << baselog->Name[j];
                }
                if(!silent)
                    log::out << " \n";
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
        for(int i=0;i<sectionHeader->NumberOfRelocations;i++){
            COFF_Relocation* relocation = (COFF_Relocation*)(objectFile->_rawFileData + sectionHeader->PointerToRelocations + i*COFF_Relocation::SIZE);
            if(!silent) {
                auto baselog = relocation;
                log::out << log::LIME << " relocation "<<i<<"\n";
                LOGIT(VirtualAddress)
                LOGIT(SymbolTableIndex)
                LOGIT(Type)
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
                if(baselog->Name.zero==0){
                    // log::out << " Name (offset): "<<baselog->Name.offset<<"\n";
                    log::out << " Name (long): "<<((char*)stringTablePointer + baselog->Name.offset)<<"\n";
                } else {
                    log::out << " Name: ";
                    for(int i=0;i<8;i++) {
                        if(0==baselog->Name.ShortName[i])
                            break;
                        log::out << baselog->Name.ShortName[i];
                    }
                    log::out << "\n";
                }
                LOGIT(Value)
                LOGIT(SectionNumber)
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
                    LOGIT(Length)
                    LOGIT(NumberOfRelocations)
                    LOGIT(NumberOfLineNumbers)
                    LOGIT(CheckSum)
                    LOGIT(Number)
                    LOGIT(Selection)
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

std::string ObjectFile::getSectionName(int sectionIndex){
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
u64 AlignOffset(u64 offset, u64 alignment){
        return offset;
    u64 diff = (offset % alignment);
    if(diff == 0)
        return offset;

    u64 newOffset = offset + alignment - offset % alignment;
    engone::log::out << "align "<<offset << "->"<<newOffset<<"\n";
    return newOffset;
}
void ObjectFile::writeFile(const std::string& path) {
    using namespace engone;
    using namespace COFF_Format;

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
    // u8* outData = (u8*)Allocate(finalSize);
    // defer { Free(outData,finalSize); };
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

    auto file = FileOpen(path,nullptr,FILE_ALWAYS_CREATE);
    if(!file) {
        log::out << "failed creating\n";
        return;
    }
    FileWrite(file, outData, fileOffset);
    // FileWrite(file, _rawFileData, fileSize);
    FileClose(file);
}

bool WriteObjectFile(const std::string& path, Program_x64* program, u32 from, u32 to){
    using namespace engone;
    using namespace COFF_Format;
    Assert(program);

    if(to==-1){
        to = program->size();
    }

    #define CHECK Assert(outOffset <= outSize);
    // TODO: It this estimation correct?
    u64 outSize = 2000 + program->size()
        + program->dataRelocations.size() * 30 // 20 for relocations and 10 for symbols, some relocations refer to the same symbols
        + program->namedRelocations.size() * 45 // a little extra since functions have bigger names
        + program->globalSize;

    if(program->debugInformation) {
        outSize += program->debugInformation->files.size() * 256;
        outSize += program->debugInformation->functions.size() * 100;
        // IMPORTANT: We need to estimate better by checking the contnet of each function
    }
    // u8* outData = (u8*)Allocate(outSize);
    // defer { Free(outData, outSize); };
    u8* outData = TRACK_ARRAY_ALLOC(u8,outSize);
    defer { TRACK_ARRAY_FREE(outData, u8, outSize); };
    u64 outOffset = 0;

    /*
        We begin with the header and sections
    */

    COFF_File_Header* header = (COFF_File_Header*)(outData + outOffset);
    outOffset+=COFF_File_Header::SIZE;
    CHECK

    header->Machine = (Machine_Flags)IMAGE_FILE_MACHINE_AMD64;
    header->Characteristics = (COFF_Header_Flags)0;
    header->NumberOfSections = 0; // incremented later
    header->NumberOfSymbols = 0; // incremented later
    header->SizeOfOptionalHeader = 0;
    header->TimeDateStamp = time(nullptr);
    
    const int textSectionNumber = 1;
    Section_Header* textSection = nullptr;
    const int dataSectionNumber = 2;
    Section_Header* dataSection = nullptr;
    
    const int debugSSectionNumber = 3;
    Section_Header* debugSSection = nullptr;
    const int debugTSectionNumber = 4;
    Section_Header* debugTSection = nullptr;

    header->NumberOfSections++;
    textSection = (Section_Header*)(outData+outOffset);
    outOffset+=Section_Header::SIZE;
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
      
    if (program->globalData && program->globalSize!=0){
        header->NumberOfSections++;
        dataSection = (Section_Header*)(outData+outOffset);
        outOffset+=Section_Header::SIZE;
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
    if(program->debugInformation) {
        header->NumberOfSections++;
        debugSSection = (Section_Header*)(outData+outOffset);
        outOffset+=Section_Header::SIZE;
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

        header->NumberOfSections++;
        debugTSection = (Section_Header*)(outData+outOffset);
        outOffset+=Section_Header::SIZE;
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

    }

    /*
        Here we deal with the data of the sections and
        other data.
    */
    u32 programSize = to - from;
    
    textSection->SizeOfRawData = programSize;
    textSection->PointerToRawData = outOffset;
    if(programSize !=0 ){
        Assert(program->text);
        memcpy(outData + outOffset, program->text + from, textSection->SizeOfRawData);
        outOffset += textSection->SizeOfRawData;
    }

    u32 totalSymbols = 0;
    std::unordered_map<i32, i32> dataSymbolMap;
    std::unordered_map<std::string, i32> namedSymbolMap; // named as in the name is the important part

    DynamicArray<i32> dataSymbols;
    DynamicArray<std::string> namedSymbols;

    textSection->NumberOfRelocations = 0;
    textSection->PointerToRelocations = outOffset;
    if(program->dataRelocations.size()!=0){
        textSection->NumberOfRelocations += program->dataRelocations.size();

        for(int i=0;i<program->dataRelocations.size();i++){
            auto& dataRelocation = program->dataRelocations[i];
            COFF_Relocation* coffReloc = (COFF_Relocation*)(outData + outOffset);
            outOffset += COFF_Relocation::SIZE;
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
    if(program->namedRelocations.size()!=0){
        textSection->NumberOfRelocations += program->namedRelocations.size();

        for(int i=0;i<program->namedRelocations.size();i++){
            auto& namedRelocation = program->namedRelocations[i];
            COFF_Relocation* coffReloc = (COFF_Relocation*)(outData + outOffset);
            outOffset += COFF_Relocation::SIZE;
            CHECK
            
            coffReloc->Type = (Type_Indicator)IMAGE_REL_AMD64_REL32;
            coffReloc->VirtualAddress = namedRelocation.textOffset;

            auto pair = namedSymbolMap.find(namedRelocation.name);
            if(pair == namedSymbolMap.end()){
                coffReloc->SymbolTableIndex = totalSymbols;
                namedSymbolMap[namedRelocation.name] = totalSymbols;
                namedSymbols.add(namedRelocation.name);
                totalSymbols++;
            } else {
                coffReloc->SymbolTableIndex = pair->second;
            }
        }
    }

    if (dataSection){
        dataSection->SizeOfRawData = program->globalSize;
        dataSection->PointerToRawData = outOffset;
        if(dataSection->SizeOfRawData !=0){
            Assert(program->globalData);
            memcpy(outData + outOffset, program->globalData, dataSection->SizeOfRawData);
            outOffset += dataSection->SizeOfRawData;
            // log::out << "Global data: "<<program->globalSize<<"\n";
        }
        CHECK
    }

    struct FuncSymbol {
        std::string name;
        // u32 sectionNumber = 0;
        u32 address = 0;
        u32 symbolIndex = 0;
    };
    DynamicArray<FuncSymbol> funcSymbols;

    if(program->debugInformation) {
        DebugInformation* di = program->debugInformation;
        #undef WRITE
        #undef ALIGN4
        #define WRITE(TYPE, EXPR) *(TYPE*)(outData + outOffset) = (EXPR); outOffset += sizeof(TYPE);
        #define ALIGN4 if((outOffset & 3) != 0)  outOffset += 4 - outOffset & 3;
        
        /*
            Debug types
        */
        TypeInformation typeInformation{};
        typeInformation.age = 0;
        memset(typeInformation.guid, 0x12, sizeof(typeInformation.guid));
        typeInformation.functionTypeIndices.resize(di->functions.size());
        bool result = WritePDBFile("bin/dev.pdb", di, &typeInformation);
        if(!result) {
            return false;
        }

        debugTSection->PointerToRawData = outOffset;
        WRITE(u32, CV_SIGNATURE_C13);
        u32 nextTypeIndex = 0x1000; // below 0x1000 are predefined types
        
        log::out << log::YELLOW << "LF_FUNC_ID should be in ID types\n";

        u32 TYPESERVER_id = nextTypeIndex++;
        {
            u16* typeLength = (u16*)(outData + outOffset);
            outOffset += 2;
            WRITE(LeafType, LF_STRING_ID); // leaf type

            // TODO: Does age or guid need to be some special values
            memcpy(outData + outOffset, typeInformation.guid, sizeof(typeInformation.guid));
            outOffset += 16;
            WRITE(u32, typeInformation.age);

            const char* pdbpath = "C:\\Users\\datao\\Desktop\\Backup\\CodeProjects\\BetterThanBatch\\bin\\dev.pdb";
            strcpy((char*)outData + outOffset, pdbpath);
            outOffset += strlen(pdbpath) + 1;
            
            *typeLength = outOffset - sizeof(*typeLength) - ((u64)typeLength - (u64)outData);
        }
        debugTSection->SizeOfRawData = outOffset - debugTSection->PointerToRawData;
        ALIGN4
        
        /*
            Debug symbols
        */
        funcSymbols._reserve(program->debugInformation->functions.size());
        debugSSection->PointerToRawData = outOffset;
        
        // TODO: You may want to move some of this code into PDBWriter
        // 4-byte alignment per sub section
        // if((outOffset & 3) != 0)
        //     outOffset += 4 - outOffset & 3;

        WRITE(u32, CV_SIGNATURE_C13);
        
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
            *(SubSectionType*)(outData + outOffset) = SubSectionType::DEBUG_S_STRINGTABLE;
            outOffset += 4;
            u32* sectionLength = (u32*)(outData + outOffset);
            outOffset += 4;
            
            u32 startOffset = outOffset;

            WRITE(char, 0); // empty string, C object files has them
            
            for(int i=0;i<di->files.size();i++) {
                auto& file = di->files[i];
                fileStringIndex[i] = outOffset - startOffset;
                // TODO: Won't work if file path is absolute.
                std::string absPath = engone::GetWorkingDirectory();
                absPath += "\\";
                absPath += file;
                strcpy((char*)outData + outOffset, absPath.c_str());
                outOffset += absPath.length() + 1;
            }
            *sectionLength = outOffset - sizeof(*sectionLength) - ((u64)sectionLength - (u64)outData);;
        }
        { // files
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
            *(SubSectionType*)(outData + outOffset) = SubSectionType::DEBUG_S_SYMBOLS;
            outOffset += 4;
            u32* sectionLength = (u32*)(outData + outOffset);
            outOffset += 4;

            { // OBJNAME record
                u16* recordLength = (u16*)(outData + outOffset);
                outOffset += 2;
                *(RecordType*)(outData + outOffset) = RecordType::S_OBJNAME;
                outOffset += 2;

                *(u32*)(outData + outOffset) = 0; // signature, usually 0 for some reason.
                outOffset += 4;

                // TODO: This won't work if path already is absolute.
                //   Operating systems use different ways to indicate absolute paths
                //   so you should use a class/struct for it. Like Path in Compiler.h.
                std::string absPath = engone::GetWorkingDirectory();
                absPath += "\\";
                absPath += path;

                strcpy((char*)outData + outOffset, absPath.c_str());
                outOffset += absPath.length() + 1;

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
                // comp->flags.iLanguage = 0xFF;
                comp->flags.iLanguage = CV_CFL_C; // TEMPORARY:
                
                const char* name = "BTB Compiler";
                strcpy((char*)outData + outOffset, name);
                outOffset += strlen(name) + 1;

                *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
            }
            // Not needed I think?
            // { // BUILDINFO record
            //     u16* recordLength = (u16*)(outData + outOffset);
            //     outOffset += 2;
            //     *(RecordType*)(outData + outOffset) = RecordType::S_BUILDINFO;
            //     outOffset += 2;

            //     WRITE(u32, BUILDINFO_id);

            //     *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
            // }
            for(int i=0;i<di->functions.size();i++) {
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
                    WRITE(u32, fun.srcStart) // dbgstart
                    WRITE(u32, fun.srcEnd) // dbgend
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

                    *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                }
                {
                    u16* recordLength = (u16*)(outData + outOffset);
                    outOffset += 2;
                    *(RecordType*)(outData + outOffset) = RecordType::S_FRAMEPROC;
                    outOffset += 2;

                    WRITE(u32, 0) // size of frame
                    WRITE(u32, 0) // padding in frame
                    WRITE(u32, 0) // padding offset
                    WRITE(u32, 0) // bytes for saved registers
                    WRITE(u32, 0) // exception handler offset
                    WRITE(u16, 0) // excpetion handler id
                    WRITE(u32, 0) // flags
                    
                    // These might need to bes in the flags but I am not sure.
                    // u32 encodedLocalBasePointer : 2;  // record function's local pointer explicitly.
                    // u32 encodedParamBasePointer : 2;  // record function's parameter pointer explicitly.

                    *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                }
                for(int j=0;j<fun.localVariables.size();j++){
                    auto& var = fun.localVariables[j];
                    {
                        u16* recordLength = (u16*)(outData + outOffset);
                        outOffset += 2;
                        *(RecordType*)(outData + outOffset) = RecordType::S_REGREL32;
                        outOffset += 2;

                        WRITE(u32, var.frameOffset); // offset in frame
                        // TODO: Don't always use int as type
                        WRITE(u32, T_INT4); // type
                        WRITE(u16, CV_AMD64_RBP); // Object files from C uses AMD64 instead of x64

                        strcpy((char*)outData + outOffset, var.name.c_str());
                        outOffset += var.name.length() + 1;

                        *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                    }
                }
                {
                    u16* recordLength = (u16*)(outData + outOffset);
                    outOffset += 2;
                    *(RecordType*)(outData + outOffset) = RecordType::S_PROC_ID_END;
                    outOffset += 2;

                    *recordLength = outOffset - sizeof(*recordLength) - ((u64)recordLength - (u64)outData);
                }
            }

            *sectionLength = outOffset - sizeof(*sectionLength) - ((u64)sectionLength - (u64)outData);;
        }
        { // lines
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
                    data.offset = line.funcOffset;
                    data.linenumStart = line.lineNumber;
                    data.deltaLineEnd = 0;
                    WRITE(CV_Line_t, data);
                }
                *sectionLength = outOffset - sizeof(*sectionLength) - ((u64)sectionLength - (u64)outData);;
            }
        }

        // 4-byte alignment per sub section
        // if((outOffset & 3) != 0)
        //     outOffset += 4 - outOffset & 3;
        
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

        #undef WRITE
    }

    // Assert(false); // fix debug relocations for functions!
    
    if((outOffset & 3) != 0)  outOffset += 4 - outOffset & 3;

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

    header->PointerToSymbolTable = outOffset;
    for (int i=0;i<dataSymbols.size();i++) {
        header->NumberOfSymbols++; //incremented when adding relocations
        Symbol_Record* symbol = (Symbol_Record*)(outData+outOffset);
        outOffset+=Symbol_Record::SIZE;
        CHECK
        sprintf(symbol->Name.ShortName,"$d%u",i);
        symbol->SectionNumber = dataSectionNumber;
        symbol->Value = dataSymbols[i];
        symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        // symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_STATIC;
        symbol->Type = 0;
        symbol->NumberOfAuxSymbols = 0;
    }

    int stringTableOffset = 4; // 4 because the integer for the string table size is included
    for (int i=0;i<namedSymbols.size();i++) {
        header->NumberOfSymbols++; //incremented when adding relocations
        Symbol_Record* symbol = (Symbol_Record*)(outData+outOffset);
        outOffset+=Symbol_Record::SIZE;
        CHECK
        auto& name = namedSymbols[i];
        if(name.length()<=8) {
            strcpy(symbol->Name.ShortName, name.c_str());
        } else {
            symbol->Name.zero = 0; // NOTE: is this okay if it's not aligned? is it gonna be really slow?
            symbol->Name.offset = stringTableOffset;
            stringTableOffset += name.length() + 1; // null termination included
        }
        symbol->SectionNumber = 0; // doesn't belong to a known section
        symbol->Value = 0; // unknown for named symbols
        symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        symbol->Type = 32; // should perhaps be a function?
        // symbol->Type = IMAGE_SYM_DTYPE_FUNCTION;
        symbol->NumberOfAuxSymbols = 0;
    }
    for (int i=0;i<funcSymbols.size();i++){
        auto& fsymbol = funcSymbols[i];
        header->NumberOfSymbols++; // incremented e
        Symbol_Record* symbol = (Symbol_Record*)(outData+outOffset);
        outOffset+=Symbol_Record::SIZE;
        CHECK
        strcpy(symbol->Name.ShortName, fsymbol.name.c_str());
        symbol->SectionNumber = textSectionNumber;
        symbol->Value = fsymbol.address; // address of function
        symbol->StorageClass = (Storage_Class)IMAGE_SYM_CLASS_EXTERNAL;
        symbol->Type = 32; // IMAGE_SYM_DTYPE_FUNCTION is a macro which evaluates to 2
        symbol->NumberOfAuxSymbols = 0;
    }

    Assert(totalSymbols == header->NumberOfSymbols);

    *(u32*)(outData + outOffset) = stringTableOffset;
    outOffset += 4;
    CHECK
    for (int i=0;i<namedSymbols.size();i++) {
        auto& name = namedSymbols[i];
        if(name.length()<=8) {
            
        } else {
            strcpy((char*)outData + outOffset, name.c_str());
            outOffset += name.length()+1;
            CHECK
        }
    }

    Assert(outOffset <= outSize); // bad estimation when allocation at beginning of function
    auto file = FileOpen(path, 0, FILE_ALWAYS_CREATE);
    Assert(file);
    FileWrite(file,outData,outOffset);
    FileClose(file);
    #undef CHECK
    return true;
}