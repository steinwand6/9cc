#include "9cc.h"

// コンパイルする文字列
static char *user_input;
// 現在着目しているトークン
static Token *token;

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, " ");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

bool at_eof() {
  return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

bool check_multibytes_op(char *str, char *op) {
  int i, len = 0;
  bool result = false;
  for(;;) {
    if(op[len] == '\0')
      break;
    len++;
  }

  for(i = 0; i < len; i++) {
    result = str[i] == op[i];
    if(!result)
      return result;
  }
  return result;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p) {
  user_input = p;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while(*p) {
    // 空白文字をスキップ
    if(isspace(*p)) {
      p++;
      continue;
    }

    if(check_multibytes_op(p, "==") || check_multibytes_op(p, "!=") || 
        check_multibytes_op(p, ">=") || check_multibytes_op(p, "<=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p = p + 2;  // 2文字分進める
      continue;
    }

    if(*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '<' || *p == '>' || *p == ';' || *p == '=') {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if(*p >= 'a' && *p <= 'z') {
      cur = new_token(TK_IDENT, cur, p++, 1);
      continue;
    }

    if(isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      cur->val = strtol(p, &p, 10);
      continue;
    }
    error_at(p, "トークナイズできません");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}


// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op) {
  if(token->kind != TK_RESERVED || token->len != strlen(op) | memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *op) {
  if(token->kind != TK_RESERVED || token->len != strlen(op) | memcmp(token->str, op, token->len))
    error_at(token->str, "'%c'ではありません", op);
  token = token->next;
}

// 次のトークンが数値のときには、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
  if(token->kind != TK_NUM)
    error_at(token->str, "数ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

Node *expr();

Node *primary() {
  Node *node = calloc(1, sizeof(Node));
  if(consume("(")) {
    node = expr();
    expect(")");
    return node;
  }

  if(token->kind == TK_IDENT) {
    node->kind = ND_LVAR;
    node->offset = (token->str[0] - 'a' + 1) * 8; // 一旦すべての変数(a〜z)の確保する位置は決め打ち
    token = token->next;
    return node;
  }

  return new_node_num(expect_number());
}

Node *unary() {
  if(consume("+")) {
    return primary();
  } else if(consume("-")) {
    return new_node(ND_SUB, new_node_num(0), primary());
  }
  return primary();
}

Node *mul() {
  Node *node = unary();
  for(;;) {
    if(consume("*")) {
      node = new_node(ND_MUL, node, unary());
    } else if(consume("/")) {
      node = new_node(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

Node *add() {
  Node *node = mul();
  for(;;) {
    if(consume("+")) {
      node = new_node(ND_ADD, node, mul());
    } else if(consume("-")) {
      node = new_node(ND_SUB, node, mul());
    } else {
      return node;
    }
  }
}

Node *relational() {
  Node *node = add();
  for(;;) {
    if(consume("<")) {
      node = new_node(ND_LES, node, add());
    } else if(consume("<=")) {
      node = new_node(ND_LTE, node, add());
    } else if(consume(">")) {
      node = new_node(ND_LES, add(), node);
    } else if(consume(">=")) {
      node = new_node(ND_LTE, add(), node);
    } else {
      return node;
    }
  }
}

Node *equality() {
  Node *node = relational();
  for(;;) {
    if(consume("==")) {
      node = new_node(ND_EQL, node, relational());
    } else if(consume("!=")) {
      node = new_node(ND_NEQ, node, relational());
    } else {
      return node;
    }
  }
}

Node *assign() {
  Node *node = equality();
  for(;;) {
    if(consume("=")) {
      node = new_node(ND_ASSIGN, node, assign());
    }
    return node;
  }
}

Node *expr() {
  return assign();
}

Node *stmt() {
  Node *node = expr();
  expect(";");
  return node;
}

Node *program(Token *input_token) {
  token = input_token;
  return stmt();
}