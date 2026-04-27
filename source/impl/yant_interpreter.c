#include "../include/yant_interpreter.h"
#include "../include/yant_value.h"
#include "../include/yant_parser.h"

static inline bool at_end(Interpreter* i) {
    return i->nodes->len == i->current;
}

Value evaluate(Interpreter* interpreter);
Value dispatcher(Interpreter* i, Node* n);

Value find_identifier     (Interpreter* i,  Node* n);
Value evaluate_declaration(Interpreter* i, Node* n);
Value evaluate_assignment (Interpreter* i,  Node* n);
Value evaluate_operation  (Interpreter* i,   Node* n);

static inline Node* advance(Interpreter* i) {
    return vec_at(Node*, i->nodes, i->current++);
}

Value dispatcher(Interpreter* i, Node* n) {
    switch (n->type) {
        case NODE_LITERAL_STRING:    return StringValue(n->as.string_literal.value);
        case NODE_LITERAL_INTEGER:   return IntegerValue(n->as.int_literal.value);
        case NODE_LITERAL_FLOAT:     return FloatValue(n->as.float_literal.value);
        case NODE_LITERAL_BOOL:      return BooleanValue(n->as.boolean_literal.value);
        case NODE_LITERAL_NIL:       return NilValue();
        case NODE_BINARY_OP:         return evaluate_operation(i, n);
        case NODE_DECLARATION:       return evaluate_declaration(i, n);
        case NODE_ASSIGNMENT:        return evaluate_assignment(i, n);
        case NODE_IDENTIFIER:        return find_identifier(i, n);
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
            case VALUE_BOOL:
                LOG_DEBUG("%s", value_type_str(v.type));
                continue;
            default: TODO("%s not implemented yet", value_type_str(v.type));
        }
    }
}

static Value evaluate_logical(Interpreter* i, Node* n) {
    TokenType op = n->as.binary.op;
    Value left = dispatcher(i, n->as.binary.left);

    if (left.type != VALUE_BOOL) {
            INVALID(
                "left operand of '%s' must be boolean, got %s",
                token_type_str(op), value_type_str(left.type)
            );
        }

    if (op == TOKEN_AND && !left.as_bool) return BooleanValue(false);
    if (op == TOKEN_OR  &&  left.as_bool) return BooleanValue(true);

    Value right = dispatcher(i, n->as.binary.right);
    if (right.type != VALUE_BOOL) {
        INVALID(
            "right operand of '%s' must be boolean, got %s",
            token_type_str(op), value_type_str(right.type)
        );
    }

    return BooleanValue(right.as_bool);
}

static Value evaluate_comparision(TokenType op, Value vA, Value vB) {
    if (vA.type == VALUE_INT && vB.type == VALUE_FLOAT) vA = FloatValue((f64)vA.as_int);
    if (vA.type == VALUE_FLOAT && vB.type == VALUE_INT) vB = FloatValue((f64)vB.as_int);

    if (vA.type != vB.type) {
        if (op == TOKEN_EQEQ)  return BooleanValue(false);
        if (op == TOKEN_NOTEQ) return BooleanValue(true);
        INVALID("cannot compare %s and %s with %s",value_type_str(vA.type), value_type_str(vB.type), token_type_str(op));
    }

    switch (vA.type) {
        case VALUE_INT: {
            switch (op) {
                case TOKEN_LTE:   return BooleanValue(vA.as_int <= vB.as_int);
                case TOKEN_GTE:   return BooleanValue(vA.as_int >= vB.as_int);
                case TOKEN_LT:    return BooleanValue(vA.as_int < vB.as_int);
                case TOKEN_GT:    return BooleanValue(vA.as_int > vB.as_int);
                case TOKEN_EQEQ:  return BooleanValue(vA.as_int == vB.as_int);
                case TOKEN_NOTEQ: return BooleanValue(vA.as_int != vB.as_int);
                default: UNREACHABLE();
            }
            break;
        }
        case VALUE_FLOAT: {
            switch (op) {
                case TOKEN_LTE:   return BooleanValue(vA.as_float <= vB.as_float);
                case TOKEN_GTE:   return BooleanValue(vA.as_float >= vB.as_float);
                case TOKEN_LT:    return BooleanValue(vA.as_float < vB.as_float);
                case TOKEN_GT:    return BooleanValue(vA.as_float > vB.as_float);
                case TOKEN_EQEQ:  return BooleanValue(vA.as_float == vB.as_float);
                case TOKEN_NOTEQ: return BooleanValue(vA.as_float != vB.as_float);
                default: UNREACHABLE();
            }
            break;
        }
        case VALUE_BOOL: {
            switch (op) {
                case TOKEN_EQEQ:  return BooleanValue(vA.as_bool == vB.as_bool);
                case TOKEN_NOTEQ: return BooleanValue(vA.as_bool != vB.as_bool);
                default: UNSUPPORTED("%s unsupported type of operation between boolean values", token_type_str(op));
            }
            break;
        }
        case VALUE_STRING: {
            switch (op) {
                case TOKEN_LTE:   return BooleanValue( ss_cmp(vA.as_string, vB.as_string) <= 0);
                case TOKEN_GTE:   return BooleanValue( ss_cmp(vA.as_string, vB.as_string) >= 0);
                case TOKEN_LT:    return BooleanValue( ss_cmp(vA.as_string, vB.as_string) < 0);
                case TOKEN_GT:    return BooleanValue( ss_cmp(vA.as_string, vB.as_string) > 0);
                case TOKEN_EQEQ:  return BooleanValue( ss_eq(vA.as_string, vB.as_string));
                case TOKEN_NOTEQ: return BooleanValue(!ss_eq(vA.as_string, vB.as_string));
                default: UNREACHABLE();
            }
            break;
        }
        case VALUE_NIL: {
            switch (op) {
                case TOKEN_EQEQ:  return BooleanValue(true);
                case TOKEN_NOTEQ: return BooleanValue(false);
                default: UNSUPPORTED("ordered comparison '%s' on nil", token_type_str(op));
            }
            break;
        }
    }
    UNREACHABLE();
}

Value evaluate_operation(Interpreter* i, Node* n) {
    TokenType operator = n->as.binary.op;
    if (TYPE_IN(operator, TOKEN_OR, TOKEN_AND))
        return evaluate_logical(i, n);

    Value left  = dispatcher(i, n->as.binary.left);
    Value right = dispatcher(i, n->as.binary.right);

    if (TYPE_IN(operator, TOKEN_EQEQ, TOKEN_NOTEQ, TOKEN_GT, TOKEN_GTE, TOKEN_LT, TOKEN_LTE))
        return evaluate_comparision(operator, left, right);

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

Value find_identifier(Interpreter* i, Node* n) {
    StringSlice identifier = n->as.identifier.name;

    if (!hmap_has(&i->environ, identifier)) {
        LOG_FATAL(
            "undefined variable '" SS_FMT "': not declared in current scope",
            SS_ARG(identifier)
        );
    }

    return *hmap_get_as(Value, &i->environ, identifier);
}
