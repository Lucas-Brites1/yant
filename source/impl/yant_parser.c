#include "../include/yant_token.h"
#include "../include/yant_ast.h"
#include "../include/yant_parser.h"
#include "../include/yant_context.h"
#include "../include/yant_vector.h"
#include "../include/yant_types.h"
#include "../include/logc.h"
#include <stdbool.h>

// Statement Functions
static Node* parse_statement(Parser* p); // dispatcher
static Node* parse_declaration(Parser* p);
static Node* parse_assignment(Parser* p);
static Node* parse_conditional(Parser* p);
static Node* parse_block(Parser* p);
static Node* parse_function_declaration(Parser* p);
static Node* parse_call(Parser* p, Node* identifier);
static Node* parse_match(Parser* p);

// Expression Functions
static Node* parse_expression(Parser* p); // dispatcher
static Node* parse_or(Parser* p);
static Node* parse_and(Parser* p);
static Node* parse_equality(Parser* p);
static Node* parse_comparison(Parser* p);
static Node* parse_addition(Parser* p);
static Node* parse_multiplication(Parser* p);
static Node* parse_primary(Parser* p);

static inline Token peek(Parser* p) {
    return vec_at(Token, p->tokens, p->current);
}

static inline Token peek_at(Parser* p, usize offset) {
    LOG_ASSERT(
        offset + p->current < p->tokens->len,
        "Out-of-bounds access: current=%zu offset=%zu len=%zu",
        p->current, offset, p->tokens->len
    );
    return vec_at(Token, p->tokens, p->current + offset);
}

static inline Token advance(Parser* p) {
    Token tk = peek(p);
    p->current++;
    return tk;
}

static inline Token advance_n(Parser* p, usize n) {
    Token tk;
    if (n == 0) n = 1;
    for (usize i = 0; i <= n; i++) tk = advance(p);
    return tk;
}

static inline bool check(Parser* p, TokenType type) {
    return peek(p).type == type;
}

static inline bool match(Parser* p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

static inline Param create_param(StringSlice pname, TokenType ptype) {
    return (Param) {
        .param_name = pname,
        .param_type = ptype
    };
}

static inline MatchArm create_match_arm(Node* pattern, Node* result, TokenType binop, bool is_wildcard) {
    return (MatchArm) {
        .binop = binop,
        .pattern = pattern,
        .arm_result = result,
        .is_wildcard = is_wildcard
    };
}

static inline Token expect(Parser* p, TokenType expected) {
    if (!check(p, expected)) {
        Token t = peek(p);
        LOG_FATAL(
            "expected %s, got %s at line %zu column %zu",
            token_type_str(expected), token_type_str(t.type), t.line, t.column
        );
    }
    return advance(p);
}

static inline bool is_eof(Parser* p) {
    return peek(p).type == TOKEN_EOF;
}

Parser parser_create(YantContext* yant_context, Vector* tokens) {
    LOG_ASSERT(yant_context && tokens, "Error while trying to create Parser due to possible null pointers");
    return (Parser) {
        .yant_ctx = yant_context,
        .tokens   = tokens,
        .current  = 0
    };
}

static inline bool is_type(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_TYPE_STRING,
        TOKEN_TYPE_INT,
        TOKEN_TYPE_FLOAT,
        TOKEN_TYPE_BOOLEAN,
        TOKEN_TYPE_FUNCTION,
        TOKEN_NIL
    );
}


static inline bool is_statement_keyword(Parser* p) {
    return TYPE_IN(peek(p).type,
        TOKEN_KEYWORD_SET,
        TOKEN_KEYWORD_FN,
        TOKEN_KEYWORD_IF,
        TOKEN_KEYWORD_MATCH
        //TOKEN_KEYWORD_LOOP
    );
}

static inline bool is_conditional_keyword(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_KEYWORD_IF,
        TOKEN_KEYWORD_COND
    );
}

static inline bool is_addition_op(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_PLUS,
        TOKEN_MINUS
    );
}

static inline bool is_comparision_op(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_GTE,
        TOKEN_LTE,
        TOKEN_GT,
        TOKEN_LT
    );
}

static inline bool is_equality_op(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_EQEQ,
        TOKEN_NOTEQ
    );
}

static inline bool is_multiplication_op(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_STAR,
        TOKEN_SLASH
    );
}

static inline Vector params_from_tokens(Parser* p) {
    Vector params = vec_of(Param);

    expect(p, TOKEN_LEFT_PARENTHESES);
    //fn:sum(integer:x, integer:y):integer:{}
    while (!check(p, TOKEN_RIGHT_PARENTHESES) && !is_eof(p)) {
        if (!is_type(p)) {
            Token bad = peek(p);
            LOG_FATAL("expected type keyword, got %s at %zu:%zu",
                              token_type_str(bad.type), bad.line, bad.column);
        }

        Token ptype = advance(p);
        expect(p, TOKEN_COLON);
        Token pname = advance(p);

        Param param = create_param(pname.lexeme, ptype.type);
        vec_push(&params, &param);

        if (!check(p, TOKEN_RIGHT_PARENTHESES)) {
            expect(p, TOKEN_COMMA);
        }
    }

    expect(p, TOKEN_RIGHT_PARENTHESES);
    return params;
}

static inline Vector args_from_callee(Parser* p) {
    Vector args = vec_of(Node*);

    expect(p, TOKEN_LEFT_PARENTHESES);
    while (!check(p, TOKEN_RIGHT_PARENTHESES) && !is_eof(p)) {
        Node* arg = parse_expression(p);
        vec_push(&args, &arg);
        match(p, TOKEN_COMMA);
    }
    expect(p, TOKEN_RIGHT_PARENTHESES);

    return args;
}

static Node* parse_expression(Parser* parser) {
    return parse_or(parser);
}

static Node* parse_or(Parser* p) {
    Node* left = parse_and(p);

    while (peek(p).type == TOKEN_OR) {
        Token op = advance(p);
        Node* right = parse_and(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_and(Parser* p) {
    Node* left = parse_equality(p);

    while (peek(p).type == TOKEN_AND) {
        Token op = advance(p);
        Node* right = parse_equality(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_equality(Parser* p) {
    Node* left = parse_comparison(p);

    while (is_equality_op(p)) {
        Token op = advance(p);
        Node* right = parse_comparison(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_comparison(Parser* p) {
    Node* left = parse_addition(p);

    while (is_comparision_op(p)) {
        Token op = advance(p);
        Node* right = parse_addition(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_addition(Parser* p) {
    Node* left = parse_multiplication(p);

    while (is_addition_op(p)) {
        Token op = advance(p);
        Node* right = parse_multiplication(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_multiplication(Parser* p) {
    Node* left = parse_primary(p);

    while (is_multiplication_op(p)) {
        Token op    = advance(p);
        Node* right = parse_primary(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_primary(Parser* p) {
    if (check(p, TOKEN_IDENTIFIER)) {
        Token tk = advance(p);
        Node* ident = Identifier(p->yant_ctx, tk.lexeme, tk.line, tk.column);

        if (check(p, TOKEN_LEFT_PARENTHESES)) {
            return parse_call(p, ident);
        }

        return ident;
    }
    if (check(p, TOKEN_NIL)) {
        Token tk_nil = advance(p);
        return Nil(p->yant_ctx, tk_nil.line, tk_nil.column);
    }
    if (check(p, TOKEN_LITERAL_INTEGER)) {
        Token tk_int = advance(p);
        return LiteralInteger(p->yant_ctx, tk_int.literal.integer_value, tk_int.line, tk_int.column);
    }
    if (check(p, TOKEN_LITERAL_FLOAT)) {
        Token tk_float = advance(p);
        return LiteralFloat(p->yant_ctx, tk_float.literal.float_value, tk_float.line, tk_float.column);
    }
    if (check(p, TOKEN_LITERAL_STRING)) {
        Token tk_string = advance(p);
        return LiteralString(p->yant_ctx, tk_string.lexeme, tk_string.line, tk_string.column);
    }
    if (check(p, TOKEN_LITERAL_BOOLEAN)) {
        Token tk_bool = advance(p);
        return LiteralBoolean(p->yant_ctx, tk_bool.literal.boolean_value, tk_bool.line, tk_bool.column);
    }
    if (check(p, TOKEN_KEYWORD_IF)) {
        return parse_conditional(p);
    }
    if (check(p, TOKEN_LEFT_PARENTHESES)) {
        advance(p);
        Node* expr = parse_addition(p);
        expect(p, TOKEN_RIGHT_PARENTHESES);
        return expr;
    }
    if (check(p, TOKEN_LEFT_BRACE)) {
        Node* block = parse_block(p);
        return block;
    }
    if (check(p, TOKEN_KEYWORD_MATCH)) {
        Node* match = parse_match(p);
        return match;
    }

    Token t = peek(p);
    LOG_FATAL("expected expression, got %s at line %zu column %zu",
              token_type_str(t.type), t.line, t.column);
    return nil;
}

static Node* parse_conditional(Parser* p) {
    Token cond_tk = advance(p);

    switch (cond_tk.type) {
        // if(conditional, then, else)
        case TOKEN_KEYWORD_IF: {
            expect(p, TOKEN_LEFT_PARENTHESES);
            Node* base_cond = parse_expression(p);
            expect(p, TOKEN_COMMA);
            Node* then_cond = parse_expression(p);
            expect(p, TOKEN_COMMA);
            Node* else_cond = parse_expression(p);
            expect(p, TOKEN_RIGHT_PARENTHESES);
            return If(p->yant_ctx, base_cond, then_cond, else_cond, cond_tk.line, cond_tk.column);
        }
        case TOKEN_KEYWORD_COND: TODO();
        default: UNREACHABLE();
    }
}

static Node* parse_statement(Parser* p) {
    if (is_type(p)) return parse_declaration(p);
    if (is_statement_keyword(p)) {
        if (is_conditional_keyword(p))     return parse_conditional(p);
        if (check(p, TOKEN_KEYWORD_SET))   return parse_assignment(p);
        if (check(p, TOKEN_KEYWORD_FN))    return parse_function_declaration(p);
        if (check(p, TOKEN_KEYWORD_MATCH)) return parse_match(p);

        Token tk = peek(p);
        TODO("implement %s", token_type_str(tk.type));
    }

    return parse_expression(p);
}

static Node* parse_declaration(Parser* p) {
    // [TOKEN_KEYWORD_..., TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_LPARENTHESES, VALUE, TOKEN_RPARENTHESES]
    Token kind = advance(p);
    expect(p, TOKEN_COLON); // :
    Token identifier = expect(p, TOKEN_IDENTIFIER);
    expect(p, TOKEN_LEFT_PARENTHESES);
    Node* value = parse_expression(p);
    expect(p, TOKEN_RIGHT_PARENTHESES);
    return Declare(p->yant_ctx, kind.type, identifier.lexeme, value, kind.line, kind.column);
}

static Node* parse_assignment(Parser* p) {
    // [TOKEN_SET, TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_LPARENTHESES, VALUE, TOKEN_RPARENTHESES]
    Token set = advance(p); // consumes Token::Set
    expect(p, TOKEN_COLON);
    Token identifier = advance(p);
    expect(p, TOKEN_LEFT_PARENTHESES);
    Node* value = parse_expression(p);
    expect(p, TOKEN_RIGHT_PARENTHESES);
    return Assign(p->yant_ctx, identifier.lexeme, value, set.line, set.column);
}

static Node* parse_block(Parser* p) {
    // [TOKEN_LBRACE, ...., TOKEN_RBRACE]
    Token lbrace = advance(p); // consumes Token::LeftBrace

    Vector statements = vec_of(Node*);
    while (!check(p, TOKEN_RIGHT_BRACE) && !is_eof(p)) {
        Node* statement = parse_statement(p);
        vec_push(&statements, &statement);
    }
    expect(p, TOKEN_RIGHT_BRACE);

    return Block(p->yant_ctx, statements, lbrace.line, lbrace.column);
}

static Node* parse_function_declaration(Parser* p) {
   // [
   //   TOKEN_KEYWORD_FN, TOKEN_COLON, TOKEN_IDENTIFIER,
   //
   //   TOKEN_LEFTPARENTHESES
   //   N(
   //   TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_COMMA,
   //   TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_COMMA
   //   )
   //   TOKEN_RIGHTPARENTHESES
   //
   //   TOKEN_COLON, TOKEN_KEYWORD,
   //   TOKEN_BLOCK
   // ]
   Token fn_identifier = advance_n(p, 2);
   if (fn_identifier.type != TOKEN_IDENTIFIER) {
       LOG_FATAL("at line:%ld, column: %ld expect %s, recieved: %s", fn_identifier.line, fn_identifier.column, token_type_str(TOKEN_IDENTIFIER), token_type_str(fn_identifier.type));
   }

   Vector params = params_from_tokens(p);
   expect(p, TOKEN_COLON);
   if (!is_type(p)) {
       LOG_FATAL("expects type to be keyword");
   }
   Token ret = advance(p);
   Node* body = parse_block(p);

   return FnDeclare(
       p->yant_ctx,
       fn_identifier.lexeme,
       params,
       body,
       ret.type,
       fn_identifier.line,
       fn_identifier.column
   );
}

static Node* parse_call(Parser* p, Node* identifier) {
    Vector args = args_from_callee(p);
    return Call(p->yant_ctx, identifier, args, identifier->line, identifier->column);
}

/*
 *
 match(sum(30, 20)):string {
   < 18 -> "nao permitido",
   18   -> "permitido agora!",
   _    -> "permitido"
 }
 *
 */
static Node* parse_match(Parser* p) {
    Token match_tk    = advance(p);

    expect(p, TOKEN_LEFT_PARENTHESES);
    Node* subject = parse_expression(p);
    expect(p, TOKEN_RIGHT_PARENTHESES);

    expect(p, TOKEN_COLON);
    if (!is_type(p)) {
        Token bad = peek(p);
        LOG_FATAL(
            "expected type after match(...): got %s at %zu:%zu",
            token_type_str(bad.type), bad.line, bad.column
        );
    }
    Token return_type_tk = advance(p);

    // { arms... }
    Vector arms = vec_of(MatchArm);

    expect(p, TOKEN_LEFT_BRACE);
    while (!check(p, TOKEN_RIGHT_BRACE) && !is_eof(p)) {
        MatchArm arm;
        TokenType binop = TOKEN_EQEQ;

        if (check(p, TOKEN_UNDERSCORE)) {
            advance(p);
            expect(p, TOKEN_ARROW);
            Node* result = parse_expression(p);
            arm = create_match_arm(NULL, result , TOKEN_EQEQ, true);
        } else {
            if (is_comparision_op(p) || is_equality_op(p)) binop = advance(p).type;
            Node* pattern = parse_expression(p);
            expect(p, TOKEN_ARROW);
            Node* result  = parse_expression(p);

            arm = create_match_arm(pattern, result, binop, false);
        }

        vec_push(&arms, &arm);
        if (!check(p, TOKEN_RIGHT_BRACE)) expect(p, TOKEN_COMMA);
    }
    expect(p, TOKEN_RIGHT_BRACE);

    if (arms.len == 0) {
        LOG_FATAL("match must have at least one arm");
    }

    MatchArm* last = vec_ref(MatchArm, &arms, arms.len - 1);
    if (!last->is_wildcard) {
        LOG_FATAL("match must end with wildcard arm '_ -> ...'");
    }

    for (usize i = 0; i + 1 < arms.len; i++) {
        MatchArm* a = vec_ref(MatchArm, &arms, i);
        if (a->is_wildcard) {
            LOG_FATAL("wildcard '_' must be the last arm");
        }
    }

    return Matcher(p->yant_ctx, subject, arms, return_type_tk.type, match_tk.line, match_tk.column);
}

Vector parse(Parser* p) {
    LOG_ASSERT(p && p->yant_ctx && p->tokens, "Failed trying to parse due to possible nil pointer");
    Vector nodes = vec_of(Node*);

    while (!is_eof(p)) {
        Node* node = parse_statement(p);
        vec_push(&nodes, &node);
    }

    return nodes;
}
