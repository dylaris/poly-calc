#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/////////////////
// Data Type
/////////////////

typedef enum {
    TOK_INTEGER,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF,
} TokenKind;

typedef struct {
    TokenKind kind;
    struct {
        const char *start;
        size_t length;
    } lexeme;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenStream;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NEG,
    OP_POS,
} OP;

typedef enum {
    AST_NUMBER,
    AST_UNARY,
    AST_BINARY,
} ASTNodeKind;

typedef struct ASTNode ASTNode;
struct ASTNode {
    ASTNodeKind kind;
    OP op;
    ASTNode *left;
    ASTNode *right;
    int value;
};

/////////////////
// Global Var
/////////////////

const char *g_input = NULL;
bool g_error = false;
TokenStream g_toks = {0};
size_t g_index = 0;

/////////////////
// Tokenizer
/////////////////

TokenStream tokenize(const char *input);
Token make_token(TokenKind kind, const char *start, size_t length);
void print_token_stream(const TokenStream *toks);

/////////////////
// Parser
/////////////////

Token peek(void);
void consume(void);
ASTNode *parse(void);
ASTNode *parse_expr(void);
ASTNode *parse_term(void);
ASTNode *parse_factor(void);
ASTNode *parse_atom(void);
ASTNode *make_node(ASTNodeKind kind, OP op, ASTNode *left, ASTNode *right, int value);
void free_nodes(ASTNode *root);

/////////////////
// Evaluator
/////////////////

float evaluate(ASTNode *root)
{
    if (!root) return 0.0f;

    switch (root->kind) {
    case AST_NUMBER:
        return (float) root->value;

    case AST_UNARY: {
        float val = evaluate(root->left);
        switch (root->op) {
        case OP_POS: return val;
        case OP_NEG: return -val;
        default: assert(0);
        }
    }

    case AST_BINARY: {
        float l = evaluate(root->left);
        float r = evaluate(root->right);
        switch (root->op) {
        case OP_ADD: return l + r;
        case OP_SUB: return l - r;
        case OP_MUL: return l * r;
        case OP_DIV: return l / r;
        default: assert(0);
        }
    }

    default: assert(0);
    }
}

/////////////////
// Other
/////////////////

void report_error(const char *pos, const char *msg);

#define is_digit(c) ((c) >= '0' && (c) <= '9')

#define da_append(da, item) \
    do { \
        if ((da)->count + 1 > (da)->capacity) { \
            (da)->capacity = (da)->capacity < 32 ? 32 : 2*(da)->capacity; \
            (da)->items = realloc((da)->items, sizeof(*(da)->items)*(da)->capacity); \
            assert((da)->items && "run out of memory"); \
        } \
        (da)->items[(da)->count++] = (item); \
    } while (0)

#define da_free(da) do { if ((da).items) free((da).items); } while (0)

#define da_reset(da) do { (da).count = 0; } while (0)

/////////////////
// main
/////////////////

int main(void)
{
    char input[1024];

    while (true) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
        if (input[0] == '\0') continue;

        g_input = input;
        g_error = false;
        ASTNode *root = NULL;

        g_toks = tokenize(g_input);
        if (g_error) goto cleanup;
        // print_token_stream(&g_toks);

        root = parse();
        if (g_error) goto cleanup;

        float value = evaluate(root);
        printf("result: %g\n", value);

cleanup:
        free_nodes(root);
        da_free(g_toks);
    }

    return 0;
}


TokenStream tokenize(const char *input)
{
    TokenStream toks = {0};
    const char *curr = input;

    while (*curr) {
        Token tok = {0};
        const char *start = curr;

        while (is_digit(*curr)) curr++;
        if (curr != start) {
            tok = make_token(TOK_INTEGER, start, curr - start);
            goto end;
        }

        switch (*curr++) {
        case '+': tok = make_token(TOK_PLUS,   start, 1); break;
        case '-': tok = make_token(TOK_MINUS,  start, 1); break;
        case '*': tok = make_token(TOK_STAR,   start, 1); break;
        case '/': tok = make_token(TOK_SLASH,  start, 1); break;
        case '(': tok = make_token(TOK_LPAREN, start, 1); break;
        case ')': tok = make_token(TOK_RPAREN, start, 1); break;
        case ' ':
        case '\t':
            continue;
        default:
            report_error(start, "unknown char");
            return toks;
        }
end:
        da_append(&toks, tok);
    }

    da_append(&toks, make_token(TOK_EOF, curr, 0));
    return toks;
}

Token make_token(TokenKind kind, const char *start, size_t length)
{
    return (Token) {
        .kind = kind,
        .lexeme.start = start,
        .lexeme.length = length,
    };
}

void print_token_stream(const TokenStream *toks)
{
    for (Token *tok = toks->items; tok->kind != TOK_EOF; tok++) {
        printf("%.*s\n", (int) tok->lexeme.length, tok->lexeme.start);
    }
}

Token peek(void)
{
    assert(g_index < g_toks.count);
    return g_toks.items[g_index];
}

void consume(void)
{
    g_index++;
}

ASTNode *parse_expr(void)
{
    ASTNode *left = parse_term();
    while (true) {
        Token tok = peek();
        OP op;
        switch (tok.kind) {
        case TOK_PLUS:  op = OP_ADD; break;
        case TOK_MINUS: op = OP_SUB; break;
        default:        goto end;
        }
        consume();
        ASTNode *right = parse_term();
        left = make_node(AST_BINARY, op, left, right, 0);
    }
end:
    return left;
}

ASTNode *parse_term(void)
{
    ASTNode *left = parse_factor();
    while (true) {
        Token tok = peek();
        OP op;
        switch (tok.kind) {
        case TOK_STAR:  op = OP_MUL; break;
        case TOK_SLASH: op = OP_DIV; break;
        default:        goto end;
        }
        consume();
        ASTNode *right = parse_factor();
        left = make_node(AST_BINARY, op, left, right, 0);
    }
end:
    return left;
}

ASTNode *parse_factor(void)
{
    Token tok = peek();
    if (tok.kind == TOK_PLUS || tok.kind == TOK_MINUS) {
        OP op = tok.kind == TOK_PLUS ? OP_POS : OP_NEG;
        consume();
        ASTNode *node = parse_atom();
        return make_node(AST_UNARY, op, node, NULL, 0);
    }
    return parse_atom();
}

ASTNode *parse_atom(void)
{
    Token tok = peek();
    switch (tok.kind) {
    case TOK_INTEGER:
        static char buffer[128] = {0};
        snprintf(buffer, sizeof(buffer), "%.*s", (int) tok.lexeme.length, tok.lexeme.start);
        consume();
        return make_node(AST_NUMBER, 0, NULL, NULL, atoi(buffer));
    case TOK_LPAREN:
        consume();
        ASTNode *node = parse_expr();
        tok = peek();
        if (tok.kind != TOK_RPAREN) {
            report_error(tok.lexeme.start, "unclosed parenthesis");
        } else {
            consume();
        }
        return node;
    default:
        report_error(tok.lexeme.start, "expected number or '('");
        return NULL;
    }
}

ASTNode *make_node(ASTNodeKind kind, OP op, ASTNode *left, ASTNode *right, int value)
{
    ASTNode *node = malloc(sizeof(ASTNode));
    assert(node && "run out of memory");
    node->kind = kind;
    node->op = op;
    node->left = left;
    node->right = right;
    node->value = value;
    return node;
}

ASTNode *parse(void)
{
    ASTNode *node = parse_expr();
    da_reset(g_toks);
    g_index = 0;
    return node;
}

void free_nodes(ASTNode *root)
{
    if (!root) return;
    free_nodes(root->left);
    free_nodes(root->right);
    free(root);
}

void report_error(const char *pos, const char *msg)
{
    // only report first error (fail-fast mode)
    if (g_error) return;
    fprintf(stderr, "error: %s\n", msg);
    fprintf(stderr, "input: %s\n", g_input);
    fprintf(stderr, "       ");
    for (int i = 0; i < (int) (pos - g_input); i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^\n");
    g_error = true;
}
