#pragma once

#include "BetBat/Bytecode.h"

bool WriteDeclFiles(const std::string& exe_path, Bytecode* bytecode, AST* ast, bool is_dynamic_library);