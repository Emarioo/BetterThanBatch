#include "BetBat/TestSuite.h"

#include "Engone/Logger.h"
#include "Engone/PlatformLayer.h"
#include "Engone/Util/Stream.h"

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) { CONTENT }

#define ERR_HEAD_SUITE() log::out << log::RED << "TestSuite: "<<log::NO_COLOR;
#define ERR_HEAD_SUITE_L() log::out << log::RED << "TestSuite "<<line <<":"<<column<<": "<<log::NO_COLOR;
#define ERR_MSG_SUITE(MSG) log::out << log::NO_COLOR << MSG << "\n";

void ParseTestCases(std::string path,  DynamicArray<TestOrigin>* outTestOrigins, DynamicArray<TestCase>* outTestCases){
    using namespace engone;
    Assert(outTestCases);
    Assert(outTestOrigins);
    u64 fileSize = 0;
    auto file = engone::FileOpen(path, FILE_READ_ONLY, &fileSize);
    if(!file) {
        ERR_SECTION(
            ERR_HEAD_SUITE()
            ERR_MSG_SUITE("Cannot open '"<<path<<"'.")
        )
        return;
    }
    if(fileSize == 0) {
        // nothing to test
        engone::FileClose(file);
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

    const char* const testKey = "TEST_CASE";
    const int testKeyLen = strlen(testKey);
    const char* const testErrKey = "TEST_ERROR";
    const int testErrKeyLen = strlen(testErrKey);
    
    TestCase nextTest = {};
    #define FINALIZE_TEST(END_INDEX) if(nextTest.textBuffer.buffer) {\
        nextTest.textBuffer.size = (u64)buffer + END_INDEX - (u64)nextTest.textBuffer.buffer;\
        outTestCases->add(nextTest);\
        }
    bool inComment = false;
    bool inQuotes = false;
    bool inEnclosedComment = false;
    bool isSingleQuotes = false;
    int nested_comment_depth = 0;
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
                    nested_comment_depth--;
                    if(nested_comment_depth == 0) {
                        inComment=false;
                        inEnclosedComment=false;
                    }
                }
                if(chr=='/'&&nextChr=='*'){
                    index++;
                    nested_comment_depth++;
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
                nested_comment_depth = 1;
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

            // TODO: we hope that we skip ( but we shouldn't
            index++; // skip (

            int depth = 0;
            int startIndex = -1;
            int nameLen = 0;
            // bool hadLineFeed = false;
            while (index<fileSize){
                char chr = buffer[index];
                index++;
                column++;

                if(chr == '(') {
                    depth++;
                }

                if(chr == '\n') {
                    line++;
                    column = 1;
                }
                if(chr == ')') {
                    if(depth == 0) {
                        break;
                    }
                    depth--;
                }
                if(chr == '\n') {
                    // hadLineFeed=true;
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
            // if(!hadLineFeed){
            //     while (index<fileSize){
            //         char chr = buffer[index];
            //         index++;
            //         column++;
            //         if(chr == '\n') {
            //             line++;
            //             column = 1;
            //         }
            //         if(chr == '\n')
            //             break;
            //     }
            // }
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

            // TODO: we hope that we skip ( but we shouldn't
            index++; // skip (
            
            int start = index;
            int end = index;
            int depth = 0;
            
            while (index<fileSize){
                char chr = buffer[index];
                index++;
                column++;


                if(chr == '\n' || chr == ',' || ( depth == 0 && chr == ')')) {
                    // add test
                    int len = end - start;
                    if(len > 0)  {
                        char errBuffer[256];
                        Assert(sizeof(errBuffer) > len);
                        memcpy(errBuffer, buffer + start, len);
                        errBuffer[len] = '\0';
                        CompileError errType = ToCompileError(errBuffer);
                        nextTest.expectedErrors.add({errType, (u32)line});

                        start = index;
                        end = index;
                    }
                }

                if(chr == '(') {
                    depth++;
                }
                if(chr == ')') {
                    if(depth == 0) {
                        break;
                    }
                    depth--;
                }
                if(chr == '\n') {
                    line++;
                    column = 1;
                    break;
                }
                if(chr != ' ') {
                    end = index;
                    if(buffer[start] == ' ')
                        start = index;
                }
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

u32 TestSuite(CompileOptions* options){
    using namespace engone;
    DynamicArray<std::string> tests;

    tests.add("tests/simple/operations.btb");
    tests.add("tests/simple/assignment.btb");
    tests.add("tests/simple/exceptions.btb");
        
    tests.add("tests/flow/loops.btb");
    tests.add("tests/flow/switch.btb");
    tests.add("tests/flow/defer.btb");
    tests.add("tests/inline-asm/simple.btb");
    tests.add("tests/polymorphism/structs.btb");
        
    tests.add("tests/structs/basic.btb");
    tests.add("tests/structs/struct.btb");
    tests.add("tests/lang/typeinfo.btb");
    tests.add("tests/modules/test_maps.btb");

    tests.add("tests/funcs/overloading.btb");
    tests.add("tests/funcs/constructor.btb");
    tests.add("tests/funcs/conventions.btb");
    tests.add("tests/funcs/func_ptr.btb");
    
    return VerifyTests(options, tests);
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
u32 VerifyTests(CompileOptions* user_options, DynamicArray<std::string>& filesToTest) {
    using namespace engone;

    // We run out of profilers contexts fast and a message about is printed
    // so we disable profiling to prevent that. When contexts can be reused
    // we can stop disabling the profiler here.
    // ProfilerEnable(false);

    DynamicArray<TestCase> testCases{};
    DynamicArray<TestOrigin> testOrigins{};
    testOrigins.resize(filesToTest.size());

    defer {
        for(auto& origin : testOrigins) {
            engone::Free(origin.buffer, origin.size);
        }
    };
    for (auto& file : filesToTest) {
        ParseTestCases(file, &testOrigins, &testCases);
    }
    
    // NOTE: We always try to load cache so that a user can run 'btb --test' first and then 'btb --test -ct' without having
    //   to rerun all tests again because we cache on 'btb --test'.
    CachedTests cached_tests{};
    std::string cache_path = "bin/cached_tests.dat";
    bool has_cache = LoadTests(cache_path, &cached_tests);
    if(has_cache) {
        if(user_options->cache_tests) {
            log::out << log::GRAY << "Skipping "<<cached_tests.tests.size()<<" tests in cache\n";
            for(int i=0;i<testCases.size();i++) {
                auto& testcase = testCases[i];
                u64 origin_timestamp;
                bool has_timestamp = FileLastWriteTimestamp_us(testcase.textBuffer.origin, &origin_timestamp);
                if(has_timestamp) {
                    int cached_test_index = -1;
                    for(int j=0;j<cached_tests.tests.size();j++) {
                        auto& cached_test = cached_tests.tests[j];
                        if (cached_test.path == testcase.textBuffer.origin && cached_test.name == testcase.testName) {
                            if(cached_test.timestamp >= origin_timestamp) {
                                cached_test_index = j;
                                break;
                            }
                        }
                    }
                    if(cached_test_index != -1) {
                        testCases.removeAt(i);
                        i--;
                    }
                }
            }
        }
    } else {
        // there was no cache which is fine
    }
    // } else {
    //     FileDelete(cache_path);
    // }

    // log::out << log::GOLD << "Test cases ("<<testCases.size()<<"):\n";
    // for(auto& testCase : testCases){
    //     log::out << " "<<testCase.testName << "["<<testCase.textBuffer.size<<" B]: "<<BriefString(testCase.textBuffer.origin,17)<<")\n";
    // }

    bool useInterp = false;
    // useInterp = true;
    
    if(user_options->execute_in_vm)
        useInterp = true;

    // TODO: Use multithreading. Some threads compile test cases while others start programs and test them.
    //   One thread can test multiple programs and redirect stdout to some file.

    i64 bufferSize = 0x10000;
    char* buffer = (char*)engone::Allocate(bufferSize);
    auto pipe = engone::PipeCreate(bufferSize, true, true);
    defer {
        engone::PipeDestroy(pipe);
        engone::Free(buffer,bufferSize);
    };
    
    struct TestResult {
        int totalTests;
        int failedTests;
        DynamicArray<TestCase::Error> missing_errors; // missing but expected errors
        DynamicArray<u16> failedLocations;
        DynamicArray<TestLocation> testLocations;
    };
    DynamicArray<TestResult> results;
    results.resize(testCases.size());

    i64 finalFailedTests = 0;
    i64 finalTotalTests = 0;

    auto test_startTime = engone::StartMeasure();
    bool progress_bar = false;
    
    for(int i=0;i<testCases.size();i++) {
        auto& testcase = testCases[i];
        auto& result = results[i];

        CompileOptions options{};
        options.silent = true;
        options.target = TARGET_BYTECODE;
        // options.executeOutput = false;
        options.threadCount = 1; // TODO: We should use more than one thread but multithreading is crashing at the moment.
        options.instant_report = user_options->instant_report;
        options.linker = user_options->linker;
        options.verbose = user_options->verbose;
        options.modulesDirectory = user_options->modulesDirectory;
        // if(!useInterp) {
        options.target = user_options->target;
        options.output_file = "bin/temp.exe";
        // }
        options.source_buffer = testcase.textBuffer;
        
        if(!options.instant_report) {
            // log::out << log::GOLD << "Remaining tests: "<<(testCases.size()-i)<<" " << log::LIME << BriefString(testcase.testName,20) <<"                               \r"; // <- extra space to cover over previous large numbers

            int console_w = GetConsoleWidth();
            char buffer[256];
            int len=0;
            len = snprintf(buffer,sizeof(buffer), "Remaining tests: %d ", (testCases.size()-i));
            log::out << log::GOLD << buffer;
            std::string name = BriefString(testcase.testName,20);
            log::out << log::LIME << name;
            if(progress_bar) {
                int w = len + name.length();
                for(int i=0;i<console_w - w;i++){
                    log::out << " ";
                }
                log::out << "\r";
            } else {
                log::out << "\n";
            }
            log::out.flush();
        }

        // TODO: Run bytecode and x64 version by default.
        //   An argument can be passed to this function if you just want one target.

        Compiler compiler{};
        compiler.run(&options);

        i64 failedTests = 0;
        i64 totalTests = 0;
        
        #define match_err(A,B) ((A) == (B) || A == ERROR_ANY)
        
        totalTests += testcase.expectedErrors.size();            
        for(int k=0;k<testcase.expectedErrors.size();k++){
            auto& expectedError = testcase.expectedErrors[k];
            
            bool found = false;
            for(int j=0;j<compiler.errorTypes.size();j++){
                auto& actualError = compiler.errorTypes[j];
                if(match_err(expectedError.errorType, actualError.errorType)
                && expectedError.line == actualError.line) {
                    found = true;
                    break;
                }
            }
            if(!found) {
                failedTests++;
                totalTests++;
                result.missing_errors.add(expectedError);
            }
        }
        for(int j=0;j<compiler.errorTypes.size();j++){
            auto& actualError = compiler.errorTypes[j];
            
            bool found = false;
            for(int k=0;k<testcase.expectedErrors.size();k++){
                auto& expectedError = testcase.expectedErrors[k];
                if(match_err(expectedError.errorType, actualError.errorType)
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
        
        if(compiler.compile_stats.errors > 0) {
            // errors will be smaller than errorTypes since errors isn't incremented when doing TEST_ERROR(
            if(compiler.errorTypes.size() < compiler.compile_stats.errors) {
                log::out << log::YELLOW << "TestSuite: errorTypes: "<< compiler.errorTypes.size() << ", errors: "<<compiler.compile_stats.errors <<", they should be equal\n";
                totalTests += compiler.compile_stats.errors - compiler.errorTypes.size();
                failedTests += compiler.compile_stats.errors - compiler.errorTypes.size();
            }
        } else {
            if(useInterp) {
                bool good_to_go = true;
                VirtualMachine vm{};
                
                auto tinycode = vm.fetch_tinycode(compiler.bytecode, compiler.entry_point);
                
                for(auto& tc : compiler.bytecode->tinyBytecodes) {
                    if(tc->try_blocks.size() > 0){
                        good_to_go = false;
                        log::out << log::RED << " exceptions are not supported in VM\n";
                        break;
                    }
                }
                if (good_to_go) {
                    auto prev_stdout = GetStandardOut();
                    auto prev_stderr = GetStandardErr();
                    bool yes = SetStandardOut(PipeGetWrite(pipe));
                    bool yes2 = SetStandardErr(PipeGetWrite(pipe));
                    
                    vm.silent = true;
                    vm.execute(compiler.bytecode, compiler.entry_point);
                    
                    SetStandardOut(prev_stdout);
                    SetStandardErr(prev_stderr);
                }
            } else {
                // TODO: Check if x64 failed, check if exe was produced

                // IMPORTANT: The program will freeze if the pipe buffer is filled since there ise
                //   no space to write to. You would need a thread which processes the buffer while
                //   the program runs to prevent this.
                
                std::string hoho{};
                hoho += options.output_file;
                int exitCode = 0;
                bool crashed = false;
                // auto file = engone::FileOpen("ya",nullptr, engone::FILE_CLEAR_AND_WRITE);
                {
                    // MEASURE_WHO("Test: StartProgram")
                    engone::StartProgram((char*)hoho.data(),PROGRAM_WAIT,&exitCode, &crashed, {}, {}, PipeGetWrite(pipe));
                    // TODO: Check whether program crashed but how do we do that on Windows and Unix?
                    //  the program may not output a 0 as exit code? Maybe we force a zero in there when parsing?
                    //  Another option is to check for error codes. Usually negative but the program may output negative numbers too.
                    //  Specific negative numbers may work.
                    //  By the way, if a program crashed, that should count as a failed test.
                    #ifdef OS_WINDOWS
                    std::string msg = StringFromExitCode(exitCode);
                    if(msg.size()) {
                        log::out << log::RED << msg<<"\n";
                    #else
                    if(exitCode < 0) { // negative numbers represents an error or crash on Unix
                        log::out << log::RED << "Exit code was negative, error?\n";
                    #endif
                        failedTests++;
                        totalTests++;
                    } else if(crashed) {
                        failedTests++;
                        totalTests++;
                    }
                }
            }
            {
                // MEASURE_WHO("Test: Pipe reading")
                result.failedLocations.resize(0);
                while(true){
                    char tinyBuffer[4]{0};
                    PipeWrite(pipe, tinyBuffer, 4); // we must write some data to the pipe to prevent PipeRead from freezing if nothing was written to the pipe.
                    
                    u64 readBytes = PipeRead(pipe, buffer, bufferSize);
                    if (readBytes < 0) {
                        log::out << "pipe error "<<testcase.testName<<"\n";
                        break;
                    }
                    readBytes -= sizeof(tinyBuffer);

                    if(readBytes%4==0) {
                        // good
                    } else {
                        Assert(failedTests);
                        break;
                    }

                    totalTests += readBytes/4;
                    int j = 0;
                    while(j<readBytes){
                        failedTests += buffer[j] != 'x';
                        if(buffer[j] != 'x'){
                            u32 index = ((u32)buffer[j+3]<<16) | ((u32)buffer[j+2]<<8) | ((u32)buffer[j+1]);
                            result.failedLocations.add(index);
                        }
                        j+=4;
                    }

                    if(readBytes != bufferSize) {
                        break;
                    }
                }
            }
        }
        finalTotalTests += totalTests;
        // log::out << testcase.testName << " add " << failedTests<<"\n";
        finalFailedTests += failedTests;

        result.totalTests = totalTests;
        result.failedTests = failedTests;
        result.testLocations.steal_from(compiler.testLocations);
    }
    // NOTE: We do cache logic even if it's not active because user may type btb --test and then btb --test -ct, if so
    //   we want to cache on the first test run so that user doesn't have to sit through passed tests again even if they didn't
    //   specify caching first time.
    // if(user_options->cache_tests)
    for(int i=0;i<testCases.size();i++) {
        auto& testcase = testCases[i];
        auto& result = results[i];

        if(result.failedTests == 0) {
            int cached_index = -1;
            for(int j=0;j<cached_tests.tests.size();j++) {
                auto& ct = cached_tests.tests[j];
                if(ct.path == testcase.textBuffer.origin && ct.name == testcase.testName) {
                    cached_index = j;
                    break;
                }
            }
            if(cached_index == -1) {
                cached_tests.tests.add({});
                auto& t = cached_tests.tests.last();
                t.path = testcase.textBuffer.origin;
                t.name = testcase.testName;
                
                u64 origin_timestamp;
                bool has_timestamp = FileLastWriteTimestamp_us(testcase.textBuffer.origin, &origin_timestamp);
                t.timestamp = origin_timestamp;
            } else {
                auto& t = cached_tests.tests[cached_index];
                
                u64 origin_timestamp;
                bool has_timestamp = FileLastWriteTimestamp_us(testcase.textBuffer.origin, &origin_timestamp);
                t.timestamp = origin_timestamp;
            }
        }
    }
    bool yes = SaveTests(cache_path, &cached_tests);
    if(!yes) {
        if(user_options->cache_tests) {
            // if caching wasn't specified then we don't print message because user doesn't care.
            log::out << log::RED << "Could not cache tests to '"<<cache_path<<"'.\n";
        }
    } else {
        log::out << log::GRAY << "Caching "<<cached_tests.tests.size()<<" tests to '"<<cache_path<<"'.\n";
    }
    
    auto test_endTime = engone::StopMeasure(test_startTime);

    if(progress_bar) {
        int console_w = GetConsoleWidth();
        for(int i=0;i<console_w;i++){
            log::out << " ";
        }
        log::out << "\r";
    }

    log::out << log::GOLD << "Test cases ("<< testCases.size() <<")\n";
    std::string lastFile = "";
    for(int i=0;i<results.size();i++) {
        auto& testCase = testCases[i];
        auto& result = results[i];
        
        if(testCase.textBuffer.origin != lastFile) {
            log::out << " "<< testCase.textBuffer.origin<<"\n";
            lastFile = testCase.textBuffer.origin;
        }
        
        log::out << "  ";
        
        if(result.failedTests == 0) {
            log::out << log::LIME << "OK ";
            log::out << log::GRAY;
        } else {
            log::out << log::RED << "NO ";
        }
        
        /* TODO: The way we print a case failed because we didn't receive
             the expected error is confusing. It looks like the case
             failed because there was an error, see below and fix it.
             
            tests/structs/struct.btb
             OK assign_add (0/0)
             OK array_in_struct (7/7)
             OK indexing_slice (1/1)
             OK methods (1/1)
             OK methods_poly (0/0)
             NO nested_struct 50.00% (1/2)
              LN 167: ERROR_ANY
             OK circular_structs (0/0)
            Summary: 92.86% (13/14)
        */
        log::out << log::NO_COLOR << testCase.testName << " ";
        if(result.failedTests != 0) {
            log::out << log::RED;
        } else {
            log::out << log::GRAY;
        }
        if(result.totalTests == 0)
            log::out << "(0/0)";
        else {
            if(result.failedTests != 0)
                log::out << (100.0f*(float)(result.totalTests-result.failedTests)/(float)result.totalTests)<<"% ";
            log::out << "("<<(result.totalTests-result.failedTests)<<"/"<<result.totalTests<<")";
        }
        log::out << "\n";
        
        for(auto& ind : result.failedLocations){
            auto loc = result.testLocations.getPtr(ind);
            if(!loc) {
                log::out << log::RED<<"Test tools are broken. (BC_TEST_VALUE may be converted to bad x64, or VirtualMachine doesn't handle them properly)\n";
                break;
            }
            log::out <<log::RED<< "   "<<loc->file<<":"<<loc->line<<":"<<loc->column<<"\n";
        }
        for(auto& err : result.missing_errors){
            log::out <<log::RED<< "   "<<"LN "<<err.line << ": "<<ToCompileErrorString({false, err.errorType})<<"\n";
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
        log::out << log::GRAY << "  Time: "<< FormatTime(test_endTime) << "\n";
    }

    return finalFailedTests;
}

bool SaveTests(const std::string& path, CachedTests* tests) {
    using namespace engone;
    ByteStream* stream = ByteStream::Create(nullptr);
    defer { ByteStream::Destroy(stream, nullptr); };
    
    stream->write4((int)tests->tests.size());
    for(auto& test : tests->tests) {
        stream->write2((int)test.path.size());
        stream->write(test.path.data(), test.path.size());
        stream->write2((int)test.name.size());
        stream->write(test.name.data(), test.name.size());
        stream->write8(test.timestamp);
    }
    
    void* ptr;
    u32 size;
    bool yes = stream->finalize(&ptr, &size);
    
    auto file = FileOpen(path, FILE_CLEAR_AND_WRITE);
    if(!file) {
        return false;
    }
    FileWrite(file, ptr, size);
    FileClose(file);
    return true;
}
bool LoadTests(const std::string& path, CachedTests* tests) {
    using namespace engone;
    ByteStream* stream = ByteStream::Create(nullptr);
    defer { ByteStream::Destroy(stream, nullptr); };
    
    u64 filesize;
    auto file = FileOpen(path, FILE_READ_ONLY, &filesize);
    if(!file) {
        return false;
    }
    defer { FileClose(file); };
    void* data = stream->prepare_written_bytes(filesize);
    
    if(!data) {
        return false;
    }
    FileRead(file, data, filesize);
    
    tests->tests.cleanup();
    
    int offset = 0;
    int test_count = stream->read4(offset);
    offset += 4;
    tests->tests.resize(test_count);
    for(auto& test : tests->tests) {
        int len = stream->read2(offset);
        offset += 2;
        test.path.resize(len);
        stream->read(offset, (void*)test.path.data(), test.path.size());
        offset += len;
        
        len = stream->read2(offset);
        offset += 2;
        test.name.resize(len);
        stream->read(offset, (void*)test.name.data(), test.name.size());
        offset += len;
        
        test.timestamp = stream->read8(offset);
        offset += 8;
    }
    
    return true;
}