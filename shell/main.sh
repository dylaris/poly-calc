#!/bin/bash

set -eu

##########
# Global
##########

input="128b+ (3 -5)a/4*5+1"

declare -i has_error=0
declare -a tokens=()
declare -A root=()

repeat() {
  local str=$1
  local count=$2
  printf "$str%.0s" $(seq 1 $count)
}

report() {
  if [ $has_error == 1 ]; then
    return
  fi
  local offset=$1
  local message="$2"
  echo "error: ${message}" >&2
  echo "input: $input" >&2
  local indent="$(repeat " " ${offset})"
  echo "       ${indent}^" >&2
  has_error=1
}

##########
# Lexer
##########

readonly TOK_PLUS=0
readonly TOK_MINUS=1
readonly TOK_STAR=2
readonly TOK_SLASH=3
readonly TOK_LPAREN=4
readonly TOK_RPAREN=5
readonly TOK_INTEGER=6
readonly TOK_EOF=7

isdigit() {
 [[ $1 =~ ^[0-9]$ ]]
}

add_token() {
  local -A tok=()
  tok["kind"]=$1
  tok["lexeme"]=$2
  tok["offset"]=$3
  tokens+=("$(declare -p tok)")
}

tokenize() {
  local input=$1
  local len=${#input}
  local curr=0
  while [ $curr -lt $len ]; do
    local start=$curr
    while [ $curr -lt $len ] && isdigit "${input:$curr:1}" ]; do
      curr=$((curr + 1))
    done
    if [ $curr -ne $start ]; then
      local toklen=$((curr - start))
      add_token $TOK_INTEGER "${input:$start:$toklen}" $start
      continue
    fi

    local ch="${input:$start:1}"
    case "$ch" in
      " "|"\r"|"\t"|"\n")
        ;;
      "+")
        add_token $TOK_PLUS "$ch" $start
        ;;
      "-")
        add_token $TOK_MINUS "$ch" $start
        ;;
      "*")
        add_token $TOK_STAR "$ch" $start
        ;;
      "/")
        add_token $TOK_SLASH "$ch" $start
        ;;
      "(")
        add_token $TOK_LPAREN "$ch" $start
        ;;
      ")")
        add_token $TOK_RPAREN "$ch" $start
        ;;
      *)
        report $start "unknown character"
        break
        ;;
    esac
    curr=$((curr + 1))
  done
  add_token $TOK_EOF "eof" $curr
}

print_tokens() {
  for tokstr in "${tokens[@]}"; do
    eval "$tokstr"
    echo "kind: ${tok["kind"]}; offset: ${tok["offset"]}; lexeme: ${tok["lexeme"]}"
  done
}

##########
# Parser
##########

readonly OP_ADD=0
readonly OP_SUB=1
readonly OP_MUL=2
readonly OP_DIV=3
readonly OP_NEG=4
readonly OP_POS=5

readonly AST_INTEGER=0
readonly AST_BINOP=1
readonly AST_UNAOP=2

##########
# Eval
##########

##########
# main
##########

tokenize "$input"
print_tokens
