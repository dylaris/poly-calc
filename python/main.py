import sys
from enum import IntEnum, auto

##############
# Diagnostics
##############

class Diagnostics:
    def __init__(self, input):
        self.has_error = False
        self.input = input

    def report(self, offset, message):
        if self.has_error:
            return
        print(f"error: {message}", file=sys.stderr)
        print(f"input: {self.input}", file=sys.stderr)
        print("       " + " " * offset + "^", file=sys.stderr)
        self.has_error = True

##########
# Lexer
##########

class TokenKind(IntEnum):
    INTEGER = auto()
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    LPAREN = auto()
    RPAREN = auto()
    EOF = auto()

class Token:
    def __init__(self, kind, offset, lexeme):
        self.kind = kind
        self.offset = offset
        self.lexeme = lexeme

class Lexer:
    def __init__(self, input):
        self.diag = Diagnostics(input)
        self.input = input
        self.tokens = []

    def tokenize(self):
        curr = 0
        length = len(self.input)
        while curr < length:
            start = curr

            while curr < length and self.input[curr].isdigit():
                curr += 1
            if start != curr:
                self.tokens.append(Token(TokenKind.INTEGER, start, self.input[start:curr]))
                continue

            ch = self.input[curr]
            match ch:
                case '+':
                    self.tokens.append(Token(TokenKind.PLUS, start, ch))
                case '-':
                    self.tokens.append(Token(TokenKind.MINUS, start, ch))
                case '*':
                    self.tokens.append(Token(TokenKind.STAR, start, ch))
                case '/':
                    self.tokens.append(Token(TokenKind.SLASH, start, ch))
                case '(':
                    self.tokens.append(Token(TokenKind.LPAREN, start, ch))
                case ')':
                    self.tokens.append(Token(TokenKind.RPAREN, start, ch))
                case ' ' | '\t':
                    pass
                case _:
                    self.diag.report(start, f"unknown char: {ch}")
                    return False
            curr += 1
        self.tokens.append(Token(TokenKind.EOF, curr, None))
        return True

    def print(self):
        for tok in self.tokens:
            if tok.kind != TokenKind.EOF:
                print(tok.lexeme)

##########
# Parser
##########

class AstNodeKind(IntEnum):
    INTEGER = auto()
    BINOP = auto()
    UNAOP = auto()

class Op(IntEnum):
    ADD = auto()
    SUB = auto()
    MUL = auto()
    DIV = auto()
    POS = auto()
    NEG = auto()

class AstNode:
    def __init__(self, kind, op, left, right, value):
        self.kind = kind
        self.op = op
        self.left = left
        self.right = right
        self.value = value

class Parser:
    def __init__(self, input, tokens):
        self.diag = Diagnostics(input)
        self.tokens = tokens
        self.root = None
        self.curr = 0

    def peek(self):
        return self.tokens[self.curr]

    def consume(self):
        self.curr += 1

    def parse(self):
        self.root = self.parse_expr()

        tok = self.peek()
        if tok.kind != TokenKind.EOF:
            self.diag.report(tok.offset, "unexpected token")

        if self.diag.has_error:
            return False
        return True

    def parse_expr(self):
        left = self.parse_term()
        while True:
            tok = self.peek()
            match tok.kind:
                case TokenKind.PLUS:
                    op = Op.ADD
                case TokenKind.MINUS:
                    op = Op.SUB
                case _:
                    return left
            self.consume()
            right = self.parse_term()
            left = AstNode(AstNodeKind.BINOP, op, left, right, None)

    def parse_term(self):
        left = self.parse_factor()
        while True:
            tok = self.peek()
            match tok.kind:
                case TokenKind.STAR:
                    op = Op.MUL
                case TokenKind.SLASH:
                    op = Op.DIV
                case _:
                    return left
            self.consume()
            right = self.parse_factor()
            left = AstNode(AstNodeKind.BINOP, op, left, right, None)

    def parse_factor(self):
        tok = self.peek()
        if tok.kind == TokenKind.PLUS or tok.kind == TokenKind.MINUS:
            self.consume()
            op = Op.POS if tok.kind == TokenKind.PLUS else Op.NEG
            left = self.parse_atom()
            return AstNode(AstNodeKind.UNAOP, op, left, None, None)
        return self.parse_atom()

    def parse_atom(self):
        tok = self.peek()
        match tok.kind:
            case TokenKind.INTEGER:
                self.consume()
                return AstNode(AstNodeKind.INTEGER, None, None, None, int(tok.lexeme))
            case TokenKind.LPAREN:
                self.consume()
                left = self.parse_expr()
                tok = self.peek()
                if tok.kind != TokenKind.RPAREN:
                    self.diag.report(tok.offset, "unclosed parenthesis")
                    return None
                self.consume()
                return left
            case _:
                self.diag.report(tok.offset, "expect number or '('")
                return None

############
# Evaluator
############

def evaluate(root):
    if not root:
        return 0
    match root.kind:
        case AstNodeKind.INTEGER:
            return root.value
        case AstNodeKind.BINOP:
            lv = evaluate(root.left)
            rv = evaluate(root.right)
            match root.op:
                case Op.ADD:
                    return lv + rv
                case Op.SUB:
                    return lv - rv
                case Op.MUL:
                    return lv * rv
                case Op.DIV:
                    return lv / rv
        case AstNodeKind.UNAOP:
            v = evaluate(root.left)
            match root.op:
                case Op.POS:
                    return v
                case Op.NEG:
                    return -v
        case _:
            return 0 # unreachable

if __name__ == "__main__":
    while True:
        try:
            line = input("> ").strip()
        except EOFError:
            break

        if not line:
            continue

        lexer = Lexer(line)
        if not lexer.tokenize():
            continue
        # lexer.print()

        parser = Parser(line, lexer.tokens)
        if not parser.parse():
            continue

        result = evaluate(parser.root)
        print(f"result: {result}")
