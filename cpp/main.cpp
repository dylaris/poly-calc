#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <string_view>

//////////////
// Diagnostics
//////////////

struct Diagnostics {
    bool has_error{ false };
    std::string_view input;

    explicit Diagnostics(std::string_view input) : input{ input } {}

    void report(const char* position, std::string_view message) {
        if (has_error) return;
        std::cerr << "error: " << message << "\n";
        std::cerr << "input: " << input << "\n";
        std::cerr << "       ";
        for (size_t i = 0; i < position - input.data(); i++) {
            std::cerr << ' ';
        }
        std::cerr << "^\n";
        has_error = true;
    }
};

////////////
// Lexer
////////////

enum class TokenKind {
    Integer,
    Plus,
    Minus,
    Star,
    Slash,
    LParen,
    RParen,
    Eof,
};

struct Token {
    TokenKind kind;
    std::string_view lexeme;
    explicit Token(TokenKind kind, std::string_view lexeme) : kind{ kind }, lexeme{ lexeme } {}
};

struct Lexer {
    std::vector<Token> tokens;

    explicit Lexer(std::string_view input) : input{ input }, diag{ input } {}

    bool tokenize() {
        const char* curr = input.data();
        while (*curr) {
            const char* start = curr;

            while (is_digit(*curr)) curr++;
            if (start != curr) {
                std::string_view lexeme{ start, static_cast<size_t>(curr - start) };
                tokens.emplace_back(TokenKind::Integer, lexeme);
                continue;
            }

            switch (*curr++) {
            case '+': tokens.emplace_back(TokenKind::Plus,   std::string_view{ start, 1 }); break;
            case '-': tokens.emplace_back(TokenKind::Minus,  std::string_view{ start, 1 }); break;
            case '*': tokens.emplace_back(TokenKind::Star,   std::string_view{ start, 1 }); break;
            case '/': tokens.emplace_back(TokenKind::Slash,  std::string_view{ start, 1 }); break;
            case '(': tokens.emplace_back(TokenKind::LParen, std::string_view{ start, 1 }); break;
            case ')': tokens.emplace_back(TokenKind::RParen, std::string_view{ start, 1 }); break;
            case ' ':
            case '\t':
                continue;
            default:
                diag.report(start, "unknown char");
                return false;
            }
        }

        tokens.emplace_back(TokenKind::Eof, std::string_view{ curr, 0 });
        return true;
    }

    void print() const {
        for (const auto& tok : tokens) {
            if (tok.kind != TokenKind::Eof) {
                std::cout << tok.lexeme << "\n";
            }
        }
    }

private:
    std::string_view input;
    Diagnostics diag;

    bool is_digit(char c) const {
        return c >= '0' && c <= '9';
    }
};

////////////
// Parser
////////////

enum class Op { Add, Sub, Mul, Div, Neg, Pos };
enum class AstNodeKind { Binop, Unaop, Integer };

struct AstNode {
    AstNodeKind kind;
    Op op;
    std::unique_ptr<AstNode> left;
    std::unique_ptr<AstNode> right;
    int value;

    explicit AstNode(int val) : kind{ AstNodeKind::Integer }, value{ val } {}
    explicit AstNode(Op op, std::unique_ptr<AstNode> l, std::unique_ptr<AstNode> r)
        : kind{ AstNodeKind::Binop }, op{ op }, left{ std::move(l) }, right{ std::move(r) } {}
    explicit AstNode(Op op, std::unique_ptr<AstNode> l)
        : kind{ AstNodeKind::Unaop }, op{ op }, left{ std::move(l) } {}
};

struct Parser {
    std::unique_ptr<AstNode> root{ nullptr };

    explicit Parser(std::string_view input, std::vector<Token> tokens)
        : input{ input }, tokens{ tokens }, diag{ input } {}

    Token peek() const {
        return tokens[curr];
    }

    void consume() {
        curr++;
    }

    bool parse() {
        root = parse_expr();

        Token tok{ peek() };
        if (tok.kind != TokenKind::Eof) {
            diag.report(tok.lexeme.data(), "unexpected token");
        }

        if (diag.has_error == true) return false;
        return true;
    }

private:
    std::vector<Token> tokens;
    size_t curr = 0;
    std::string_view input;
    Diagnostics diag;

    std::unique_ptr<AstNode> parse_expr() {
        auto left = parse_term();
        while (true) {
            const auto& tok = peek();
            Op op;
            switch (tok.kind) {
            case TokenKind::Plus:  op = Op::Add; break;
            case TokenKind::Minus: op = Op::Sub; break;
            default: return left;
            }
            consume();
            auto right = parse_term();
            left = std::make_unique<AstNode>(op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<AstNode> parse_term() {
        auto left = parse_factor();
        while (true) {
            const auto& tok = peek();
            Op op;
            switch (tok.kind) {
            case TokenKind::Star:  op = Op::Mul; break;
            case TokenKind::Slash: op = Op::Div; break;
            default: return left;
            }
            consume();
            auto right = parse_factor();
            left = std::make_unique<AstNode>(op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<AstNode> parse_factor() {
        const auto& tok = peek();
        if (tok.kind == TokenKind::Plus || tok.kind == TokenKind::Minus) {
            consume();
            Op op{ tok.kind == TokenKind::Plus ? Op::Pos : Op::Neg };
            auto left = parse_atom();
            return std::make_unique<AstNode>(op, std::move(left));
        }
        return parse_atom();
    }

    std::unique_ptr<AstNode> parse_atom() {
        auto tok = peek();
        if (tok.kind == TokenKind::Integer) {
            consume();
            return std::make_unique<AstNode>(std::stoi(std::string(tok.lexeme)));
        } else if (tok.kind == TokenKind::LParen) {
            consume();
            auto left = parse_expr();
            tok = peek();
            if (tok.kind != TokenKind::RParen) {
                const char* pos = tok.lexeme.data();
                diag.report(pos, "unclosed parenthesis");
                return nullptr;
            }
            consume();
            return left;
        } else {
            const char* pos = tok.lexeme.data();
            diag.report(pos, "expect number or '('");
            return nullptr;
        }
    }
};

////////////
// Evaluator
////////////

float evaluate(std::unique_ptr<AstNode> root) {
    if (root == nullptr) return 0;

    switch (root->kind) {
    case AstNodeKind::Integer:
        return static_cast<float>(root->value);

    case AstNodeKind::Unaop: {
        float value = evaluate(std::move(root->left));
        switch (root->op) {
        case Op::Pos: return value;
        case Op::Neg: return -value;
        default:      return 0; // unreachable
        }
    }

    case AstNodeKind::Binop: {
        float lv = evaluate(std::move(root->left));
        float rv = evaluate(std::move(root->right));
        switch (root->op) {
        case Op::Add: return lv + rv;
        case Op::Sub: return lv - rv;
        case Op::Mul: return lv * rv;
        case Op::Div: return lv / rv;
        default:      return 0; // unreachable
        }
    }
    }
}

int main() {
    std::string input;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        Lexer lexer{ input };
        if (!lexer.tokenize()) continue;
        // lexer.print();

        Parser parser{ input, lexer.tokens };
        if (!parser.parse()) continue;

        float value = evaluate(std::move(parser.root));
        std::cout << "result: " << value << '\n';
    }

    return 0;
}
