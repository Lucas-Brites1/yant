#include "../include/yant_value.h"

Value* value_alloc(YantContext* ctx, Value value) {
    LOG_ASSERT(ctx && ctx->runtime, "invalid runtime context");
    Value* ptr = (Value*)blob_reserve(ctx->runtime, sizeof(Value));
    LOG_ASSERT(ptr, "out of runtime memory");
    *ptr = value;
    return ptr;
}

void value_print(void* v) {
    Value* val = (Value*)v;
    const char* valtype_str = value_type_str(val->type);
    switch (val->type) {
        case VALUE_STRING: printf("%s("SS_FMT")", valtype_str, SS_ARG(val->as_string));          break;
        case VALUE_FLOAT:  printf("%s(%f)",       valtype_str, val->as_float);                   break;
        case VALUE_INT:    printf("%s(%ld)",       valtype_str, val->as_int);                    break;
        case VALUE_BOOL:   printf("%s(%s)",       valtype_str, val->as_bool?"true":"false");     break;
        case VALUE_NIL:    printf("%s",           valtype_str);                                  break;
        default: TODO("Otto says: '%s' not implemented yet..", valtype_str);
    }
}
