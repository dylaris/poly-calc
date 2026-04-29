////////////
// Diag
////////////

class Diag {
  construct new(input) {
    _input = input
    _hasError = false
  }

  report(offset, message) {
    if (_hasError) return
    System.print("error: %(message)")
    System.print("input: %(_input)")
    System.print("       " + " " * offset + "^")
    _hasError = true
  }
}

////////////
// Lexer
////////////

class TokenKind {
  static Integer { 0 }
  static Plus    { 1 }
  static Minus   { 2 }
  static Star    { 3 }
  static Slash   { 4 }
  static LParen  { 5 }
  static RParen  { 6 }
  static Eof     { 7 }
}

class Token {
  construct new(kind, offset, lexeme) {
    _kind = kind
    _offset = offset
    _lexeme = lexeme
  }

  kind { _kind }
  offset { _offset }
  lexeme { _lexeme }
}

class Lexer {
  construct new(input) {
    _input = input
    _diag = Diag.new(input)
    _tokens = []
  }

  tokens { _tokens }

  tokenize() {
    var length = _input.count
    var curr = 0

    while (curr < length) {
      var start = curr

      while (curr < length && isDigit(_input[curr])) curr = curr + 1
      if (start != curr) {
        _tokens.add(Token.new(TokenKind.Integer, start, _input[start...curr]))
        continue
      }

      var ch = _input[curr]
      var kind = null
      if (ch == "+") {
        kind = TokenKind.Plus
      } else if (ch == "-") {
        kind = TokenKind.Minus
      } else if (ch == "*") {
        kind = TokenKind.Star
      } else if (ch == "/") {
        kind = TokenKind.Slash
      } else if (ch == "(") {
        kind = TokenKind.LParen
      } else if (ch == ")") {
        kind = TokenKind.RParen
      } else if (ch == " " || ch == "\t") {
        curr = curr + 1
        continue
      } else {
        _diag.report(start, "unknown char: %(ch)")
        return false
      }
      _tokens.add(Token.new(kind, start, ch))
      curr = curr + 1
    }
    _tokens.add(Token.new(TokenKind.Eof, curr, null))

    return true
  }

  isDigit(ch) {
    var a = ch.bytes[0]
    var b = "0".bytes[0]
    if (a - b >= 0 && a - b <= 9) return true
    return false
  }

  print() {
    for (tok in _tokens) {
      if (tok.kind != TokenKind.Eof) {
        System.print(tok.lexeme)
      }
    }
  }
}

////////////
// Parser
////////////

class Op {
  static Add { 0 }
  static Sub { 1 }
  static Mul { 2 }
  static Div { 3 }
  static Pos { 4 }
  static Neg { 5 }
}

class AstNodeKind {
  static Integer { 0 }
  static Binop   { 1 }
  static Unaop   { 2 }
}

class AstNode {
  construct new(kind, op, left, right) {
    _kind = kind
    _op = op
    _left = left
    _right = right
  }

  construct new(kind, op, left) {
    _kind = kind
    _op = op
    _left = left
  }

  construct new(kind, value) {
    _kind = kind
    _value = value
  }

  kind { _kind }
  op { _op }
  left { _left }
  right { _right }
  value { _value }
}

class Parser {
  construct new(input, tokens) {
    _diag = Diag.new(input)
    _tokens = tokens
    _curr = 0
    _root = null
  }

  root { _root }

  peek() { _tokens[_curr] }
  consume() { _curr = _curr + 1 }

  parse() {
    _root = parseExpr()
    var tok = peek()
    if (tok.kind != TokenKind.Eof) {
      _diag.report(tok.offset, "unexpected token")
      return false
    }
    return true
  }

  parseExpr() {
    var left = parseTerm()
    while (true) {
      var op = null
      var tok = peek()
      if (tok.kind == TokenKind.Plus) {
        op = Op.Add
      } else if (tok.kind == TokenKind.Minus) {
        op = Op.Sub
      } else {
        return left
      }
      consume()
      var right = parseTerm()
      left = AstNode.new(AstNodeKind.Binop, op, left, right)
    }
  }

  parseTerm() {
    var left = parseFactor()
    while (true) {
      var op = null
      var tok = peek()
      if (tok.kind == TokenKind.Star) {
        op = Op.Mul
      } else if (tok.kind == TokenKind.Slash) {
        op = Op.Div
      } else {
        return left
      }
      consume()
      var right = parseFactor()
      left = AstNode.new(AstNodeKind.Binop, op, left, right)
    }
  }

  parseFactor() {
    var tok = peek()
    if (tok.kind == TokenKind.Plus || tok.kind == TokenKind.Minus) {
      consume()
      var op = tok.kind == TokenKind.Plus ? Op.Pos : Op.Neg
      var left = parseAtom()
      return AstNode.new(AstNodeKind.Unaop, op, left)
    }
    return parseAtom()
  }

  parseAtom() {
    var tok = peek()
    if (tok.kind == TokenKind.Integer) {
      consume()
      return AstNode.new(AstNodeKind.Integer, Num.fromString(tok.lexeme))
    } else if (tok.kind == TokenKind.LParen) {
      consume()
      var left = parseExpr()
      tok = peek()
      if (tok.kind != TokenKind.RParen) {
        _diag.report(tok.offset, "unclosed parenthesis")
        return null
      }
      consume()
      return left
    } else {
      _diag.report(tok.offset, "expect number or '('")
      return null
    }
  }
}

////////////
// Evaluator
////////////

var evaluate = null
evaluate = Fn.new{|root|
  if (root == null) return 0
  if (root.kind == AstNodeKind.Integer) {
    return root.value
  } else if (root.kind == AstNodeKind.Unaop) {
    if (root.op == Op.Pos) {
      return root.value
    } else if (root.op = Op.Neg) {
      return -root.value
    } else {
      Fiber.abort("unreachable")
    }
  } else if (root.kind == AstNodeKind.Binop) {
    var lv = evaluate.call(root.left)
    var rv = evaluate.call(root.right)
    if (root.op == Op.Add) {
      return lv + rv
    } else if (root.op == Op.Sub) {
      return lv - rv
    } else if (root.op == Op.Mul) {
      return lv * rv
    } else if (root.op == Op.Div) {
      return lv / rv
    } else {
      Fiber.abort("unreachable")
    }
  } else {
    Fiber.abort("unreachable")
  }
}

////////////
// main
////////////

var input = "123+ 3*(3- 4)/5)"
var lexer = Lexer.new(input)

lexer.tokenize()
lexer.print()

var parser = Parser.new(input, lexer.tokens)
parser.parse()

var result = evaluate.call(parser.root)
System.print("result: %(result)")
