#include "9cc.h"

static char *user_input;

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

    if(*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '<' || *p == '>') {
      cur = new_token(TK_RESERVED, cur, p++, 1);
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
