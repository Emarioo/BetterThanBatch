#include "BetBat/Util/TestSuite.h"

#include "Engone/Logger.h"

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
    const char* const testErrKey = "TEST-ERROR ";
    const int testErrKeyLen = strlen(testErrKey);
    // const char* const textKey = "text ";
    // const int textKeyLen = strlen(textKey);
    TestCase nextTest = {};
    #define FINALIZE_TEST(END_INDEX) if(nextTest.textBuffer.buffer) {\
        nextTest.textBuffer.size = (u64)buffer + END_INDEX - (u64)nextTest.textBuffer.buffer;\
        outTestCases->add(nextTest);\
        }
    bool inComment = false;
    bool inQuotes = false;
    bool inEnclosedComment = false;
    bool isSingleQuotes = false;
    int line = 1;
    int column = 1;
    int index = 0;
    while(index<(int)fileSize){
        char chr = buffer[index];
        char nextChr = 0;
        if(index + 1 < fileSize)
            nextChr = buffer[index + 1];
        const char* _debugStr = buffer + index;
        index++;
        column++;
        if(chr == '\n') {
            line++;
            column = 1;
        }
        if(inComment){
            if(inEnclosedComment){
                if(chr=='*'&&nextChr=='/'){
                    index++;
                    inComment=false;
                    inEnclosedComment=false;
                }
            }else{
                if(chr=='\n'||index==fileSize){
                    inComment=false;
                }
            }
            continue;
        } else if(inQuotes) {
            if((!isSingleQuotes && chr == '"') || 
                (isSingleQuotes && chr == '\'')){
                // Stop reading string token
                inQuotes=false;
            } else {
                if(chr=='\\'){
                    // index++; // \0 \t \n
                }
            }
            continue;
        }
        if(chr == '/' && (nextChr=='/' || nextChr=='*')) {
            inComment=true;
            if(chr=='/' && nextChr=='*'){
                inEnclosedComment=true;
            }
            index++; // skip the next slash
            continue;
        } else if(chr == '\'' || chr=='"'){
            inQuotes = true;
            isSingleQuotes = chr == '\'';
            continue;
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
                nextTest.expectedErrors.cleanup();
            }
        } else if(fileSize - index >= testErrKeyLen && !strncmp(buffer + index, testErrKey, testErrKeyLen)) {
            if(!nextTest.textBuffer.buffer)
                continue;
            index += testErrKeyLen;
            column += testErrKeyLen;
            
            int start = index;
            int testLine = line;
            
            while (index<fileSize){
                char chr = buffer[index];
                index++;
                column++;
                if(chr == '\n') {
                    line++;
                    column = 1;
                }
                if(chr == '\n' || chr == '\r' || chr == ' ')
                    break;
            }
            int len = index - 1 - start;
            char errBuffer[256];
            Assert(sizeof(errBuffer) > len);
            memcpy(errBuffer, buffer + start, len);
            errBuffer[len] = '\0';
            CompileError errType = ToCompileError(errBuffer);
            nextTest.expectedErrors.add({errType, (u32)testLine});
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

u32 TestSuite(TestSelection testSelection){
    using namespace engone;
    DynamicArray<std::string> tests;

    if(testSelection&TEST_ARITHMETIC) {
        tests.add("tests/simple/operations.btb");
    }
    if(testSelection&TEST_FLOW) {
        tests.add("tests/flow/loops.btb");
        tests.add("tests/flow/switch.btb");
        tests.add("tests/funcs/overloading.btb");
        // tests.add("tests/flow/defer.btb"); not fixed yet
    }

    return VerifyTests(tests);
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
u32 VerifyTests(DynamicArray<std::string>& filesToTest){
    using namespace engone;

    // We run out of profilers contexts fast and a message about is printed
    // so we disable profiling to prevent that. When contexts can be reused
    // we can stop disabling the profiler here.
    ProfilerEnable(false);

    DynamicArray<TestCase> testCases{};
    DynamicArray<TestOrigin> testOrigins{};
    testOrigins.resize(filesToTest.size());

    for (auto& file : filesToTest) {
        ParseTestCases(file, &testOrigins, &testCases);
    }
    log::out << log::GOLD << "Test cases ("<<testCases.size()<<"):\n";
    for(auto& testCase : testCases){
        log::out << " "<<testCase.testName << "["<<testCase.textBuffer.size<<" B]: "<<BriefString(testCase.textBuffer.origin,17)<<")\n";
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

    Interpreter interpreter{};
    for(int i=0;i<testCases.size();i++) {
        auto& testcase = testCases[i];

        CompileOptions options{};
        options.silent=true;
        options.target = BYTECODE;
        options.executeOutput = true;
        if(!useInterp)
            options.target = WINDOWS_x64;
        // options.initialSourceFile = testcase.textBuffer.origin;
        options.initialSourceBuffer = testcase.textBuffer;
        // options.initialSourceBufferSize = testcase.size


        // TODO: Run bytecode and x64 version by default.
        //   An argument can be passed to this function if you just want one target.

        Bytecode* bytecode = CompileSource(&options);
        options.silent = true;

        u64 failedTests = 0;
        u64 totalTests = 0;
        
        totalTests += testcase.expectedErrors.size();            
        for(int k=0;k<testcase.expectedErrors.size();k++){
            auto& expectedError = testcase.expectedErrors[k];
            
            bool found = false;
            for(int j=0;j<options.compileStats.errorTypes.size();j++){
                auto& actualError = options.compileStats.errorTypes[j];
                if(expectedError.errorType == actualError.errorType
                && expectedError.line == actualError.line) {
                    found = true;
                    break;
                }
            }
            if(!found)
                failedTests++;
        }
        for(int j=0;j<options.compileStats.errorTypes.size();j++){
            auto& actualError = options.compileStats.errorTypes[j];
            
            bool found = false;
            for(int k=0;k<testcase.expectedErrors.size();k++){
            auto& expectedError = testcase.expectedErrors[k];
                if(expectedError.errorType == actualError.errorType
                && expectedError.line == actualError.line) {
                    found = true;
                    break;
                }
            }
            if(!found) {
                failedTests++;
                totalTests++;
            }
        }
        
        if(!bytecode) {
            // errors will be smaller than errorTypes since errors isn't incremented when doing TEST-ERROR
            if(options.compileStats.errorTypes.size() < options.compileStats.errors) {
                log::out << log::YELLOW << "TestSuite: errorTypes: "<< options.compileStats.errorTypes.size() << ", errors: "<<options.compileStats.errors <<", they should be equal\n";
            }
        } else {
            if(useInterp) {
                interpreter.reset();
                interpreter.silent = true;
                interpreter.execute(bytecode);
            } else {
                options.outputFile = "bin/temp.exe";
                bool yes = ExportTarget(&options, bytecode);
                if(!yes)
                    continue;

                // IMPORTANT: The program will freeze if the pipe buffer is filled since there ise
                //   no space to write to. You would need a thread which processes the buffer while
                //   the program runs to prevent this.
                
                std::string hoho{};
                hoho += options.outputFile.text;
                int errorCode = 0;
                engone::StartProgram((char*)hoho.data(),PROGRAM_WAIT,&errorCode, {}, {}, PipeGetWrite(pipe));
                // log::out << "Error level: "<<errorCode<<"\n";
            }
            Bytecode::Destroy(bytecode);
            bytecode = nullptr;

            failedLocations.resize(0);
            while(true){
                char tinyBuffer[4]{0};
                PipeWrite(pipe, tinyBuffer, 4); // we must write some data to the pipe to prevent PipeRead from freezing.
                
                u64 readBytes = PipeRead(pipe, buffer, bufferSize);

                Assert(readBytes%4==0);

                totalTests += (readBytes-sizeof(tinyBuffer))/4;
                int j = sizeof(tinyBuffer);
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
        }
        finalTotalTests += totalTests;
        finalFailedTests += failedTests;

        if(failedTests == 0)
            log::out << log::LIME<<"Success ";
        else
            log::out << log::RED<<"Failure ";
        if(totalTests == 0)
            log::out << "'"<<testcase.testName << "': 100.0% (0/0)";
        else
            log::out << "'"<<testcase.testName << "': "<<(100.0f*(float)(totalTests-failedTests)/(float)totalTests)<<"% ("<<(totalTests-failedTests)<<"/"<<totalTests<<")";
        log::out << "\n";
        for(auto& ind : failedLocations){
            auto loc = options.getTestLocation(ind);
            log::out <<log::RED<< "  "<<loc->file<<":"<<loc->line<<":"<<loc->column<<"\n";
        }

    }
    if(testCases.size()>1){
        if(finalFailedTests == 0)
            log::out << log::LIME<<"Summary: ";
        else
            log::out << log::RED<<"Summary: ";
        if(finalTotalTests == 0)
            log::out << "100.0% (0/0)";
        else
            log::out <<(100.0f*(float)(finalTotalTests-finalFailedTests)/(float)finalTotalTests)<<"% ("<<(finalTotalTests-finalFailedTests)<<"/"<<finalTotalTests<<")";
        log::out << "\n";
    }
    engone::PipeDestroy(pipe);

    engone::Free(buffer,bufferSize);

    for(auto& origin : testOrigins) {
        engone::Free(origin.buffer, origin.size);
    }
    return finalFailedTests;
}