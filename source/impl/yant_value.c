#include "../include/yant_value.h"

Value* value_alloc(YantContext* ctx, Value value) {
    LOG_ASSERT(ctx && ctx->runtime, "invalid runtime context");
    Value* ptr = (Value*)blob_reserve(ctx->runtime, sizeof(Value));
    LOG_ASSERT(ptr, "out of runtime memory");
    *ptr = value;
    return ptr;
}
