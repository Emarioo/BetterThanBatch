#include "BetBat/Util/TestSuite.h"

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) { CONTENT }

#define ERR_HEAD_SUITE() log::out << log::RED << "TestSuite: "<<log::SILVER;
#define ERR_HEAD_SUITE_L() log::out << log::RED << "TestSuite "<<line <<":"<<column<<": "<<log::SILVER;
#define ERR_MSG_SUITE(MSG) log::out << log::SILVER << MSG << "\n";

void ParseTestCases(std::string path,  DynamicArray<TestOrigin>* outTestOrigins, DynamicArray<TestCase>* outTestCases){
    using namespace engone;
    Assert(outTestCases);
    Assert(outTestOrigins);
    u64 fileSize = 0;
    auto file = engone::FileOpen(path, &fileSize, FILE_ONLY_READ);
    if(!file) {
        ERR_SECTION(
            ERR_HEAD_SUITE()
            ERR_MSG_SUITE("Cannot open '"<<path<<"'.")
        )
        return;
    }
    char* buffer = (char*)engone::Allocate(fileSize);
    Assert(buffer);
    bool yes = engone::FileRead(file, buffer, fileSize);
    Assert(yes);
    engone::FileClose(file);

    TestOrigin origin{};
    origin.buffer = buffer;
    origin.size = fileSize;
    outTestOrigins->add(origin);

    // TODO: Not handling comments or quotes. Expect wierd things if @ is
    //   found in one.
    const char* const testKey = "TEST-CASE ";
    const int testKeyLen = strlen(testKey);
    // const char* const textKey = "text ";
    // const int textKeyLen = strlen(textKey);
    TestCase nextTest = {};
    #define FINALIZE_TEST(END_INDEX) if(nextTest.textBuffer.buffer) {\
        nextTest.textBuffer.size = (u64)buffer + END_INDEX - (u64)nextTest.textBuffer.buffer;\
        outTestCases->add(nextTest);\
        }
    int line = 1;
    int column = 1;
    int index = 0;
    while(index<(int)fileSize){
        char chr = buffer[index];
        index++;
        column++;
        if(chr == '\n') {
            line++;
            column = 1;
        }

        if(chr != '@')
            continue;

        if(fileSize - index >= testKeyLen && !strncmp(buffer + index, testKey, testKeyLen)) {
            FINALIZE_TEST(index-1)
            
            index += testKeyLen;
            column += testKeyLen;

            int startIndex = -1;
            int nameLen = 0;
            bool hadLineFeed = false;
            while (index<fileSize){
                char chr = buffer[index];
                index++;
                column++;
                if(chr == '\n') {
                    line++;
                    column = 1;
                }
                if(chr == '\n') {
                    hadLineFeed=true;
                    break;
                }
                if(chr == ' ' || chr == '\t' || chr == '\r') {
                    if (startIndex!=-1)
                        break;
                    continue;
                }
                if(startIndex==-1)
                    startIndex = index-1;
                nameLen++;
            }
            if(!hadLineFeed){
                while (index<fileSize){
                    char chr = buffer[index];
                    index++;
                    column++;
                    if(chr == '\n') {
                        line++;
                        column = 1;
                    }
                    if(chr == '\n')
                        break;
                }
            }
            if(startIndex==-1){
                ERR_SECTION(
                    ERR_HEAD_SUITE_L()
                    ERR_MSG_SUITE("Bad syntax.")

                    // TODO: Print file and line
                )
            } else {
                nextTest.testName = std::string(buffer + startIndex,nameLen);
                nextTest.textBuffer.origin = path;
                nextTest.textBuffer.buffer = buffer + index;
                nextTest.textBuffer.startLine = line;
                nextTest.textBuffer.startColumn = column;
            }
        } else {
            // TODO: text insertion
            // Otherwise the @ is an annotation in the actual code
            // ERR_SECTION(
            //     ERR_HEAD_SUITE_L()
            //     ERR_MSG_SUITE("Bad syntax.")
            // )
        }
    }
    FINALIZE_TEST(index)
    #undef FINALIZE_TEST
}

void TestSuite(TestSelection testSelection){
    using namespace engone;
    DynamicArray<std::string> tests;

    if(testSelection&TEST_ARITHMETIC) {
        tests.add("tests/simple/operations.btb");
    }
    if(testSelection&TEST_FLOW) {
        tests.add("tests/flow/loops.btb");
        // tests.add("tests/flow/defer.btb"); not fixed yet
    }

    VerifyTests(tests);
}

// struct ThreadTestInfo {
//     volatile long failedTests = 0;
//     volatile long totalTests = 0;
//     engone::APIPipe pipe = {};
//     char* buffer = nullptr;
//     u64 bufferSize = 0;
//     bool stop = false;
// };
// u32 ProcessesTestvalues(void* ptr) {
//     ThreadTestInfo* info = (ThreadTestInfo*)ptr;

//     while(true){
//         if(info->stop)
//             break;
//         u64 readBytes = engone::PipeRead(info->pipe, info->buffer, info->bufferSize);

//         u64 failedTests = 0;
//         u64 totalTests = 0;
//         totalTests += readBytes;
//         for(int j=0;j<readBytes;j++){
//             failedTests += info->buffer[j] == 'x';
//         }

//         _interlockedadd(&info->totalTests,totalTests);
//         _interlockedadd(&info->failedTests,failedTests);

//         // if(readBytes != bufferSize) {
//         //     break;
//         // }
//     }
//     return 0;
// }
void VerifyTests(DynamicArray<std::string>& filesToTest){
    using namespace engone;
    DynamicArray<TestCase> testCases{};
    DynamicArray<TestOrigin> testOrigins{};
    testOrigins.resize(filesToTest.size());

    for (auto& file : filesToTest) {
        ParseTestCases(file, &testOrigins, &testCases);
    }
    log::out << log::GOLD << "Test cases ("<<testCases.size()<<"):\n";
    for(auto& testCase : testCases){
        log::out << " "<<testCase.testName << "["<<testCase.textBuffer.size<<" B]: "<<BriefPath(testCase.textBuffer.origin,17)<<")\n";
    }

    bool useInterp = false;
    // bool useInterp = true;

    // TODO: Use multithreading. Each compilation can use two threads (to trigger thread bugs) while
    //  4-8 threads are used to compile and run multiple test cases.

    u64 bufferSize = 0x10000;
    char* buffer = (char*)engone::Allocate(bufferSize);
    
    auto pipe = engone::PipeCreate(bufferSize, true, true);
    
    
    u64 finalFailedTests = 0;
    u64 finalTotalTests = 0;
    DynamicArray<u16> failedLocations{};

    for(int i=0;i<testCases.size();i++) {
        auto& testcase = testCases[i];

        CompileOptions options{};
        options.silent=true;
        options.target = BYTECODE;
        if(!useInterp)
            options.target = WINDOWS_x64;
        // options.initialSourceFile = testcase.textBuffer.origin;
        options.initialSourceBuffer = testcase.textBuffer;
        // options.initialSourceBufferSize = testcase.size;
        Bytecode* bytecode = CompileSource(&options);
        options.silent = true;

        // TODO: Run bytecode and x64 version by default.
        //   An argument can be passed to this function if you just want one target.

        if(!bytecode)
            continue;

        APIFile prevStandard = GetStandardErr();
        SetStandardErr(PipeGetWrite(pipe));

        if(useInterp) {
            RunBytecode(&options, bytecode);

        } else {
            options.outputFile = "bin/temp.exe";
            bool yes = ExportTarget(&options, bytecode);
            if(!yes)
                continue;

            // IMPORTANT: The program may freeze when testing values if the pipe buffer
            //  is filled. The fwrite function will freeze untill more bytes can be written.
            //  You can create a thread here which reads the pipe and stops when the program
            //  exits.
            
            std::string hoho{};
            hoho += options.outputFile.text;
            int errorCode = 0;
            engone::StartProgram((char*)hoho.data(),PROGRAM_WAIT,&errorCode);
            // log::out << "Error level: "<<errorCode<<"\n";

        }
        failedLocations.resize(0);
        u64 failedTests = 0;
        u64 totalTests = 0;
        while(true){
            u64 readBytes = PipeRead(pipe, buffer, bufferSize);

            Assert(readBytes%4==0);

            totalTests += readBytes/4;
            int j=0;
            while(j<readBytes){
                failedTests += buffer[j] == 'x';
                if(buffer[j] == 'x'){
                    u16 index = ((u16)buffer[j+2]<<8) | ((u16)buffer[j+3]);
                    failedLocations.add(index);
                }
                j+=4;
            }

            if(readBytes != bufferSize) {
                break;
            }
        }
        finalTotalTests += totalTests;
        finalFailedTests += failedTests;
        
        SetStandardErr(prevStandard);

        if(failedTests == 0)
            log::out << log::LIME<<"Success ";
        else
            log::out << log::RED<<"Failure ";
        log::out << "'"<<testcase.testName << "': "<<(100.0f*(float)(totalTests-failedTests)/(float)totalTests)<<"% ("<<(totalTests-failedTests)<<"/"<<totalTests<<")";
        log::out << "\n";
        for(auto& ind : failedLocations){
            auto loc = options.getTestLocation(ind);
            log::out <<log::RED<< "  "<<loc->file<<":"<<loc->line<<":"<<loc->column<<"\n";
        }

        Bytecode::Destroy(bytecode);
    }
    if(testCases.size()>1){
        if(finalFailedTests == 0)
            log::out << log::LIME<<"Summary: ";
        else
            log::out << log::RED<<"Summary: ";
        log::out <<(100.0f*(float)(finalTotalTests-finalFailedTests)/(float)finalTotalTests)<<"% ("<<(finalTotalTests-finalFailedTests)<<"/"<<finalTotalTests<<")";
        log::out << "\n";
    }
    engone::PipeDestroy(pipe);

    engone::Free(buffer,bufferSize);

    for(auto& origin : testOrigins) {
        engone::Free(origin.buffer, origin.size);
    }
}