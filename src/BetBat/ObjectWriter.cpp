#include "BetBat/ObjectWriter.h"
#include "Engone/Logger.h"
#include "BetBat/Util/Utility.h"

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
    engone::Free(objectFile,sizeof(ObjectFile));
}
ObjectFile* ObjectFile::DeconstructFile(const std::string& path) {
    using namespace engone;
    using namespace COFF_Format;
    u64 fileSize=0;
    // std::string filename = "obj_test.o";
    // std::string filename = "obj_test.obj";
    auto file = FileOpen(path,&fileSize,FILE_ONLY_READ);
    if(!file)
        return nullptr;
    u8* filedata = (u8*)engone::Allocate(fileSize);
    // defer {engone::Free(filedata,fileSize); };

    FileRead(file,filedata,fileSize);
    FileClose(file);
    log::out << "Read file "<<path << ", size: "<<fileSize<<"\n";

    ObjectFile* objectFile = (ObjectFile*)engone::Allocate(sizeof(ObjectFile));
    new(objectFile)ObjectFile();
    objectFile->_rawFileData = filedata;
    objectFile->fileSize = fileSize;

    u64 fileOffset = 0;

    Assert(fileSize-fileOffset>=COFF_File_Header::SIZE);
    COFF_File_Header* coffHeader = (COFF_File_Header*)filedata;
    fileOffset = COFF_File_Header::SIZE;

    objectFile->header = coffHeader;

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

    fileOffset += coffHeader->SizeOfOptionalHeader;
    if(coffHeader->SizeOfOptionalHeader!=0){
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
            log::out << log::LIME<<"SECTION HEADER: ";
            if(baselog->Name[0]=='/'){
                u32 offset = 0;
                for(int i=1;8;i++) {
                    if(baselog->Name[i]==0)
                        break;
                    Assert(baselog->Name[i]>='0'&&baselog->Name[i]<='9');
                    offset = offset*10 + baselog->Name[i] - '0';
                }
                log::out <<((char*)stringTablePointer + offset)<<"\n";
            } else {
                for(int i=0;i<8;i++) {
                    if(0==baselog->Name[i])
                        break; 
                    log::out << baselog->Name[i];
                }
                log::out << " \n";
            }
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
        for(int i=0;i<sectionHeader->NumberOfRelocations;i++){
            COFF_Relocation* relocation = (COFF_Relocation*)(objectFile->_rawFileData + sectionHeader->PointerToRelocations + i*COFF_Relocation::SIZE);
            log::out << log::LIME << " relocation "<<i<<"\n";
            auto baselog = relocation;
            LOGIT(VirtualAddress)
            LOGIT(SymbolTableIndex)
            LOGIT(Type)
        }
    }

    log::out << log::LIME<<"Symbol table\n";
    int skipAux = 0;
    Storage_Class previousStorageClass = (Storage_Class)0;
    for(int i=0;i<coffHeader->NumberOfSymbols;i++){
        u32 symbolOffset = coffHeader->PointerToSymbolTable + i * Symbol_Record::SIZE;
        Assert(fileSize>=symbolOffset+Symbol_Record::SIZE);
        Symbol_Record* symbolRecord = (Symbol_Record*)(filedata + symbolOffset);

        objectFile->symbols.add(symbolRecord);
        if(skipAux==0){
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

            previousStorageClass = symbolRecord->StorageClass;
            skipAux+=symbolRecord->NumberOfAuxSymbols; // skip them for now
        } else {
            if (previousStorageClass==IMAGE_SYM_CLASS_STATIC){
                auto baselog = (Aux_Format_5*)symbolRecord;
                log::out << log::LIME << " Auxilary Format 5 (index "<<i<<")\n";
                LOGIT(Length)
                LOGIT(NumberOfRelocations)
                LOGIT(NumberOfLineNumbers)
                LOGIT(CheckSum)
                LOGIT(Number)
                LOGIT(Selection)
            }
            skipAux--;
        }
    }
    
    log::out << log::LIME<<"String table ";
    log::out << log::LIME<<" ("<<stringTableSize<<" bytes)\n";
    for(int i=4;i<stringTableSize;i++){
        if(*(u8*)(stringTablePointer+i)==0)
            log::out << " ";
        else
            log::out << *(char*)(stringTablePointer+i);
    }
    log::out << "\n";
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

    struct SectionIndexSwap {
        int oldIndex=0;
        int newIndex=0;
    };
    DynamicArray<SectionIndexSwap> sectionSwaps;

    int decr=0;
    // Filter sections
    for(int i=0;i<sections.size();i++){
        auto section = sections[i];
        std::string name = getSectionName(i);
        // if(name == ".text$mn"){
        sectionSwaps.add({i,i-decr});
        if(
            // name == ".drectve" 
            // || 
            name == ".chks64" 
            || 
            name == ".debug$S"
        ){
            decr++;
        } else {
            sectionIndices.add(i);
        }
    }
    
    for(int i=0;i<sectionIndices.size();i++){
        auto section = sections[sectionIndices[i]];
        std::string name = getSectionName(sectionIndices[i]);
        
        finalSize += Section_Header::SIZE + section->SizeOfRawData + 
            section->NumberOfRelocations * COFF_Relocation::SIZE;
        // if(name.length()>8){
        //     finalSize += name.length(); // stringTableSize is added later which includes this
        // }
    }

    finalSize += symbols.size() * Symbol_Record::SIZE;

    finalSize += stringTableSize;

    log::out << "Final Size: "<<finalSize<<"\n";
    
    // finalSize += 16 * (2 * sectionIndices.size()); // potential alignment for raw data and relocations in eaach section

    // log::out << "Final Size (aligned): "<<finalSize<<"\n";

    u8* outData = (u8*)Allocate(finalSize);
    defer { Free(outData,finalSize); };

    Assert((u64)outData % 8 == 0);

    u64 fileOffset = 0;

    COFF_File_Header* newHeader = (COFF_File_Header*)(outData + fileOffset);
    fileOffset += COFF_File_Header::SIZE;
    memcpy(newHeader, header, COFF_File_Header::SIZE);

    newHeader->NumberOfSections = sectionIndices.size();
    // newHeader->NumberOfSymbols =  // same
    newHeader->SizeOfOptionalHeader = 0;
    // newHeader->PointerToSymbolTable = 0;

    DynamicArray<Section_Header*> newSections;
    for(int i=0;i<sectionIndices.size();i++){
        auto* section = sections[sectionIndices[i]];
        
        // fileOffset = AlignOffset(fileOffset, 16); // NOTE: 16-byte alignment isn't always necessary.
        auto* newSection = (Section_Header*)(outData + fileOffset);
        fileOffset += Section_Header::SIZE;
        memcpy(newSection, section, Section_Header::SIZE);

        newSections.add(newSection);
    }

    for(int i=0;i<newSections.size();i++){
        auto* section = newSections[i];
        
        if(section->SizeOfRawData!=0){
            fileOffset = AlignOffset(fileOffset, 16);
            memcpy(outData + fileOffset, _rawFileData + section->PointerToRawData, section->SizeOfRawData);
            section->PointerToRawData = fileOffset;
            fileOffset += section->SizeOfRawData;
        }
        if(section->NumberOfRelocations!=0){
            fileOffset = AlignOffset(fileOffset, 16);
            memcpy(outData + fileOffset, _rawFileData + section->PointerToRelocations, section->NumberOfRelocations * COFF_Relocation::SIZE);
            section->PointerToRelocations = fileOffset;
            fileOffset += section->NumberOfRelocations * COFF_Relocation::SIZE;
        }
    }

    // not aligned? if it needs don't forget to add extra bytes to final size
    // fileOffset = AlignOffset(fileOffset, 16);
    memcpy(outData + fileOffset, _rawFileData + header->PointerToSymbolTable, header->NumberOfSymbols * Symbol_Record::SIZE);
    newHeader->PointerToSymbolTable = fileOffset;
    fileOffset += header->NumberOfSymbols * Symbol_Record::SIZE;

    int skipAux = 0;
    Storage_Class previousStorageClass = (Storage_Class)0;
    for(int i=0;i<newHeader->NumberOfSymbols;i++){
        u32 symbolOffset = newHeader->PointerToSymbolTable + i * Symbol_Record::SIZE;
        // Assert(fileSize>=symbolOffset+Symbol_Record::SIZE);
        Symbol_Record* symbolRecord = (Symbol_Record*)(outData + symbolOffset);

        if(skipAux==0){
            int sectionIndex = symbolRecord->SectionNumber-1;
            if(sectionIndex>=0){
                // MAGE_SYM_UNDEFINED = 0 // The symbol record is not yet assigned a section. A value of zero indicates that a reference to an external symbol is defined elsewhere. A value of non-zero is a common symbol with a size that is specified by the value.
                // IMAGE_SYM_ABSOLUTE = -1 // The symbol has an absolute (non-relocatable) value and is not an address.
                // IMAGE_SYM_DEBUG = -2 // ?
                symbolRecord->SectionNumber = sectionSwaps[sectionIndex].newIndex + 1;
            }
            // log::out << log::LIME << "Symbol "<<i<<"\n";
            // auto baselog = symbolRecord;
            // if(baselog->Name.zero==0){
            //     // log::out << " Name (offset): "<<baselog->Name.offset<<"\n";
            //     log::out << " Name (long): "<<((char*)stringTablePointer + baselog->Name.offset)<<"\n";
            // } else {
            //     log::out << " Name: ";
            //     for(int i=0;i<8;i++) {
            //         if(0==baselog->Name.ShortName[i])
            //             break;
            //         log::out << baselog->Name.ShortName[i];
            //     }
            //     log::out << "\n";
            // }

            previousStorageClass = symbolRecord->StorageClass;
            skipAux+=symbolRecord->NumberOfAuxSymbols; // skip them for now
        } else {
            if (previousStorageClass==IMAGE_SYM_CLASS_STATIC){
                auto baselog = (Aux_Format_5*)symbolRecord;
            }
            skipAux--;
        }
    }

    u64 stringTableOffset = header->PointerToSymbolTable+header->NumberOfSymbols*Symbol_Record::SIZE;
    // char* stringTablePointer = (char*)_rawFileData + stringTableOffset;
    // u32 newstringTableSize = *(u32*)(stringTablePointer);

    // fileOffset = AlignOffset(fileOffset, 16);
    memcpy(outData + fileOffset, stringTableData, stringTableSize);
    // header->PointerToSymbolTable = fileOffset;
    fileOffset += stringTableSize;

    auto file = FileOpen(path,nullptr,FILE_WILL_CREATE);
    if(!file) {
        log::out << "failed creating\n";
        return;
    }
    FileWrite(file, outData, fileOffset);
    // FileWrite(file, _rawFileData, fileSize);
    FileClose(file);
}