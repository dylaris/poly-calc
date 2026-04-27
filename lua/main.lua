-------------
-- Global Var
-------------

local has_error = false
local input = nil

------------
-- Other
------------

local function iota(start)
    local n = start or 0
    return function()
        local cur = n
        n = n + 1
        return cur
    end
end

local function enum(...)
    local names = {...}
    local tbl = {}
    for i = 1, #names do
        tbl[names[i]] = i - 1
    end
    setmetatable(tbl, {
        __newindex = function() error("enum is readonly") end,
        __index = tbl,
    })
    return tbl
end

local function is_digit(c)
    if not c then return false end
    local b = c:byte()
    return b >= 48 and b <= 57
end

local function report_error(pos, msg)
    if has_error then return end
    io.stderr:write("error: " .. msg .. "\n")
    io.stderr:write("input: " .. input .. "\n")
    io.stderr:write("       " .. string.rep(" ", pos-1) .. "^\n")
    io.stderr:flush()
    has_error = true
end

------------
-- Data Type
------------

local TokenKind = enum(
    "Plus",
    "Minus",
    "Star",
    "Slash",
    "LParen",
    "RParen",
    "Integer",
    "EOF"
)

local Op = enum(
    "Add",
    "Sub",
    "Mul",
    "Div",
    "Pos",
    "Neg"
)

local AstNodeKind = enum(
    "Integer",
    "Unaop",
    "Binop"
)

------------
-- Lexer
------------

local Lexer = {}
Lexer.__index = Lexer

function Lexer.new(input)
    local obj = setmetatable({}, Lexer)
    obj.input = input or ""
    obj.tokens = {}
    return obj
end

local function make_token(kind, position, lexeme)
    return { kind = kind, position = position, lexeme = lexeme }
end

function Lexer:tokenize()
    local curr = 1
    while curr <= #self.input do
        local tok = nil
        local start = curr

        local ch = string.sub(self.input, curr, curr)
        while is_digit(ch) do
            curr = curr + 1
            if curr > #self.input then break end
            ch = string.sub(self.input, curr, curr)
        end
        if start ~= curr then
            tok = make_token(TokenKind.Integer, start, string.sub(self.input, start, curr - 1))
            goto append
        end

        curr = curr + 1
        if ch == '+' then
            tok = make_token(TokenKind.Plus,   start, ch)
        elseif ch == '-' then
            tok = make_token(TokenKind.Minus,  start, ch)
        elseif ch == '*' then
            tok = make_token(TokenKind.Star,   start, ch)
        elseif ch == '/' then
            tok = make_token(TokenKind.Slash,  start, ch)
        elseif ch == '(' then
            tok = make_token(TokenKind.LParen, start, ch)
        elseif ch == ')' then
            tok = make_token(TokenKind.RParen, start, ch)
        elseif ch == ' ' or ch == '\t' then
            goto continue
        else
            report_error(start, "unknown char")
            return
        end

::append::
        table.insert(self.tokens, tok)
::continue::
    end

    table.insert(self.tokens, make_token(TokenKind.EOF, curr, nil))
end

function Lexer:__tostring()
    local flat = {}
    for _, tok in ipairs(self.tokens) do
        if tok.lexeme then table.insert(flat, tok.lexeme) end
    end
    return table.concat(flat, "\n")
end

------------
-- Parser
------------

local function make_node(kind, op, left, right, value)
    return {
        kind = kind,
        op = op,
        left = left,
        right = right,
        value = value,
    }
end

local Parser = {}
Parser.__index = Parser

function Parser.new(toks)
    local obj = setmetatable({}, Parser)
    obj.tokens = toks
    obj.curr = 1
    obj.root = nil
    return obj
end

function Parser:peek()
    return self.tokens[self.curr]
end

function Parser:consume()
    self.curr = self.curr + 1
end

function Parser:parse()
    self.root = self:parse_expr()

    tok = self:peek()
    if tok.kind ~= TokenKind.EOF then
        report_error(tok.position, "unexpected token")
    end
end

function Parser:parse_expr()
    local left = self:parse_term()
    while true do
        local tok = self:peek()
        local op = nil
        if tok.kind == TokenKind.Plus then
            op = Op.Add
        elseif tok.kind == TokenKind.Minus then
            op = Op.Sub
        else
            goto out
        end
        self:consume()
        local right = self:parse_term()
        left = make_node(AstNodeKind.Binop, op, left, right, nil)
    end
::out::
    return left
end

function Parser:parse_term()
    local left = self:parse_factor()
    while true do
        local tok = self:peek()
        local op = nil
        if tok.kind == TokenKind.Star then
            op = Op.Mul
        elseif tok.kind == TokenKind.Slash then
            op = Op.Div
        else
            goto out
        end
        self:consume()
        local right = self:parse_factor()
        left = make_node(AstNodeKind.Binop, op, left, right, nil)
    end
::out::
    return left
end

function Parser:parse_factor()
    local tok = self:peek()
    if tok.kind == TokenKind.Plus or tok.kind == TokenKind.Minus then
        self:consume()
        local op = tok.kind == TokenKind.Plus and Op.Add or Op.Sub
        local node = self:parse_atom()
        return make_node(AstNodeKind.Unaop, op, node, nil, nil)
    end
    return self:parse_atom()
end

function Parser:parse_atom()
    local tok = self:peek()
    if tok.kind == TokenKind.Integer then
        self:consume()
        return make_node(AstNodeKind.Integer, nil, nil, nil, tonumber(tok.lexeme))
    elseif tok.kind == TokenKind.LParen then
        self:consume()
        local node = self:parse_expr()
        tok = self:peek()
        if tok.kind ~= TokenKind.RParen then
            report_error(tok.position, "unclosed parenthesis")
            return nil
        end
        self:consume()
        return node
    else
        report_error(tok.position, "expect number or '('")
        return nil
    end
end

------------
-- Evaluator
------------

local function evaluate(root)
    if not root then return 0 end
    if root.kind == AstNodeKind.Integer then
        return root.value
    elseif root.kind == AstNodeKind.Unaop then
        local val = evaluate(root.left)
        if root.op == Op.Neg then val = -val end
        return val
    elseif root.kind == AstNodeKind.Binop then
        local lv = evaluate(root.left)
        local rv = evaluate(root.right)
        if root.op == Op.Add then
            return lv + rv
        elseif root.op == Op.Sub then
            return lv - rv
        elseif root.op == Op.Mul then
            return lv * rv
        elseif root.op == Op.Div then
            return lv / rv
        end
    end
end

------------
-- main
------------

while true do
    io.write("> ")
    io.flush()

    has_error = false
    input = io.read()
    if input == "" then goto continue end
    if not input then break end

    local lexer = Lexer.new(input)
    lexer:tokenize()
    if has_error then goto continue end
    -- print(lexer)

    local parser = Parser.new(lexer.tokens)
    parser:parse()
    if has_error then goto continue end

    print("result: " .. evaluate(parser.root))
::continue::
end

