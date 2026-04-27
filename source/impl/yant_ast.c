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

Node* Call           (YantContext* ctx, Node* callee, Vector args, usize line, usize col) {
    Node* n = alloc_new_node(ctx->ast, NODE_CALL, line, col);
    n->as.call.callee = callee;
    n->as.call.arguments = args;
    return n;
}

void node_print(Node* n, int depth) {
    for (int i = 0; i < depth * 2; i++) putchar(' ');

    switch (n->type) {
        case NODE_LITERAL_INTEGER:
            printf("LiteralInteger(%ld)\n", n->as.int_literal.value);
            break;
        case NODE_LITERAL_FLOAT:
            printf("LiteralFloat(%f)\n", n->as.float_literal.value);
            break;
        case NODE_LITERAL_STRING:
            printf("LiteralString(\"" SS_FMT "\")\n", SS_ARG(n->as.string_literal.value));
            break;
        case NODE_LITERAL_BOOL:
            printf("LiteralBoolean(%s)\n", n->as.boolean_literal.value ? "true" : "false");
            break;

        case NODE_IDENTIFIER:
            printf("Identifier(" SS_FMT ")\n", SS_ARG(n->as.identifier.name));
            break;
        case NODE_BINARY_OP:
            printf("Operation(%s)\n", token_type_str(n->as.binary.op));
            node_print(n->as.binary.left, depth + 1);
            node_print(n->as.binary.right, depth + 1);
            break;
        case NODE_DECLARATION:
            printf("Declare(%s, " SS_FMT ")\n",
                   token_type_str(n->as.declare.kind),
                   SS_ARG(n->as.declare.name));
            node_print(n->as.declare.value, depth + 1);
            break;
        case NODE_ASSIGNMENT:
            printf("Assign(" SS_FMT ")\n", SS_ARG(n->as.assign.name));
            node_print(n->as.assign.value, depth + 1);
            break;
        case NODE_CALL:
            printf("Call\n");
            node_print(n->as.call.callee, depth + 1);
            break;
        default:
            fprintf(stderr, "Unknown node type: %d\n", n->type);
    }
}
