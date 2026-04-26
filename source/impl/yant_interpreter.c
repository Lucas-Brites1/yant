#include "../include/yant_interpreter.h"
#include "../include/yant_value.h"

static inline bool at_end(Interpreter* i) {
    return i->nodes->len == i->current;
}

Value evaluate(Interpreter* interpreter);
Value dispatcher(Node* n);

void  evaluate_declaration(Interpreter* i, Node* declare);
void  evaluate_assignment(Interpreter* i,  Node* assign);
Value evaluate_operation(Node* n);

static inline Node* advance(Interpreter* i) {
    return vec_at(Node*, i->nodes, i->current++);
}

Value dispatcher(Node* n) {
    switch (n->type) {
        case NODE_LITERAL_STRING:  return StringValue(n->as.string_literal.value);
        case NODE_LITERAL_INTEGER: return IntegerValue(n->as.int_literal.value);
        case NODE_LITERAL_FLOAT:   return FloatValue(n->as.float_literal.value);
        case NODE_BINARY_OP:       return evaluate_operation(n);
        default:
            LOG_DEBUG("To be supported");
            return NilValue();
    }
}

// 2 * 3
// [TOKEN_LITERAL_INTEGER(2), TOKEN_STAR, TOKEN_LITERAL_INTEGER(3)]
// * 2 3
// BinaryOp::Star{left=2, right=3};
Value evaluate(Interpreter* interpreter) {
    Node* current = advance(interpreter);
    return dispatcher(current);
}

Interpreter interpreter_create(YantContext* yant_context, Vector* nodes) {
    return (Interpreter) {
        .yant_ctx = yant_context,
        .nodes    = nodes,
        .environ  = hmap_create(HMAP_INITIAL_CAP),
        .current  = 0
    };
}

void interpreter_free(Interpreter* i) {
    if (!i) return;
    hmap_free(&i->environ);
}

Vector interpret(Interpreter* interpreter) {
    LOG_ASSERT(interpreter && interpreter->yant_ctx && interpreter->nodes, "Failed trying to parse due to possible nil pointer");
    Vector values = vec_of(Value);

    while (!at_end(interpreter)) {
        Value v = evaluate(interpreter);
        vec_push(&values, (void*)&v);
    }

    return values;
}

Value evaluate_operation(Node* n) {
    Value left  = dispatcher(n->as.binary.left);
    Value right = dispatcher(n->as.binary.right);
    TokenType operator = n->as.binary.op;

    if (left.type == VALUE_INT && right.type == VALUE_FLOAT) {
        left = FloatValue((f64)left.as_int);
    } else if (left.type == VALUE_FLOAT && right.type == VALUE_INT) {
        right = FloatValue((f64)right.as_int);
    }

    if (left.type == VALUE_INT && right.type == VALUE_INT) {
        switch(operator) {
            case TOKEN_PLUS:  return IntegerValue(left.as_int + right.as_int);
            case TOKEN_MINUS: return IntegerValue(left.as_int - right.as_int);
            case TOKEN_STAR:  return IntegerValue(left.as_int * right.as_int);
            case TOKEN_SLASH: {
                if (right.as_int == 0) LOG_FATAL("division by zero");
                return IntegerValue(left.as_int / right.as_int);
            }
            default:
                LOG_FATAL("op not supported: %s", token_type_str(operator));
        }
    }

    if (left.type == VALUE_FLOAT && right.type == VALUE_FLOAT) {
        switch(operator) {
            case TOKEN_PLUS:  return FloatValue(left.as_float + right.as_float);
            case TOKEN_MINUS: return FloatValue(left.as_float - right.as_float);
            case TOKEN_STAR:  return FloatValue(left.as_float * right.as_float);
            case TOKEN_SLASH: {
                if (right.as_float == 0.0) LOG_FATAL("division by zero");
                return FloatValue(left.as_float / right.as_float);
            }
            default:
                LOG_FATAL("op not supported: %s", token_type_str(operator));
        }
    }

    LOG_FATAL(
        "type error: cannot apply %s to %s and %s",
        token_type_str(operator),
        value_type_str(left.type),
        value_type_str(right.type)
    );

    return NilValue();
}
