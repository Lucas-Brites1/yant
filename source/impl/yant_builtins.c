#include "../include/yant_builtins.h"
#include "../include/yant_value.h"
#include "../include/yant_vector.h"
#include "../include/yant_hashmap.h"
#include "../include/yant_strings.h"
#include "../include/logc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#define STD_BUILTIN_TABLE(X) \
    X(print)                 \
    X(input)                 \
    X(random_integer)        \
    X(random_float)          \
    X(to_integer)            \
    X(to_float)              \
    X(to_string)             \
    X(sqrt)

#define DECLARE_BUILTIN(name) \
    static Value builtin_##name(Interpreter* i, Vector* args);

STD_BUILTIN_TABLE(DECLARE_BUILTIN)
#undef DECLARE_BUILTIN


// ============================================================================
// IO
// ============================================================================

static Value builtin_print(Interpreter* i, Vector* args) {
    (void)i;
    if (args->len != 1) {
        LOG_FATAL("print expects 1 argument, got %zu", args->len);
    }

    Value v = *vec_ref(Value, args, 0);
    switch (v.type) {
        case VALUE_INT:    printf("%ld\n", v.as_int); break;
        case VALUE_FLOAT:  printf("%g\n", v.as_float); break;
        case VALUE_STRING: printf(SS_FMT "\n", SS_ARG(v.as_string)); break;
        case VALUE_BOOL:   printf("%s\n", v.as_bool ? "true" : "false"); break;
        case VALUE_NIL:    printf("nil\n"); break;
        default: LOG_FATAL("cannot print %s", value_type_str(v.type));
    }
    return NilValue();
}

static Value builtin_input(Interpreter* i, Vector* args) {
    if (args->len > 0) {
        Value prompt = *vec_ref(Value, args, 0);
        if (prompt.type == VALUE_STRING) {
            fwrite(prompt.as_string.data, 1, prompt.as_string.length, stdout);
            fflush(stdout);
        }
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NilValue();
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') buffer[--len] = '\0';

    char* dup = blob_reserve(i->yant_ctx->runtime, len + 1);
    memcpy(dup, buffer, len);
    dup[len] = '\0';

    return StringValue((StringSlice){.data = dup, .length = len});
}


// ============================================================================
// CONVERSÕES
// ============================================================================

static Value builtin_to_integer(Interpreter* i, Vector* args) {
    (void)i;
    if (args->len != 1) {
        LOG_FATAL("to_integer expects 1 argument, got %zu", args->len);
    }

    Value val = *vec_ref(Value, args, 0);
    if (val.type != VALUE_STRING) {
        LOG_FATAL("to_integer expects string, got '%s'", value_type_str(val.type));
    }

    // StringSlice não é null-terminated — copia pra buffer com '\0'
    char buf[64];
    if (val.as_string.length >= sizeof(buf)) {
        LOG_FATAL("to_integer: string too long");
    }
    memcpy(buf, val.as_string.data, val.as_string.length);
    buf[val.as_string.length] = '\0';

    char* end;
    errno = 0;
    long n = strtol(buf, &end, 10);
    if (errno != 0 || *end != '\0' || end == buf) {
        LOG_FATAL("to_integer: cannot convert '%s'", buf);
    }

    return IntegerValue((i64)n);
}

static Value builtin_to_float(Interpreter* i, Vector* args) {
    (void)i;
    if (args->len != 1) {
        LOG_FATAL("to_float expects 1 argument, got %zu", args->len);
    }

    Value val = *vec_ref(Value, args, 0);
    if (val.type != VALUE_STRING) {
        LOG_FATAL("to_float expects string, got '%s'", value_type_str(val.type));
    }

    char buf[64];
    if (val.as_string.length >= sizeof(buf)) {
        LOG_FATAL("to_float: string too long");
    }
    memcpy(buf, val.as_string.data, val.as_string.length);
    buf[val.as_string.length] = '\0';

    char* end;
    errno = 0;
    double n = strtod(buf, &end);
    if (errno != 0 || *end != '\0' || end == buf) {
        LOG_FATAL("to_float: cannot convert '%s'", buf);
    }

    return FloatValue((f64)n);
}

static Value builtin_to_string(Interpreter* i, Vector* args) {
    if (args->len != 1) {
        LOG_FATAL("to_string expects 1 argument, got %zu", args->len);
    }

    Value v = *vec_ref(Value, args, 0);
    char buffer[64];
    int len = 0;

    switch (v.type) {
        case VALUE_INT:
            len = snprintf(buffer, sizeof(buffer), "%ld", v.as_int);
            break;
        case VALUE_FLOAT:
            len = snprintf(buffer, sizeof(buffer), "%g", v.as_float);
            break;
        case VALUE_BOOL:
            len = snprintf(buffer, sizeof(buffer), "%s", v.as_bool ? "true" : "false");
            break;
        case VALUE_NIL:
            len = snprintf(buffer, sizeof(buffer), "nil");
            break;
        case VALUE_STRING:
            return v;
        default:
            LOG_FATAL("to_string: cannot convert %s", value_type_str(v.type));
    }

    if (len < 0 || len >= (int)sizeof(buffer)) {
        LOG_FATAL("to_string: buffer overflow");
    }

    char* dup = blob_reserve(i->yant_ctx->runtime, len + 1);
    memcpy(dup, buffer, len);
    dup[len] = '\0';

    return StringValue((StringSlice){
        .data = dup,
        .length = (usize)len
    });
}


// ============================================================================
// MATH
// ============================================================================

static Value builtin_random_integer(Interpreter* i, Vector* args) {
    (void)i;
    if (args->len != 2) {
        LOG_FATAL("random_integer expects 2 arguments (min, max), got %zu", args->len);
    }

    Value min = *vec_ref(Value, args, 0);
    Value max = *vec_ref(Value, args, 1);
    if (min.type != VALUE_INT || max.type != VALUE_INT) {
        LOG_FATAL("random_integer: min and max must be integer");
    }
    if (min.as_int > max.as_int) {
        LOG_FATAL("random_integer: min (%ld) must be <= max (%ld)",
                  min.as_int, max.as_int);
    }

    i64 lo = min.as_int;
    i64 hi = max.as_int;
    i64 val = lo + (i64)(rand() % (hi - lo + 1));

    return IntegerValue(val);
}

static Value builtin_random_float(Interpreter* i, Vector* args) {
    (void)i;
    if (args->len != 2) {
        LOG_FATAL("random_integer expects 2 arguments (min, max), got %zu", args->len);
    }

    Value min = *vec_ref(Value, args, 0);
    Value max = *vec_ref(Value, args, 1);

    if (min.type == VALUE_INT) {
        min = FloatValue((f64)min.as_int);
    }
    if (max.type == VALUE_INT) {
        max = FloatValue((f64)max.as_int);
    }

    if (min.type != VALUE_FLOAT || max.type != VALUE_FLOAT) {
        LOG_FATAL("random_float: min and max must be float");
    }

    if (min.as_float > max.as_float) {
        LOG_FATAL("random_float: min (%g) must be <= max (%g)",
                  min.as_float, max.as_float);
    }

    f64 r = (f64)rand() / RAND_MAX;
    f64 l = min.as_float;
    f64 h = max.as_float;
    f64 val = l + r * (h - l);

    return FloatValue(val);
}

static Value builtin_sqrt(Interpreter* i, Vector* args) {
    (void)i;
    if (args->len != 1) {
        LOG_FATAL("sqrt expects 1 argument, got %zu", args->len);
    }

    Value v = *vec_ref(Value, args, 0);
    f64 x;
    switch (v.type) {
        case VALUE_INT:   x = (f64)v.as_int;   break;
        case VALUE_FLOAT: x = v.as_float;       break;
        default:
            LOG_FATAL("sqrt: invalid type '%s'", value_type_str(v.type));
    }

    if (x < 0.0) {
        LOG_FATAL("sqrt: negative argument %g", x);
    }

    return FloatValue(sqrt(x));
}

#define REGISTER_BUILTIN(name) {                       \
    Value v = BuiltinValue(builtin_##name);            \
    Value* stored = value_alloc(i->yant_ctx, v);       \
    hmap_insert(global, SS(#name), stored);            \
}

void register_builtins(Interpreter* i, Map* global) {
    STD_BUILTIN_TABLE(REGISTER_BUILTIN)
}

#undef REGISTER_BUILTIN
