#include "BetBat/Compiler.h"

void CompileInfo::cleanup(){
    for(auto& pair : tokenStreams){
        TokenStream::Destroy(pair.second.stream);
    }
    for(auto& pair : includeStreams){
        TokenStream::Destroy(pair.second);
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
    if(pair == tokenStreams.end())
        return nullptr;
    return &pair->second;
}
// does not handle backslash
ASTScope* ParseFile(CompileInfo& info, const std::string& path, std::string as = ""){
    using namespace engone;
    
    _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefPath(path)<<"\n";)
    TokenStream* tokenStream = TokenStream::Tokenize(path);
    if(!tokenStream){
        log::out << log::RED << " Failed tokenization: " << BriefPath(path) <<"\n";
        return nullptr;
    }
    info.lines += tokenStream->lines;
    info.readBytes += tokenStream->readBytes;
    
    info.addStream(tokenStream);
    
    if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        TokenStream* old = tokenStream;
        
        _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(path)<<"\n";)
        int errs = 0;
        Preprocess(&info, tokenStream, &errs);
        if (tokenStream->enabled == LAYER_PREPROCESSOR)
            tokenStream->print();
        info.errors += errs;
        if(errs){
            return nullptr;
        }
    }
    
    std::string dir = TrimLastFile(path);
    for(auto& item : tokenStream->importList){
        std::string importName = "";
        int dotindex = item.name.find_last_of(".");
        int slashindex = item.name.find_last_of("/");
        // log::out << "dot "<<dotindex << " slash "<<slashindex<<"\n";
        if(dotindex==-1 || dotindex<slashindex){
            importName = item.name+".btb";
        } else {
            importName = item.name;
        }
        // TODO: import AS
        
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
        
        //-- Search cwd or absolute path
        if(fullPath.empty() && FileExist(importName)){
            fullPath = engone::GetWorkingDirectory() + "/" + importName;
            ReplaceChar((char*)fullPath.data(),fullPath.length(),'\\','/');
        }
        
        // TODO: Search additional import directories, DO THIS WITH #INCLUDE TOO!
        
        if(fullPath.empty()){
            log::out << log::RED << "Could not find import '"<<importName<<"' (import from '"<<BriefPath(path,20)<<"'\n";
        }else{
            auto fileInfo = info.getStream(fullPath);
            if(fileInfo){   
                log::out << log::LIME << "Already imported "<<BriefPath(fullPath,20)<<"\n";
            }else{
                ASTScope* body = ParseFile(info, fullPath, item.as);
                if(body){
                    if(item.as.empty()){
                        info.ast->appendToMainBody(body);   
                    } else {
                        // body->convertToNamespace(item.as);
                        info.ast->mainBody->add(body, info.ast);
                    }
                }
            }
        }
    }
    
    _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefPath(path)<<"\n";)
    ASTScope* body = ParseTokens(tokenStream,info.ast,&info.errors, as);
    // info.ast->destroy(body);
    // body = 0;
    return body;
    // if(!body)
    //     return false;
        
    // info.errors += TypeCheck(info.ast);
    // bool yes = EvaluateTypes(info.ast,body,&info.errors);
    // info.ast->appendToMainBody(body);
    // return true;
}

Bytecode* CompileSource(const std::string& sourcePath, const std::string& compilerPath) {
    using namespace engone;
    AST* ast=0;
    Bytecode* bytecode=0;
    double seconds = 0;
    
    std::string cwd = engone::GetWorkingDirectory();
    
    std::string absPath = cwd +"/"+ sourcePath;
    ReplaceChar((char*)absPath.data(),absPath.length(),'\\','/');
    
    // NOTE: Parser and generator uses tokens. Do not free tokens before compilation is complete.

    auto startCompileTime = engone::MeasureSeconds();
    
    CompileInfo compileInfo{};
    compileInfo.ast = AST::Create();
    compileInfo.compilerDir = TrimLastFile(compilerPath);
    // compileInfo.errors += TypeCheck(compileInfo.ast, compileInfo.ast->mainBody);
    // EvaluateTypes(compileInfo.ast,compileInfo.ast->mainBody,&compileInfo.errors);
    
    ASTScope* body = ParseFile(compileInfo, absPath);
    compileInfo.ast->appendToMainBody(body);
    ast = compileInfo.ast;
    
    compileInfo.errors += TypeCheck(ast, ast->mainBody);
    
    // if(compileInfo.errors==0){
        _VLOG(log::out << log::BLUE<< "Final "; compileInfo.ast->print();)
    // }
    // if (tokens.enabled & LAYER_GENERATOR){

    if(ast
        // && compileInfo.errors==0 /* commented out so we keep going and catch more errors */
        ){
        _VLOG(log::out <<log::BLUE<< "Generating code:\n";)
        bytecode = Generate(ast, &compileInfo.errors);
    // }
    }

    // _VLOG(log::out << "\n";)
    seconds = engone::StopMeasure(startCompileTime);
    if(bytecode && compileInfo.errors==0){
        int bytes = bytecode->getMemoryUsage();
    // _VLOG(
        log::out << "Compiled " << FormatUnit((uint64)compileInfo.lines) << " lines ("<<FormatBytes(compileInfo.readBytes)<<") in " << FormatTime(seconds) << "\n (" << FormatUnit(compileInfo.lines / seconds) << " lines/s)\n";
    // )
    }
    AST::Destroy(ast);
    if(compileInfo.errors!=0){
        log::out << log::RED<<"Compilation failed with "<<compileInfo.errors<<" error(s)\n";
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    }
    compileInfo.cleanup();
    return bytecode;
}

void CompileAndRun(const std::string& sourcePath, const std::string& compilerPath) {
    using namespace engone;
    Bytecode* bytecode = CompileSource(sourcePath,compilerPath);
    if(bytecode){
        Interpreter interpreter{};
        interpreter.execute(bytecode);
        interpreter.cleanup();
        Bytecode::Destroy(bytecode);
    }
}