#ifndef mtots_lexer_h
#define mtots_lexer_h

#include "mtots_common.h"
#include "mtots_util_string.h"

typedef enum TokenType {
  TOKEN_ERROR,

  /* Single-character tokens. */
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET,
  TOKEN_RIGHT_BRACKET,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_MINUS,
  TOKEN_PERCENT,
  TOKEN_PLUS,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_SLASH,
  TOKEN_STAR,
  TOKEN_AT,
  TOKEN_PIPE,
  TOKEN_AMPERSAND,
  TOKEN_CARET,
  TOKEN_TILDE,
  TOKEN_QMARK,
  TOKEN_QMARK_QMARK,
  TOKEN_SHIFT_LEFT,
  TOKEN_SHIFT_RIGHT,
  /* One or two character tokens. */
  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,
  TOKEN_SLASH_SLASH,
  TOKEN_STAR_STAR,
  /* Literals. */
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_RAW_STRING,
  TOKEN_NUMBER,
  TOKEN_NUMBER_HEX,
  TOKEN_NUMBER_BIN,
  /* Keywords. */
  TOKEN_AND,
  TOKEN_CLASS,
  TOKEN_DEF,
  TOKEN_ELIF,
  TOKEN_ELSE,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_IF,
  TOKEN_NIL,
  TOKEN_OR,
  TOKEN_RETURN,
  TOKEN_SUPER,
  TOKEN_THIS,
  TOKEN_TRUE,
  TOKEN_VAR,
  TOKEN_WHILE,

  /* Additional Keywords */
  TOKEN_AS,
  TOKEN_ASSERT,
  TOKEN_ASYNC,
  TOKEN_AWAIT,
  TOKEN_BREAK,
  TOKEN_CONTINUE,
  TOKEN_DEL,
  TOKEN_EXCEPT,
  TOKEN_FINAL,
  TOKEN_FINALLY,
  TOKEN_FROM,
  TOKEN_GLOBAL,
  TOKEN_IMPORT,
  TOKEN_IN,
  TOKEN_IS,
  TOKEN_LAMBDA,
  TOKEN_NOT,
  TOKEN_PASS,
  TOKEN_RAISE,
  TOKEN_STATIC,
  TOKEN_THEN,
  TOKEN_TRAIT,
  TOKEN_TRY,
  TOKEN_WITH,
  TOKEN_YIELD,

  /* Whitespace Tokens */
  TOKEN_NEWLINE,
  TOKEN_INDENT,
  TOKEN_DEDENT,

  TOKEN_EOF
} TokenType;

typedef struct Token {
  TokenType type;
  const char *start;
  size_t length;
  int line;
} Token;

typedef struct Lexer {
  const char *start;
  const char *current;
  int line;             /* The current line number, indexed from one */
  int currentIndent;    /* the depth of the current indentation */
  int indentTokenQueue; /* INDENT (+) and DEDENT (-) tokens that are waiting to be emitted */
  int groupingDepth;    /* how deeply we are currently nested in one of the grouping symbols */
  ubool fakeFinalNewlineEmitted;
} Lexer;

void initLexer(Lexer *lexer, const char *source);
Status lexerNext(Lexer *lexer, Token *token);
const char *tokenTypeToName(TokenType type);

#endif /*mtots_lexer_h*/
