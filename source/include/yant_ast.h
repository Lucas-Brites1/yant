#pragma once
#include "logc.h"
#include "yant_token.h"
#include "yant_types.h"
#include "yant_context.h"
#include "yant_strings.h"
#include "yant_vector.h"

typedef struct Node Node;

#define NODES_TABLE(extends) \
    extends(NODE_TYPE_SIMPLE,        "Node::T::Simple")     \
    extends(NODE_TYPE_GENERIC,       "Node::T::Generic")    \
    extends(NODE_LITERAL_STRING,     "Node::LitString")     \
    extends(NODE_LITERAL_INTEGER,    "Node::LitInteger")    \
    extends(NODE_LITERAL_FLOAT,      "Node::LitFloat")      \
    extends(NODE_LITERAL_BOOL,       "Node::LitBoolean")    \
    extends(NODE_LITERAL_NIL,        "Node::LitNil")        \
    extends(NODE_IF,                 "Node::If")            \
    extends(NODE_IDENTIFIER,         "Node::Identifier")    \
    extends(NODE_BINARY_OP,          "Node::BinaryOp")      \
    extends(NODE_BLOCK,              "Node::Block")         \
    extends(NODE_FNDECLARE,          "Node::FunctionDecl")  \
    extends(NODE_CALL,               "Node::Call")          \
    extends(NODE_DECLARATION,        "Node::Declaration")   \
    extends(NODE_ASSIGNMENT,         "Node::Assignment")

#define X_AS_ENUM(T, str) T,
typedef enum {
    NODES_TABLE(X_AS_ENUM)
    NODE_COUNT
} NodeType;
#undef X_AS_ENUM

#define X_AS_STRING(T, str) case T: return str;
static inline const char* node_type_str(NodeType type) {
    switch (type) {
        NODES_TABLE(X_AS_STRING)
        default:
            LOG_FATAL("Unknown node type");
            return "Unknown";  // unreachable
    }
}
#undef X_AS_STRING

typedef struct {
    StringSlice param_name;
    TokenType   param_type;
} Param;


typedef struct {
    Vector      params; // vec_of(Param)
    StringSlice name;
    Node*       body;
    TokenType   return_type;
} Function;

struct Node {
    NodeType type;
    usize    line;
    usize column;
    union {
        struct { i64 value; }         int_literal;
        struct { f64 value; }         float_literal;
        struct { StringSlice value; } string_literal;
        struct { StringSlice name; }  identifier;
        struct { bool value; }        boolean_literal;

        struct {
            TokenType op;
            Node* left;
            Node* right;
        } binary;

        struct {
            TokenType   kind;
            StringSlice name;
            Node*       value;
        } declare;

        struct {
            StringSlice name;
            Node*       value;
        } assign;

        struct {
            // if(base,then,else)
            Node* base_cond;
            Node* then_cond;
            Node* else_cond;
        } if_expression;

        struct {
            Vector statements;
        } block;

        struct {
            Node*  callee;
            Vector arguments;
        } call;

        struct {
            TokenType kind;
        } simple_type;

        struct {
            TokenType kind;
            Vector    typed_arguments;
        } generic_type;

        Function* function;
    } as;
};

Node* LiteralInteger (YantContext* ctx, i64 value, usize line, usize col);
Node* LiteralFloat   (YantContext* ctx, f64 value, usize line, usize col);
Node* LiteralString  (YantContext* ctx, StringSlice value, usize line, usize col);
Node* LiteralBoolean (YantContext* ctx, bool value, usize line, usize col);
Node* Identifier     (YantContext* ctx, StringSlice name, usize line, usize col);
Node* Operation      (YantContext* ctx, TokenType op, Node* left, Node* right);
Node* Declare        (YantContext* ctx, TokenType kind, StringSlice name, Node* value, usize line, usize col);
Node* Assign         (YantContext* ctx, StringSlice name, Node* value, usize line, usize col);
Node* If             (YantContext* ctx, Node* base_cond, Node* then_cond, Node* else_cond, usize line, usize col);
Node* Block          (YantContext* ctx, Vector statements, usize line, usize col);
Node* FnDeclare      (YantContext* ctx, StringSlice name, Vector params, Node* body, TokenType return_type, usize line, usize col);
Node* Call           (YantContext* ctx, Node* callee, Vector args, usize line, usize col);
Node* Nil            (YantContext* ctx, usize line, usize col);
Node* Eof            (YantContext* ctx, usize line, usize col);

void node_print(Node* n, int depth);
void node_free(Node* n);
