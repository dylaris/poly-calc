package main

import (
	"bufio"
	"fmt"
	"os"
	"strings"
	"strconv"
)

/////////////////
// Global
/////////////////

var (
	Input = "128 + 4*(2-5)/6 - 1"
	HasError = false
)

/////////////////
// Other
/////////////////

func Report(offset int, message string) {
	if !HasError {
		fmt.Fprintln(os.Stderr, "error: " + message)
		fmt.Fprintln(os.Stderr, "input: " + Input)
		fmt.Fprintln(os.Stderr, "       " + strings.Repeat(" ", offset) + "^")
		HasError = true
	}
}

func IsDigit(c byte) bool {
	return c >= '0' && c <= '9'
}

/////////////////
// Lexer
/////////////////

type TokenKind int

const (
	TKEof TokenKind     = 0
	TKInteger TokenKind = 1
	TKPlus TokenKind    = '+'
	TKMinus TokenKind   = '-'
	TKStar TokenKind    = '*'
	TKSlash TokenKind   = '/'
	TKLParen TokenKind  = '('
	TKRParen TokenKind  = ')'
)

type Token struct {
	Kind TokenKind
	Lexeme string
	Offset int
}

type Lexer struct {
	Tokens []Token
}

func (l *Lexer) Tokenize() {
	curr := 0
	length := len(Input)

	for curr < length {
		start := curr

		// Identify number
		for curr < length && IsDigit(Input[curr]) {
			curr++
		}
		if start != curr {
			token := Token{Kind: TKInteger, Lexeme: Input[start:curr], Offset: start}
			l.Tokens = append(l.Tokens, token)
			continue
		}

		// Identify operator
		ch := Input[curr]
		curr++
		switch ch {
		case '+', '-', '*', '/', '(', ')':
			token := Token{Kind: TokenKind(ch), Lexeme: Input[start:curr], Offset: start}
			l.Tokens = append(l.Tokens, token)
		case '\n', '\r', '\t', ' ':
			// Ignore
		default:
			Report(start, "unknown char: " + string(ch))
			return
		}
	}

	l.Tokens = append(l.Tokens, Token{Kind: TKEof})
}

func (l Lexer) Print() {
	for _, token := range l.Tokens {
		if token.Kind != TKEof {
			fmt.Println(token.Lexeme)
		}
	}
}

/////////////////
// Parser
/////////////////

type ASTNodeKind int

const (
	NKInteger ASTNodeKind  = iota
	NKBinaryOP
	NKUnaryOP
)

type OPKind int

const (
	OKAdd OPKind = iota
	OKSub
	OKMul
	OKDiv
	OKPos
	OKNeg
)

type ASTNode struct {
	Kind ASTNodeKind
	OP OPKind
	Left *ASTNode
	Right *ASTNode
	Value int
}

type Parser struct {
	Root *ASTNode
	tokens []Token
	curr int
}

func (p *Parser) Peek() Token {
	return p.tokens[p.curr]
}

func (p *Parser) Consume() {
	p.curr++
}

func (p *Parser) Parse(tokens []Token) {
	p.tokens = tokens
	p.curr = 0

	p.Root = p.ParseExpr()
	token := p.Peek()
	if token.Kind != TKEof {
		Report(token.Offset, "unexpected token")
	}
}

func (p *Parser) ParseExpr() *ASTNode {
	left := p.ParseTerm()
	for {
		token := p.Peek()
		var op OPKind
		switch token.Kind {
		case TKPlus:
			op = OKAdd
		case TKMinus:
			op = OKSub
		default:
			return left
		}
		p.Consume()
		right := p.ParseTerm()
		left = &ASTNode{Kind: NKBinaryOP, OP: op, Left: left, Right: right}
	}
}

func (p *Parser) ParseTerm() *ASTNode {
	left := p.ParseFactor()
	for {
		token := p.Peek()
		var op OPKind
		switch token.Kind {
		case TKStar:
			op = OKMul
		case TKSlash:
			op = OKDiv
		default:
			return left
		}
		p.Consume()
		right := p.ParseFactor()
		left = &ASTNode{Kind: NKBinaryOP, OP: op, Left: left, Right: right}
	}
}

func (p *Parser) ParseFactor() *ASTNode {
	token := p.Peek()
	if token.Kind == TKPlus || token.Kind == TKMinus {
		p.Consume()
		var op OPKind
		if token.Kind == TKPlus {
			op = OKPos
		} else {
			op = OKNeg
		}
		left := p.ParseAtom()
		return &ASTNode{Kind: NKUnaryOP, OP: op, Left: left}
	}
	return p.ParseAtom()
}

func (p *Parser) ParseAtom() *ASTNode {
	token := p.Peek()
	switch token.Kind {
	case TKInteger:
		p.Consume()
		value, _ := strconv.Atoi(token.Lexeme)
		return &ASTNode{Kind: NKInteger, Value: value}
	case TKLParen:
		p.Consume()
		left := p.ParseExpr()
		token = p.Peek()
		if token.Kind != TKRParen {
			Report(token.Offset, "unclosed parenthesis")
			return &ASTNode{}
		}
		p.Consume()
		return left
	default:
		Report(token.Offset, "expect number or '('")
		return &ASTNode{}
	}
}

/////////////////
// Eval
/////////////////

func Evaluate(root *ASTNode) float32 {
	if root == nil {
		return 0
	}
	switch root.Kind {
	case NKInteger:
		return float32(root.Value)
	case NKBinaryOP:
		lv := Evaluate(root.Left)
		rv := Evaluate(root.Right)
		switch root.OP {
		case OKAdd:
			return lv + rv
		case OKSub:
			return lv - rv
		case OKMul:
			return lv * rv
		case OKDiv:
			return lv / rv
		}
	case NKUnaryOP:
		v := Evaluate(root.Left)
		switch root.OP {
		case OKPos:
			return v
		case OKNeg:
			return -v
		}
	}
	return 0
}

func main() {
	scanner := bufio.NewScanner(os.Stdin)

	for {
		fmt.Print(">>> ")
		if !scanner.Scan() {
			break
		}

		Input = strings.TrimSpace(scanner.Text())
		if Input == "" {
			continue
		}
		if Input == "exit" || Input == "quit" {
			break
		}

		HasError = false

		var lexer Lexer
		lexer.Tokenize()
		// lexer.Print()
		if HasError {
			continue
		}

		var parser Parser
		parser.Parse(lexer.Tokens)
		if HasError {
			continue
		}

		v := Evaluate(parser.Root)
		fmt.Printf("result: %g\n", v)
	}
}

