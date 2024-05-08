#include "BetBat/Config.h"

/* ###############
   Major config
   Make most edits here with global variables
############### */

LoggingSection global_loggingSection = (LoggingSection)(0
// | LOG_ALL
// | LOG_TASKS
// | LOG_BYTECODE

// | LOG_TOKENIZER    
// | LOG_PREPROCESSOR 
// | LOG_PARSER       
// | LOG_TYPECHECKER  
// | LOG_GENERATOR
// | LOG_OPTIMIZER     
// | LOG_CONVERTER     
// | LOG_INTERPRETER   
// | LOG_OVERVIEW       
// | LOG_MACRO_MATCH    

// | LOG_ALLOCATIONS
// | LOG_AST
// | LOG_IMPORTS    
// | LOG_INCLUDES   
);

// static int s_activelogs=0;
// void SetLog(int type, bool active){
//     if(active) s_activelogs |= type;
//     else s_activelogs &= ~type;
// }
// bool GetLog(int type){
//     return s_activelogs&type;
// }