#include "Value.h"
class Context;
class String;

Ref ExtPrint(Context* context, int refType, void* value);
Ref ExtTime(Context* context, int refType, void* value);
Ref ExtToNum(Context* context, int refType, void* value);