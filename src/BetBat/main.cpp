#include "BetBat/Compiler.h"

void print_help(){
    using namespace engone;
    log::out << log::BLUE << "##   Help   ##\n";
    log::out << log::GRAY << "More information can be found here:\n"
        "https://github.com/Emarioo/BetterThanBatch/tree/master/docs\n";
    log::out << log::YELLOW << "compiler.exe [file0 file1 ...]: "<<log::SILVER;
    log::out << "Arguments after the executable specifies source files to compile. "
             "They will compile and run seperately. Use #import for more files in your "
             "projects.\n";
    log::out << log::LIME << " Examples:\n";
    log::out << "  compiler.exe file0.btb script.txt\n";
    log::out << "\n";
    // log::out << log::YELLOW << "compiler.exe -log [type0,type1,...]: "<<log::SILVER;
    // log::out << "Prints debug info. The argument after determines what "
    //          << "type of info to print. Types are tokenizer, preprocessor, parser, "
    //          << "optimizer, interpreter and threads. Comma can be used to specify multiple. "
    //          << "Script files can be specified before and after\n";
    // log::out << log::LIME << " Examples:\n";
    // log::out << "  compiler.exe script.btb -log  "<<log::GRAY<<"(log by itself gives some basic information)\n";
    // log::out << "  -log tok          "<<log::GRAY<<"(extra info about tokenizer)\n";
    // log::out << "  -log pre,par,thr  "<<log::GRAY<<"(extra info about preprocessor, parser and threads)\n";
    // log::out << "  -log opt,int      "<<log::GRAY<<"(extra info about optimizer and interpreter)\n";
}

// void stac(int num){
//     int cool = num - 1;
//     engone::log::out << num << " " << (u64)&cool << "\n";
//     if(cool==0)
//         return;
//     int cool2 = num - 2;
//     engone::log::out << (u64)&cool2 << "\n";

//     stac(cool);
// }

int main(int argc, const char** argv){
    using namespace engone;

    // int file = open("eaeea",O_RDWR);
    // printf("file %d\n",err);

    // char buffer[1000];
    // auto fr = read("out",&size, FILE_ONLY_READ);
    // u64 readBytes = read(fr,buffer,size);
    // FileClose(fr);
    // printf("%d\n", (int)readBytes);
    // read("");
    

    // u64 size=0;
    // const char* text = "????";
    // auto fw = FileOpen("out",&size, FILE_CAN_CREATE);
    // FileWrite(fw,text,strlen(text));
    // FileClose(fw);


    // printf("Data: ");
    // for(int i=0;i<(int)readBytes;i++){
    //     printf("%c",buffer[i]);
    // }
    // printf("\n");
    // stac(3);
    // char a = 'a';
    // char a1 = 'b';
    // char a2 = 'c';
    // int num = 23;
    // log::out << (u64)&a  << " "<<(u64)&a2<< " "<<(u64)&num <<"\n";
    // for(int i=0;i<3;i++){
    //     log::out << *(&a + i)<<"\n";
    // }
    
    // return 0;

    log::out.enableReport(false);

    #define IfArg(X) if(!strcmp(arg,X))
    #define MODE_TEST 1
    #define MODE_RUN 2
    #define MODE_LOG 3
    int mode = MODE_RUN;
    
    bool devmode=false;
    
    std::string compilerPath = argv[0];

    std::vector<std::string> tests; // could be const char*
    std::vector<std::string> files;

    for(int i=1;i<argc;i++){
        const char* arg = argv[i];
        int len = strlen(argv[i]);
        // log::out << "arg["<<i<<"] "<<arg<<"\n";
        IfArg("--help") {
            print_help();
            return 0;
        } 
        // IfArg("-test") {
        //     mode = MODE_TEST;
        // } 
        // else IfArg("-testall") {
            // TestSuite(tests,true);
        // } else IfArg("-log") {
        //     mode = MODE_LOG;
        //     SetLog(LOG_OVERVIEW,true);
        // } 
        else IfArg("-dev") {
            devmode = true;
        } else if(mode==MODE_RUN){
            files.push_back(arg);
        } 
        // else if(mode==MODE_TEST){
        //     mode = MODE_RUN;
        //     tests.push_back(arg);
        // } else if(mode==MODE_LOG) {
            // mode = MODE_RUN;
            
            // int corrects[6]{0};
            // const char* strs[]{"tokenizer","preprocessor","parser","optimizer","interpreter","threads"};
            
            // for (int j=0;j<len;j++){
            //     char chr = arg[j];
            //     if(chr!=','){
            //         for (int k=0;k<6;k++){
            //             // log::out << "T "<<k<<" "<<chr<<"\n";
            //             if(strs[k][corrects[k]] == chr){
            //                 // log::out << "R "<<k<<" "<<chr<<"\n";
            //                 corrects[k]++;
            //                 if(corrects[k] == (int)strlen(strs[k])){
            //                     corrects[k] = 0;
            //                     SetLog(1<<k,true);
            //                 }
            //             }else{
            //                 corrects[k]=0;
            //             }
            //         }
            //     }
            //     if(chr == ',' || j+1 == len){
            //         int max = 0;
            //         int index = -1;
            //         for (int k=0;k<6;k++){
            //             if(corrects[k] >= max){
            //                 index = k;
            //                 max = corrects[k];
            //             }
            //             corrects[k] = 0;
            //         }
            //         // log::out << "on "<<index<<"\n";
            //         if(index!=-1){
            //             SetLog(1<<index,true);
            //         }
            //         continue;
            //     }
            // }
        // }
    }
    if(!tests.empty()){
        // TestSuite(tests);
    }
    Path compilerDir = compilerPath;
    compilerDir = compilerDir.getAbsolute().getDirectory();
    // ReplaceChar((char*)compilerPath.data(),compilerPath.length(),'\\','/');
    // std::string compilerDir = TrimLastFile(compilerPath);
    // log::out << "CompDir: " <<compilerDir.text <<"\n";
    if(compilerDir.text.length()>4){
        if(compilerDir.text.substr(compilerDir.text.length()-5,5) == "/bin/")
            compilerDir = compilerDir.text.substr(0,compilerDir.text.length() - 4);
    }
    // log::out << "CompDir: " <<compilerPath << " "<< compilerDir.text <<"\n";
    for(std::string& file : files){
        // Should source files be compiled seperatly like this
        CompileAndRun({file.c_str(), compilerDir.text});
    }
    if(!devmode && files.size()==0){
        print_help();
        // log::out << "No input files!\n";
    } else if(devmode){

        // PerfTestTokenize("example/build_fast.btb",200);

        CompileAndRun({"examples/strings.btb", compilerDir.text});
        // CompileAndRun("example/ast.btb", compilerPath);
        // CompileAndRun("tests/benchmark/loop.btb");
        // CompileAndRun("tests/benchmark/loop2.btb");
        
        // log::out.enableConsole(false);
        // TestVariableLimit(10000);
        // TestMathExpression(100);

        // auto tp = MeasureTime();
        // int sum = 0;
        // // int sum1 = 0;
        // // int sum2 = 0;
        // for (int i=0;i<1000000;i++) {
        //     sum = sum + i;
        //     // sum2 = sum1 + i*2;
        //     // sum2 = sum2 + i*3;
        // }
        // log::out << (StopMeasure(tp)*1e3)<<"\n";

        // CompileDisassemble("tests/varlimit.btb");

    }
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage();
    if(finalMemory!=0){
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";
        PrintRemainingTrackTypes();
    }
    log::out.cleanup();
}