#include "../include/yant_interpreter.h"
#include "../include/yant_value.h"

static inline bool at_end(Interpreter* i) {
    return i->nodes->len == i->current;
}

Value evaluate(Interpreter* interpreter);
Value dispatcher(Interpreter* i, Node* n);

Value evaluate_declaration(Interpreter* i, Node* n);
Value evaluate_assignment(Interpreter* i,  Node* n);
Value evaluate_identifier(Interpreter* i,  Node* n);
Value evaluate_operation(Interpreter* i,   Node* n);

static inline Node* advance(Interpreter* i) {
    return vec_at(Node*, i->nodes, i->current++);
}

Value dispatcher(Interpreter* i, Node* n) {
    switch (n->type) {
        case NODE_LITERAL_STRING:    return StringValue(n->as.string_literal.value);
        case NODE_LITERAL_INTEGER:   return IntegerValue(n->as.int_literal.value);
        case NODE_LITERAL_FLOAT:     return FloatValue(n->as.float_literal.value);
        case NODE_LITERAL_BOOL:      return BooleanValue(n->as.boolean_literal.value);
        case NODE_BINARY_OP:         return evaluate_operation(i, n);
        case NODE_DECLARATION:       return evaluate_declaration(i, n);
        case NODE_ASSIGNMENT:        return evaluate_assignment(i, n);
        case NODE_IDENTIFIER:        return evaluate_identifier(i, n);
        default:
            LOG_DEBUG("WIP");
            return NilValue();
    }
}

// 2 * 3
// [TOKEN_LITERAL_INTEGER(2), TOKEN_STAR, TOKEN_LITERAL_INTEGER(3)]
// * 2 3
// BinaryOp::Star{left=2, right=3};
Value evaluate(Interpreter* interpreter) {
    Node* current = advance(interpreter);
    return dispatcher(interpreter, current);
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

void interpret(Interpreter* interpreter) {
    LOG_ASSERT(interpreter && interpreter->yant_ctx && interpreter->nodes, "Failed trying to parse due to possible nil pointer");

    while (!at_end(interpreter)) {
        Value v = evaluate(interpreter);

        switch(v.type) {
            case VALUE_FLOAT:
                LOG_DEBUG("%s(%f)", value_type_str(v.type), v.as_float);
                continue;
            case VALUE_INT:
                LOG_DEBUG("%s(%ld)", value_type_str(v.type), v.as_int);
                continue;
            case VALUE_STRING:
                LOG_DEBUG("%s(" SS_FMT")", value_type_str(v.type), SS_ARG(v.as_string));
                continue;
            case VALUE_NIL:
                LOG_DEBUG("%s", value_type_str(v.type));
                continue;
            default: LOG_DEBUG("%s not implemented yet", value_type_str(v.type));
        }
    }
}

Value evaluate_operation(Interpreter* i, Node* n) {
    Value left  = dispatcher(i, n->as.binary.left);
    Value right = dispatcher(i, n->as.binary.right);
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
            case TOKEN_LTE:   return BooleanValue(left.as_int <= right.as_int);
            case TOKEN_GTE:   return BooleanValue(left.as_int >= right.as_int);
            case TOKEN_LT:    return BooleanValue(left.as_int <  right.as_int);
            case TOKEN_GT:    return BooleanValue(left.as_int >  right.as_int);
            case TOKEN_EQEQ:  return BooleanValue(left.as_int == right.as_int);

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

Value evaluate_declaration(Interpreter* i, Node* n) {
    StringSlice identifier = n->as.declare.name;

    if(hmap_has(&i->environ, identifier)) {
        LOG_FATAL("variable already declared: " SS_FMT, SS_ARG(identifier));
    }

    Value* stored = value_alloc(i->yant_ctx, dispatcher(i, n->as.declare.value));
    hmap_insert(&i->environ, identifier, stored);

    return NilValue();
}

Value evaluate_assignment(Interpreter* i,  Node* n) {
    StringSlice identifier = n->as.assign.name;

    if (!hmap_has(&i->environ, identifier)) {
        LOG_FATAL("undefined variable in assignment: " SS_FMT, SS_ARG(identifier));
    }

    Value  new_value  = dispatcher(i, n->as.assign.value);
    Value* old_stored = hmap_get_as(Value, &i->environ, identifier);

    if (old_stored->type != new_value.type) {
        LOG_FATAL(
            "type mismatch in assignment: %s vs %s",
            value_type_str(old_stored->type),
            value_type_str(new_value.type)
        );
    }

    Value* new_stored = value_alloc(i->yant_ctx, new_value);
    hmap_update(&i->environ, identifier, new_stored);

    return NilValue();
}

Value evaluate_identifier(Interpreter* i, Node* n) {
    StringSlice identifier = n->as.identifier.name;

    if (!hmap_has(&i->environ, identifier)) {
        LOG_FATAL(
            "undefined variable '" SS_FMT "': not declared in current scope",
            SS_ARG(identifier)
        );
    }

    return *hmap_get_as(Value, &i->environ, identifier);
}
