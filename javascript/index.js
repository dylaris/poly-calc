//////////////////
// Diagnostics
//////////////////

class Diagnostics {
  constructor(input) {
    this.input = input;
    this.hasError = false;
  }

  report(offset, message) {
    if (this.hasError) {
      return;
    }
    console.log(`error: ${message}`);
    console.log(`input: ${this.input}`);
    console.log("       " + " " * offset + "^");
    this.hasError = true;
  }
}

//////////////////
// Lexer
//////////////////

const TokenKind = {
  INTEGER: 0,
  PLUS: 1,
  MINUS: 2,
  STAR: 3,
  SLASH: 4,
  LPAREN: 5,
  RPAREN: 6,
  EOF: 7
}

class Token {
  constructor(kind, offset, lexeme) {
    this.kind = kind;
    this.offset = offset;
    this.lexeme = lexeme;
  }
}

class Lexer {
  constructor(input) {
    this.input = input;
    this.diag = new Diagnostics(input);
    this.tokens = [];
  }

  tokenize() {
    let curr = 0;
    const length = this.input.length;
    const isDigit = (c) => c >= '0' && c <= '9';

    while (curr < length) {
      let start = curr;
      while (curr < length && isDigit(this.input[curr])) {
        curr++;
      }
      if (start != curr) {
        this.tokens.push(new Token(TokenKind.INTEGER, start, this.input.slice(start, curr)));
        continue;
      }
      const ch = this.input[curr++];
      switch (ch) {
        case '+':
          this.tokens.push(new Token(TokenKind.PLUS, start, ch));
          break;
        case '-':
          this.tokens.push(new Token(TokenKind.MINUS, start, ch));
          break;
        case '*':
          this.tokens.push(new Token(TokenKind.STAR, start, ch));
          break;
        case '/':
          this.tokens.push(new Token(TokenKind.SLASH, start, ch));
          break;
        case '(':
          this.tokens.push(new Token(TokenKind.LPAREN, start, ch));
          break;
        case ')':
          this.tokens.push(new Token(TokenKind.RPAREN, start, ch));
          break;
        case ' ':
        case '\t':
          break;
        default:
          this.diag.report(start, `unknown char: ${ch}`);
          return false;
      }
    }
    this.tokens.push(new Token(TokenKind.EOF, curr, null));
    return true;
  }

  print() {
    this.tokens.forEach((tok, _) => {
      console.log(tok.lexeme);
    });
  }
}

//////////////////
// Parser
//////////////////

const AstNodeKind = {
  INTEGER: 0,
  BINOP: 1,
  UNAOP: 2,
}

const Op = {
  ADD: 0,
  SUB: 1,
  MUL: 2,
  DIV: 3,
  POS: 4,
  NEG: 5,
}

class AstNode {
  constructor(kind, op, left, right, value) {
    this.kind  = kind;
    this.op    = op;
    this.left  = left;
    this.right = right;
    this.value = value;
  }
}

class Parser {
  constructor(input, tokens) {
    this.input  = input;
    this.tokens = tokens;
    this.root   = null;
    this.curr   = 0;
    this.diag   = new Diagnostics(input);
  }

  peek() {
    return this.tokens[this.curr];
  }

  consume() {
    this.curr++;
  }

  parse() {
    this.root = this.parse_expr();
    const tok = this.peek();
    if (tok.kind != TokenKind.EOF) {
      this.diag.report(tok.offset, "unexpected token");
    }
    if (this.diag.hasError) {
      return false;
    }
    return true;
  }

  parse_expr() {
    let left = this.parse_term();
    while (true) {
      let tok = this.peek();
      let op = null;
      switch (tok.kind) {
        case TokenKind.PLUS:
          op = Op.ADD;
          break;
        case TokenKind.MINUS:
          op = Op.SUB;
          break;
        default:
          return left;
      }
      this.consume();
      let right = this.parse_term();
      left = new AstNode(AstNodeKind.BINOP, op, left, right, null);
    }
  }

  parse_term() {
    let left = this.parse_factor();
    while (true) {
      let tok = this.peek();
      let op = null;
      switch (tok.kind) {
        case TokenKind.STAR:
          op = Op.MUL;
          break;
        case TokenKind.SLASH:
          op = Op.DIV;
          break;
        default:
          return left;
      }
      this.consume();
      let right = this.parse_factor();
      left = new AstNode(AstNodeKind.BINOP, op, left, right, null);
    }
  }

  parse_factor() {
    const tok = this.peek();
    if (tok.kind == TokenKind.PLUS || tok.kind == TokenKind.MINUS) {
      this.consume();
      let op = tok.kind == TokenKind.PLUS ? Op.POS : Op.NEG;
      let left = this.parse_atom();
      return new AstNode(AstNodeKind.UNAOP, op, left, null, null);
    }
    return this.parse_atom();
  }

  parse_atom() {
    let tok = this.peek();
    switch (tok.kind) {
      case TokenKind.INTEGER:
        this.consume();
        return new AstNode(AstNodeKind.INTEGER, null, null, null, Number(tok.lexeme));
      case TokenKind.LPAREN:
        this.consume();
        let left = this.parse_expr();
        tok = this.peek();
        if (tok.kind != TokenKind.RPAREN) {
          this.diag.report(tok.offset, "unclosed parenthesis");
          return null;
        }
        this.consume();
        return left;
      default:
        this.diag.report(tok.offset, "expect number or '('");
        return null;
    }
  }
}

//////////////////
// Evaluator
//////////////////
function evaluate(root) {
  if (!root) {
    return 0;
  }
  switch (root.kind) {
    case AstNodeKind.INTEGER:
      return root.value;
    case AstNodeKind.BINOP:
      let lv = evaluate(root.left);
      let rv = evaluate(root.right);
      switch (root.op) {
        case Op.ADD:
          return lv + rv;
        case Op.SUB:
          return lv - rv;
        case Op.MUL:
          return lv * rv;
        case Op.DIV:
          return lv / rv;
      }
    case AstNodeKind.UNAOP:
      let v = evaluate(root.left);
      return root.op == Op.NEG ? -v : v;
    default:
      return 0; // unreachable
  }
}

//////////////////
// main
//////////////////

while (true) {
  const expr = prompt("input: ", "123-8*2/(2-4)");
  if (expr) {
      let lexer = new Lexer(expr);
      lexer.tokenize();
      // lexer.print();
      let parser = new Parser(expr, lexer.tokens);
      parser.parse();
      const result = evaluate(parser.root);
      console.log(`result: ${result}`);
      alert(`result: ${result}`);
  }
}
