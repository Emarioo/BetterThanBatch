#include "BetBat/Compiler.h"

void CompileInfo::cleanup(){
    for(auto& pair : tokenStreams){
        TokenStream::Destroy(pair.second.stream);
    }
}
bool CompileInfo::addStream(TokenStream* stream){
    auto pair = tokenStreams.find(stream->streamName);
    if(pair!=tokenStreams.end()) return false;
    auto ptr = &(tokenStreams[stream->streamName] = {});
    ptr->stream = stream;
    return true;
}
CompileInfo::FileInfo* CompileInfo::getStream(const std::string& name){
    auto pair = tokenStreams.find(name);
    if(pair == tokenStreams.end()) return 0;
    return &pair->second;
}
// src/util/base.btb -> src/util/
// base.btb -> /
// src -> /
std::string TrimLastFile(const std::string& path){
    int slashI = path.find_last_of("/");
    if(slashI==-1)
        return "/";
    return path.substr(0,slashI + 1);
}
std::string BriefPath(const std::string& path, int max=40){
    if((int)path.length()>max){
        return std::string("...") + path.substr(path.length()-max,max);
    }   
    return path;
}
// does not handle backslash
bool ParseFile(CompileInfo& info, const std::string& path){
    using namespace engone;
    
    _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefPath(path)<<"\n";)
    TokenStream* tokenStream = TokenStream::Tokenize(path);
    if(!tokenStream){
        log::out << log::RED << " Failed tokenization: " << BriefPath(path) <<"\n";
        return false;
    }
    info.lines += tokenStream->lines;
    info.readBytes += tokenStream->readBytes;
    
    
    
    info.addStream(tokenStream);
    std::string dir = TrimLastFile(path);
    for(const std::string& _importName : tokenStream->importList){
        std::string importName = "";
        int dotindex = importName.find_last_of(".");
        int slashindex = importName.find_last_of("/");
        if(dotindex==-1 || dotindex<slashindex){
            importName = _importName+".btb";
        } else {
            importName = _importName;
        }
        
        std::string fullPath = "";
        // TODO: Test "/src.btb", "ok./hum" and other unusual paths
        
        //-- Search standard modules
        std::string temp = info.compilerDir + "modules/"+importName;
        if(fullPath.empty() && FileExist(temp)) {
            fullPath = temp;
        }
        
        //-- Search directory of current source file
        if(fullPath.empty() && importName.find("./")==0){
            fullPath = dir + importName.substr(2);
        }
        
        //-- Search cwd
        if(fullPath.empty() && FileExist(importName)){
            fullPath = engone::GetWorkingDirectory() + "/" + importName;
        }
        
        // TODO: Search additional import directories
        
        if(fullPath.empty()){
            log::out << log::RED << "Could not find import '"<<importName<<"' in '"<<BriefPath(path,20)<<"'\n";
        }else{
            auto fileInfo = info.getStream(fullPath);
            if(fileInfo){   
                log::out << log::LIME << "Already imported "<<BriefPath(fullPath,20)<<"\n";
            }else{
                bool yes = ParseFile(info, fullPath);
            }
        }
    }
    
    if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        TokenStream* old = tokenStream;
        
        _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(path)<<"\n";)
        Preprocess(tokenStream, &info.errors);
        if (tokenStream->enabled == LAYER_PREPROCESSOR)
            tokenStream->print();
    }
    
    _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefPath(path)<<"\n";)
    ASTBody* body = ParseTokens(tokenStream,info.ast,&info.errors);
    if(!body)
        return false;
    bool yes = EvaluateTypes(info.ast,body,&info.errors);
    if(yes){
        info.ast->appendToMainBody(body);
    }
    return true;
}

Bytecode* CompileSource(const std::string& sourcePath, const std::string& compilerPath) {
    using namespace engone;
    AST* ast=0;
    Bytecode* bytecode=0;
    double seconds = 0;
    std::string dis;
    int bytes = 0;
    
    std::string cwd = engone::GetWorkingDirectory();
    
    std::string absPath = cwd +"/"+ sourcePath;
    
    // NOTE: Parser and generator uses tokens. Do not free tokens before compilation is complete.

    auto startCompileTime = engone::MeasureSeconds();
    
    CompileInfo compileInfo{};
    compileInfo.ast = AST::Create();
    compileInfo.compilerDir = TrimLastFile(compilerPath);
    EvaluateTypes(compileInfo.ast,compileInfo.ast->mainBody,&compileInfo.errors);
    ParseFile(compileInfo, absPath);
    ast = compileInfo.ast;
    
    _VLOG(log::out << "Final "; compileInfo.ast->print();)
    // if (tokens.enabled & LAYER_GENERATOR){
    if(compileInfo.errors==0 && ast){
        _VLOG(log::out <<log::BLUE<< "Generating code:\n";)
        bytecode = Generate(ast, &compileInfo.errors);
    }
    // }

    // _VLOG(log::out << "\n";)
    seconds = engone::StopMeasure(startCompileTime);
    if(bytecode && compileInfo.errors==0){
        bytes = bytecode->getMemoryUsage();
    // _VLOG(
        log::out << "Compiled " << FormatUnit((uint64)compileInfo.lines) << " lines ("<<FormatBytes(compileInfo.readBytes)<<") in " << FormatTime(seconds) << "\n (" << FormatUnit(compileInfo.lines / seconds) << " lines/s)\n";
    // )
    }
    AST::Destroy(ast);
    if(compileInfo.errors!=0){
        Bytecode::Destroy(bytecode);
        bytecode = 0;   
    }
    compileInfo.cleanup();
    return bytecode;
}

void CompileAndRun(const std::string& sourcePath, const std::string& compilerPath) {
    using namespace engone;
    Bytecode* Bytecode = CompileSource(sourcePath,compilerPath);
    if(Bytecode){
        Interpreter inp{};
        
        inp.execute(Bytecode);
        
        inp.cleanup();
        Bytecode::Destroy(Bytecode);
    }
}