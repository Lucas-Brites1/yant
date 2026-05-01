#include "../include/yant_ast.h"
#include "../include/yant_context.h"
#include "../include/yant_types.h"
#include "../include/yant_strings.h"
#include "../include/yant_token.h"
#include "../include/yant_vector.h"
#include "../include/blobberman.h"
#include "../include/logc.h"

static inline Node* alloc_new_node(Blob* ast_blob, NodeType type, usize line, usize column) {
    LOG_ASSERT(ast_blob, "Failed due to invalid pointer as ast blob");
    Node* new_node = (Node*)blob_reserve(ast_blob, sizeof(Node));
    LOG_ASSERT(new_node, "Failed trying to allocate new ast node");
    new_node->type   = type;
    new_node->line   = line;
    new_node->column = column;
    return new_node;
}

static inline Function* alloc_new_function(Blob* ast_blob) {
    LOG_ASSERT(ast_blob, "Failed due to invalid pointer as ast blob");
    Function* fn = (Function*)blob_reserve(ast_blob, sizeof(Function));
    LOG_ASSERT(fn, "Failed trying to allocate new function");
    return fn;
}

static inline Match*  alloc_new_match(Blob *ast_blob) {
    LOG_ASSERT(ast_blob, "Failed due to invalid pointer as ast blob");
    Match* match = (Match*)blob_reserve(ast_blob, sizeof(Match));
    LOG_ASSERT(match, "Failed trying to allocate new match");
    return match;
}

Node* LiteralInteger (YantContext* ctx, i64 value, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_LITERAL_INTEGER, line, col);
    n->as.int_literal.value = value;
    return n;
}

Node* LiteralFloat   (YantContext* ctx, f64 value, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_LITERAL_FLOAT, line, col);
    n->as.float_literal.value = value;
    return n;
}

Node* LiteralString  (YantContext* ctx, StringSlice value, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_LITERAL_STRING, line, col);
    n->as.string_literal.value = value;
    return n;
}

Node* LiteralBoolean (YantContext* ctx, bool value, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_LITERAL_BOOL, line, col);
    n->as.boolean_literal.value = value;
    return n;
}

Node* Identifier     (YantContext* ctx, StringSlice name, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_IDENTIFIER, line, col);
    n->as.identifier.name = name;
    return n;
}

Node* Operation      (YantContext* ctx, TokenType op, Node* left, Node* right) {
    Node* n = alloc_new_node(ctx->ast, NODE_BINARY_OP, left->line, left->column);
    n->as.binary.op = op;
    n->as.binary.left = left;
    n->as.binary.right = right;
    return n;
}

Node* Declare        (YantContext* ctx, TokenType kind, StringSlice name, Node* value, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_DECLARATION, line, col);
    n->as.declare.kind  = kind;
    n->as.declare.name  = name;
    n->as.declare.value = value;
    return n;
}

Node* Assign         (YantContext* ctx, StringSlice name, Node* value, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_ASSIGNMENT, line, col);
    n->as.assign.name = name;
    n->as.assign.value = value;
    return n;
}

Node* If             (YantContext* ctx, Node* base_cond, Node* then_cond, Node* else_cond, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_IF, line, col);
    n->as.if_expression.base_cond = base_cond;
    n->as.if_expression.then_cond = then_cond;
    n->as.if_expression.else_cond = else_cond;
    return n;
}

Node* Block         (YantContext* ctx, Vector statements, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_BLOCK, line, col);
    n->as.block.statements = statements;
    return n;
}

Node* Call           (YantContext* ctx, Node* callee, Vector args, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_CALL, line, col);
    n->as.call.callee = callee;
    n->as.call.arguments = args;
    return n;
}

Node* Nil            (YantContext* ctx, usize line, usize col) {
    Node*n = alloc_new_node(ctx->ast, NODE_LITERAL_NIL, line, col);
    return n;
}

Node* FnDeclare      (YantContext* ctx, StringSlice name, Vector params, Node* body, TokenType return_type, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_FNDECLARE, line, col);
    Function* fn = alloc_new_function(ctx->ast);

    fn->name   = name;
    fn->params = params;
    fn->body   = body;
    fn->return_type = return_type;

    n->as.function = fn;
    return n;
}

Node* Matcher          (YantContext* ctx, Node* subject, Vector arms, TokenType match_return_type, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_MATCH, line, col);
    Match* match = alloc_new_match(ctx->ast);
    match->arms = arms;
    match->subject = subject;
    match->return_type = match_return_type;
    n->as.match = match;
    return n;
}


void node_print(Node* n, int depth) {
    for (int i = 0; i < depth * 2; i++) putchar(' ');
    NodeType t = n->type;

    switch (n->type) {
        case NODE_LITERAL_INTEGER:
            printf("%s(%ld)\n", node_type_str(t), n->as.int_literal.value);
            break;
        case NODE_LITERAL_FLOAT:
            printf("%s(%f)\n", node_type_str(t), n->as.float_literal.value);
            break;
        case NODE_LITERAL_STRING:
            printf("%s(\"" SS_FMT "\")\n", node_type_str(t), SS_ARG(n->as.string_literal.value));
            break;
        case NODE_LITERAL_BOOL:
            printf("%s(%s)\n", node_type_str(t), n->as.boolean_literal.value ? "true" : "false");
            break;
        case NODE_LITERAL_NIL:
            printf("%s(nil)\n", node_type_str(t));
            break;
        case NODE_IDENTIFIER:
            printf("%s(" SS_FMT ")\n", node_type_str(t), SS_ARG(n->as.identifier.name));
            break;
        case NODE_BINARY_OP:
            printf("%s(%s)\n", node_type_str(t), token_type_str(n->as.binary.op));
            node_print(n->as.binary.left, depth + 1);
            node_print(n->as.binary.right, depth + 1);
            break;
        case NODE_DECLARATION:
            printf("%s(%s, " SS_FMT ")\n",
                node_type_str(t),
                token_type_str(n->as.declare.kind),
                SS_ARG(n->as.declare.name)
            );
            node_print(n->as.declare.value, depth + 1);
            break;
        case NODE_ASSIGNMENT:
            printf("%s(" SS_FMT ")\n", node_type_str(t), SS_ARG(n->as.assign.name));
            node_print(n->as.assign.value, depth + 1);
            break;
        case NODE_FNDECLARE:
            printf("%s:" SS_FMT"(", node_type_str(t), SS_ARG(n->as.function->name));
            Vector params = n->as.function->params;
            for (usize i = 0; i < params.len; i++) {
                Param* param = vec_ref(Param, &params, i);
                if (i == params.len - 1) {
                    printf("%s:" SS_FMT, token_type_str(param->param_type), SS_ARG(param->param_name));
                    break;
                }
                printf("%s:" SS_FMT ", ", token_type_str(param->param_type), SS_ARG(param->param_name));
            }
            printf("):%s ", token_type_str(n->as.function->return_type));
            node_print(n->as.function->body, 0);
            break;
        case NODE_CALL:
            printf("%s\n", node_type_str(t));
            node_print(n->as.call.callee, depth + 1);
            break;
        case NODE_IF:
            printf("%s\n", node_type_str(t));
            node_print(n->as.if_expression.base_cond, depth + 1);
            node_print(n->as.if_expression.then_cond, depth + 1);
            node_print(n->as.if_expression.else_cond, depth + 1);
            break;
        case NODE_BLOCK:
            printf("%s::Begin\n", node_type_str(t));
            vec_foreach(Node*, st, &n->as.block.statements) {
                node_print(*st, 1);
            }
            printf("%s::End\n", node_type_str(t));
            break;
        case NODE_MATCH: {
                Match* m = n->as.match;
                printf("%*sNode::Match\n", depth*2, "");
                printf("%*s  subject:\n", depth*2, "");
                node_print(m->subject, depth + 2);
                printf("%*s  arms (%zu):\n", depth*2, "", m->arms.len);
                vec_foreach(MatchArm, arm, &m->arms) {
                    if (arm->is_wildcard) {
                        printf("%*s    [wildcard] ->\n", depth*2, "");
                    } else {
                        printf("%*s    [%s]\n", depth*2, "", token_type_str(arm->binop));
                        node_print(arm->pattern, depth + 3);
                        printf("%*s    ->\n", depth*2, "");
                    }
                    node_print(arm->arm_result, depth + 3);
                }
                break;
            }
        default:
            TODO("Unknown node type: %s\n", node_type_str(n->type));
    }
}

void node_free(Node* n) {
    if (!n) return;
    switch (n->type) {
        case NODE_IDENTIFIER: break;
        case NODE_BLOCK:
            vec_foreach(Node*, child, &n->as.block.statements) {
                node_free(*child);
            }
            vec_free(&n->as.block.statements);
            break;
        case NODE_FNDECLARE:
            vec_free(&n->as.function->params);
            node_free(n->as.function->body);
            break;
        case NODE_MATCH:
            Match* m    = n->as.match;
            Vector arms = m->arms;
            vec_foreach(MatchArm, arm, &arms) {
                if (!arm->is_wildcard) {
                    node_free(arm->pattern);
                }
                node_free(arm->arm_result);
            }
            vec_free(&arms);
            break;
        case NODE_CALL:
            node_free(n->as.call.callee);
            Vector args = n->as.call.arguments;
            vec_foreach(Node*, arg, &args) {
                node_free(*arg);
            }
            vec_free(&args);
            break;
        case NODE_BINARY_OP:
            node_free(n->as.binary.left);
            node_free(n->as.binary.right);
            break;
        case NODE_IF:
            node_free(n->as.if_expression.base_cond);
            node_free(n->as.if_expression.then_cond);
            node_free(n->as.if_expression.else_cond);
            break;
        case NODE_DECLARATION:
            node_free(n->as.declare.value);
            break;
        case NODE_ASSIGNMENT:
            node_free(n->as.assign.value);
            break;
        default: break;
    }
}
