#include "../include/yant_interpreter.h"
#include "../include/yant_value.h"
#include "../include/yant_parser.h"
#include "../include/yant_builtins.h"
#include <stdbool.h>

static inline Map* current_scope(Interpreter* i) {
    return &vec_at(Map, &i->scopes, i->scopes.len - 1);
}

static inline void enter_scope(Interpreter* i) {
    Map scope = hmap_create(HMAP_INITIAL_CAP);
    vec_push(&i->scopes, &scope);
}

static inline void exit_scope(Interpreter* i) {
    LOG_ASSERT(i->scopes.len > 1, "cannot exit global scope");
    Map* top = current_scope(i);
    hmap_free(top);
    i->scopes.len--;
}

static inline bool at_end(Interpreter* i) {
    return i->nodes->len == i->current;
}

static Map* find_scope_with(Interpreter* i, StringSlice name) {
    for (isize idx = (isize)i->scopes.len - 1; idx >= 0; idx--) {
        Map* scope = &vec_at(Map, &i->scopes, idx);
        if (hmap_has(scope, name)) return scope;
    }

    return nil;
}

static ValueType expected_value_type(TokenType kind) {
    switch (kind) {
        case TOKEN_TYPE_INT:     return VALUE_INT;
        case TOKEN_TYPE_FLOAT:   return VALUE_FLOAT;
        case TOKEN_TYPE_STRING:  return VALUE_STRING;
        case TOKEN_TYPE_BOOLEAN: return VALUE_BOOL;
        case TOKEN_NIL:          return VALUE_NIL;
        default:
            LOG_FATAL("invalid kind in declaration: %s", token_type_str(kind));
            return VALUE_NIL;
    }
}

Value evaluate(Interpreter* interpreter);
Value dispatcher(Interpreter* i, Node* n);

Value find_identifier     (Interpreter* i,  Node* n);
Value evaluate_declaration(Interpreter* i,  Node* n);
Value evaluate_assignment (Interpreter* i,  Node* n);
Value evaluate_operation  (Interpreter* i,  Node* n);
Value evaluate_unary      (Interpreter* i,  Node* n);
Value evaluate_conditional(Interpreter* i,  Node* n);
Value evaluate_loop       (Interpreter* i,  Node* n);
Value evaluate_block      (Interpreter* i,  Node* n);
Value evaluate_function_declaration(Interpreter* i, Node* n);
Value evaluate_function_call(Interpreter* i,  Node* n);
Value evaluate_match(Interpreter* i, Node* n);

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
        case NODE_UNARY_OP:          return evaluate_unary(i, n);
        case NODE_IDENTIFIER:        return find_identifier(i, n);
        case NODE_DECLARATION:       return evaluate_declaration(i, n);
        case NODE_ASSIGNMENT:        return evaluate_assignment(i, n);
        case NODE_IF:                return evaluate_conditional(i, n);
        case NODE_LOOP:              return evaluate_loop(i, n);
        case NODE_BLOCK:             return evaluate_block(i, n);
        case NODE_FNDECLARE:         return evaluate_function_declaration(i, n);
        case NODE_CALL:              return evaluate_function_call(i, n);
        case NODE_MATCH:             return evaluate_match(i, n);
        default:
            LOG_DEBUG("WIP");
            return NilValue();
    }
}

Value evaluate(Interpreter* interpreter) {
    Node* current = advance(interpreter);
    return dispatcher(interpreter, current);
}

Interpreter interpreter_create(YantContext* yant_context, Vector* nodes) {
    Interpreter i = {
        .yant_ctx = yant_context,
        .nodes    = nodes,
        .scopes   = vec_of(Map),
        .current  = 0
    };

    Map global = hmap_create(HMAP_INITIAL_CAP);
    vec_push(&i.scopes, &global);

    register_builtins(&i, &global);
    return i;
}

void interpreter_free(Interpreter* i) {
    if (!i) return;
    vec_foreach(Map, scope, &i->scopes) {
        hmap_free(scope);
    }

    vec_free(&i->scopes);
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
                LOG_DEBUG("%s(%s)", value_type_str(v.type), v.as_bool ? "true" : "false");
                continue;
            case VALUE_FN:
                LOG_DEBUG("%s:" SS_FMT "(...):%s {}", value_type_str(v.type), SS_ARG(v.as_fn->name), token_type_str(v.as_fn->return_type));
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
        default: UNREACHABLE();
    }
    UNREACHABLE();
}

Value evaluate_unary      (Interpreter* i,  Node* n) {
    TokenType op = n->as.unary.unary_op;
    Value operand = dispatcher(i, n->as.unary.operand);

    if (op == TOKEN_MINUS) {
        switch (operand.type) {
            case VALUE_INT:   return IntegerValue(-operand.as_int);
            case VALUE_FLOAT: return FloatValue(-operand.as_float);
            LOG_FATAL("unary '-' requires numeric, got %s", value_type_str(operand.type));
        }
    }

    if (op == TOKEN_NOT) {
        if (operand.type == VALUE_BOOL) return BooleanValue(!operand.as_bool);
        LOG_FATAL("unary '!' requires boolean, got %s", value_type_str(operand.type));
    }

    LOG_FATAL("unknown unary operator: %s", token_type_str(op));
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

Value find_identifier(Interpreter* i, Node* n) {
    StringSlice identifier = n->as.identifier.name;

    Map* scope = find_scope_with(i, identifier);
    if (!scope) LOG_FATAL("undefined variable '" SS_FMT "'", SS_ARG(identifier));
    return *hmap_get_as(Value, scope, identifier);
}

Value evaluate_declaration(Interpreter* i, Node* n) {
    StringSlice identifier = n->as.declare.name;

    if(hmap_has(current_scope(i), identifier)) {
        LOG_FATAL("variable already declared: " SS_FMT, SS_ARG(identifier));
    }

    Value     computed = dispatcher(i, n->as.declare.value);
    ValueType expected = expected_value_type(n->as.declare.kind);

    if (computed.type != expected) {
        LOG_FATAL(
            "type mismatch in declaration of '" SS_FMT "': expected %s, got %s",
            SS_ARG(identifier),
            value_type_str(expected),
            value_type_str(computed.type)
        );
    }

    Value* stored = value_alloc(i->yant_ctx, computed);
    hmap_insert(current_scope(i), identifier, stored);

    return computed;
}

Value evaluate_assignment(Interpreter* i,  Node* n) {
    StringSlice identifier = n->as.assign.name;
    Value       new_value  = dispatcher(i, n->as.assign.value);

    Map* scope = find_scope_with(i, identifier);
    if (!scope) LOG_FATAL("undefined variable in assignment: '" SS_FMT "'", SS_ARG(identifier));

    Value* old_stored = hmap_get_as(Value, scope, identifier);

    if (old_stored->type != new_value.type) {
        LOG_FATAL(
            "type mismatch in assignment: %s vs %s",
            value_type_str(old_stored->type),
            value_type_str(new_value.type)
        );
    }

    Value* new_stored = value_alloc(i->yant_ctx, new_value);
    hmap_update(scope, identifier, new_stored);
    return new_value;
}

Value evaluate_conditional(Interpreter* i, Node* n) {
    // (conditional(boolean), then_branch<expression>, else_branch<expression>)
    Value conditional_branch = dispatcher(i, n->as.if_expression.base_cond);

    if (conditional_branch.type != VALUE_BOOL) LOG_FATAL("if condition must be boolean, got %s", value_type_str(conditional_branch.type));
    if (conditional_branch.as_bool) return dispatcher(i, n->as.if_expression.then_cond);
    return dispatcher(i, n->as.if_expression.else_cond);
}

Value evaluate_block(Interpreter* i, Node* n) {
    enter_scope(i);
    Vector statements = n->as.block.statements;

    Value last = NilValue();
    vec_foreach(Node*, node, &statements) {
        last = dispatcher(i, *node);
    }

    exit_scope(i);
    return last;
}

Value evaluate_loop       (Interpreter* i,  Node* n) {
    Loop* loop = n->as.loop;

    enter_scope(i);
    if (loop->init) dispatcher(i, n->as.loop->init);
    while (true) {
        Value cond = dispatcher(i, loop->condition);
        if (cond.type != VALUE_BOOL) {
            LOG_FATAL("loop condition must be boolean, got %s", value_type_str(cond.type));
        }
        if (!cond.as_bool) break;

        evaluate_block(i, loop->body);
        if (loop->step) dispatcher(i, loop->step);
    }
    exit_scope(i);

    return NilValue();
}

Value evaluate_function_declaration(Interpreter* i, Node* n) {
    Function* fn = n->as.function;
    Map*   scope = current_scope(i);

    if (hmap_has(scope, fn->name)) {
        LOG_FATAL("function '" SS_FMT "' already declared in current scope", SS_ARG(fn->name));
    }

    Value fn_value = FnValue(fn);
    Value* stored  = value_alloc(i->yant_ctx, fn_value);
    hmap_insert(scope, fn->name, stored);

    return fn_value;
}

Value evaluate_function_call(Interpreter* i, Node* n) {
    Value fn_val = dispatcher(i, n->as.call.callee);

    Vector args = n->as.call.arguments;
    Vector arg_values = vec_of(Value);
    vec_foreach(Node*, arg_node, &args) {
        Value arg_val = dispatcher(i, *arg_node);
        vec_push(&arg_values, &arg_val);
    }

    if (fn_val.type == VALUE_BUILTIN) {
        Value result = fn_val.as_builtin(i, &arg_values);
        vec_free(&arg_values);
        return result;
    }

    if (fn_val.type == VALUE_FN) {
        Function* fn = fn_val.as_fn;
        Vector params = fn->params;

        if (params.len != arg_values.len) {
            LOG_FATAL("'" SS_FMT "' expects %zu args, got %zu",
                      SS_ARG(fn->name), params.len, arg_values.len);
        }

        for (usize idx = 0; idx < params.len; idx++) {
            Param* p = vec_ref(Param, &params, idx);
            Value* v = vec_ref(Value, &arg_values, idx);
            ValueType expected = expected_value_type(p->param_type);
            if (v->type != expected) {
                LOG_FATAL("arg %zu of '" SS_FMT "': expected %s, got %s",
                          idx, SS_ARG(fn->name),
                          value_type_str(expected), value_type_str(v->type));
            }
        }

        enter_scope(i);
        for (usize idx = 0; idx < params.len; idx++) {
            Param* p = vec_ref(Param, &params, idx);
            Value* v = vec_ref(Value, &arg_values, idx);
            Value* stored = value_alloc(i->yant_ctx, *v);
            hmap_insert(current_scope(i), p->param_name, stored);
        }

        Value result = dispatcher(i, fn->body);
        exit_scope(i);
        vec_free(&arg_values);

        ValueType expected_return = expected_value_type(fn->return_type);
        if (result.type != expected_return) {
            LOG_FATAL("'" SS_FMT "' return: expected %s, got %s",
                      SS_ARG(fn->name),
                      value_type_str(expected_return),
                      value_type_str(result.type));
        }

        return result;
    }

    vec_free(&arg_values);
    LOG_FATAL("not callable: %s", value_type_str(fn_val.type));
}

Value evaluate_match(Interpreter* i, Node* n) {
    Match* m = n->as.match;
    Vector arms = m->arms;

    Value result = NilValue();
    bool matched = false;

    for (usize idx = 0; idx < arms.len; idx++) {
        MatchArm* arm = vec_ref(MatchArm, &arms, idx);

        if (arm->is_wildcard) {
            result = dispatcher(i, arm->arm_result);
            matched = true;
            break;
        }

        Node* binop = Operation(i->yant_ctx, arm->binop, m->subject, arm->pattern);
        Value cmp   = evaluate_operation(i, binop);

        if (cmp.type != VALUE_BOOL) {
            LOG_FATAL("match arm comparison did not return boolean (internal bug)");
        }

        if (cmp.as_bool) {
            result = dispatcher(i, arm->arm_result);
            matched = true;
            break;
        }
    }

    if (!matched) {
        LOG_FATAL("match: no arm matched (wildcard missing? internal bug)");
    }

    ValueType expected = expected_value_type(m->return_type);
    if (result.type != expected) {
        LOG_FATAL(
            "match return type mismatch: expected %s, got %s",
            value_type_str(expected),
            value_type_str(result.type)
        );
    }

    return result;
}
