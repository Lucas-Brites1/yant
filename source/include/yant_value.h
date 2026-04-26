#pragma once
#include "logc.h"
#include "blobberman.h"
#include "yant_context.h"
#include "yant_strings.h"
#include "yant_types.h"

#define VALUES_TABLE(extends) \
    extends(VALUE_INT,   "Value::Integer") \
    extends(VALUE_FLOAT, "Value::Float")   \
    extends(VALUE_STRING,"Value::String")  \
    extends(VALUE_NIL,   "Value::Nil")

#define EXTENDS_VALUE_ENUM(T, str) T,
typedef enum {
    VALUES_TABLE(EXTENDS_VALUE_ENUM)
    VALUES_COUNT
} ValueType;
#undef EXTENDS_VALUE_ENUM

#define EXTENDS_VALUE_AS_STRING(T, str) case T: return str;
static inline const char* value_type_str(ValueType t) {
    switch (t) {
        VALUES_TABLE(EXTENDS_VALUE_AS_STRING)
        default:
            LOG_FATAL("Unknown value type");
            return "?";   // unreachable
    }
}
#undef EXTENDS_VALUE_AS_STRING

typedef struct {
    ValueType type;
    union {
      i64 as_int;
      f64 as_float;
      StringSlice as_string;
    };
} Value;


Value* value_alloc(YantContext* ctx, Value value);


static inline Value  StringValue(StringSlice value) {
    return (Value) {
        .type = VALUE_STRING,
        .as_string = value
    };
}

static inline Value  IntegerValue(i64 value) {
    return (Value) {
        .type = VALUE_INT,
        .as_int = value
    };
}

static inline Value  FloatValue(f64 value) {
    return (Value) {
        .type = VALUE_FLOAT,
        .as_float = value
    };
}

static inline Value  NilValue(void) {
    return (Value) {
        .type = VALUE_NIL,
    };
}
