#include "mtots_lexer.h"

#include <stdio.h>
#include <string.h>

#include "mtots_util_error.h"

#define PEEK (lexer->current[0])
#define PEEK1 (lexer->current[1])
#define PEEK2 (lexer->current[2])

typedef struct KeywordInfo {
  TokenType type;
  const char *name;
  u32 len;
} KeywordInfo;

typedef struct OneCharSymbolInfo {
  TokenType type;
  char first;
} OneCharSymbolInfo;

typedef struct TwoCharSymbolInfo {
  TokenType type;
  char first;
  char second;
} TwoCharSymbolInfo;

static OneCharSymbolInfo oneCharSymbols[] = {
    /* grouping tokens */
    {TOKEN_LEFT_PAREN, '('},
    {TOKEN_RIGHT_PAREN, ')'},
    {TOKEN_LEFT_BRACKET, '['},
    {TOKEN_RIGHT_BRACKET, ']'},
    {TOKEN_LEFT_BRACE, '{'},
    {TOKEN_RIGHT_BRACE, '}'},

    /* other single character tokens */
    {TOKEN_COLON, ':'},
    {TOKEN_SEMICOLON, ';'},
    {TOKEN_COMMA, ','},
    {TOKEN_DOT, '.'},
    {TOKEN_MINUS, '-'},
    {TOKEN_PLUS, '+'},
    {TOKEN_SLASH, '/'},
    {TOKEN_PERCENT, '%'},
    {TOKEN_STAR, '*'},
    {TOKEN_AT, '@'},
    {TOKEN_PIPE, '|'},
    {TOKEN_AMPERSAND, '&'},
    {TOKEN_CARET, '^'},
    {TOKEN_TILDE, '~'},
    {TOKEN_QMARK, '?'},
    {TOKEN_BANG, '!'},
    {TOKEN_EQUAL, '='},
    {TOKEN_LESS, '<'},
    {TOKEN_GREATER, '>'},
};

static TwoCharSymbolInfo twoCharSymbols[] = {
    {TOKEN_BANG_EQUAL, '!', '='},
    {TOKEN_EQUAL_EQUAL, '=', '='},
    {TOKEN_GREATER_EQUAL, '>', '='},
    {TOKEN_LESS_EQUAL, '<', '='},
    {TOKEN_SHIFT_LEFT, '<', '<'},
    {TOKEN_SHIFT_RIGHT, '>', '>'},
    {TOKEN_SLASH_SLASH, '/', '/'},
    {TOKEN_STAR_STAR, '*', '*'},
};

static KeywordInfo keywords[] = {
    {TOKEN_AND, "and", 3},
    {TOKEN_AS, "as", 2},
    {TOKEN_ASSERT, "assert", 6},
    {TOKEN_ASYNC, "async", 5},
    {TOKEN_AWAIT, "await", 5},
    {TOKEN_BREAK, "break", 5},
    {TOKEN_CLASS, "class", 5},
    {TOKEN_CONTINUE, "continue", 8},
    {TOKEN_DEF, "def", 3},
    {TOKEN_DEL, "del", 3},
    {TOKEN_ELIF, "elif", 4},
    {TOKEN_ELSE, "else", 4},
    {TOKEN_EXCEPT, "except", 6},
    {TOKEN_FALSE, "false", 5},
    {TOKEN_FINAL, "final", 5},
    {TOKEN_FINALLY, "finally", 7},
    {TOKEN_FOR, "for", 3},
    {TOKEN_FROM, "from", 4},
    {TOKEN_GLOBAL, "global", 6},
    {TOKEN_IF, "if", 2},
    {TOKEN_IMPORT, "import", 6},
    {TOKEN_IN, "in", 2},
    {TOKEN_IS, "is", 2},
    {TOKEN_LAMBDA, "lambda", 6},
    {TOKEN_NIL, "nil", 3},
    {TOKEN_NOT, "not", 3},
    {TOKEN_OR, "or", 2},
    {TOKEN_PASS, "pass", 4},
    {TOKEN_RAISE, "raise", 5},
    {TOKEN_RETURN, "return", 6},
    {TOKEN_STATIC, "static", 6},
    {TOKEN_SUPER, "super", 5},
    {TOKEN_THEN, "then", 4},
    {TOKEN_THIS, "this", 4},
    {TOKEN_TRAIT, "trait", 5},
    {TOKEN_TRUE, "true", 4},
    {TOKEN_TRY, "try", 3},
    {TOKEN_VAR, "var", 3},
    {TOKEN_WHILE, "while", 5},
    {TOKEN_WITH, "with", 4},
    {TOKEN_YIELD, "yield", 5},
};

static void skipSpacesAndComments(Lexer *lexer) {
  for (;;) {
    switch (PEEK) {
      case ' ':
      case '\r':
      case '\t':
        lexer->current++;
        break;
      case '\n':
        if (lexer->groupingDepth == 0) {
          return;
        }
        lexer->line++;
        lexer->current++;
        break;
      case '#':
        while (PEEK != '\n' && PEEK != '\0') {
          lexer->current++;
        }
        break;
      default:
        return;
    }
  }
}

static ubool isHexDigit(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

static ubool isDigit(char ch) {
  return ch >= '0' && ch <= '9';
}

static ubool isUnderAlpha(char ch) {
  return ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static ubool isUnderAlnum(char ch) {
  return isDigit(ch) || isUnderAlpha(ch);
}

void initLexer(Lexer *lexer, const char *source) {
  lexer->start = lexer->current = source;
  lexer->line = 1;
  lexer->indentTokenQueue = 0;
  lexer->currentIndent = 0;
  lexer->groupingDepth = 0;
  lexer->fakeFinalNewlineEmitted = UFALSE;
}

ubool lexerNext(Lexer *lexer, Token *token) {
  char first, second;

  if (lexer->indentTokenQueue > 0) {
    lexer->indentTokenQueue--;
    token->start = lexer->current;
    token->line = lexer->line;
    token->type = TOKEN_INDENT;
    token->length = 0;
    return UTRUE;
  } else if (lexer->indentTokenQueue < 0) {
    lexer->indentTokenQueue++;
    token->start = lexer->current;
    token->line = lexer->line;
    token->type = TOKEN_DEDENT;
    token->length = 0;
    return UTRUE;
  }

  skipSpacesAndComments(lexer);

  first = PEEK;
  token->start = lexer->current;
  token->line = lexer->line;

  if (first == '\0') {
    token->length = 0;
    if (lexer->currentIndent > 0) {
      lexer->currentIndent--;
      token->type = TOKEN_DEDENT;
    } else if (!lexer->fakeFinalNewlineEmitted) {
      lexer->fakeFinalNewlineEmitted = UTRUE;
      token->type = TOKEN_NEWLINE;
    } else {
      token->type = TOKEN_EOF;
    }
    return UTRUE;
  }

  second = PEEK1;

  if (first == '\n') {
    lexer->current++;
    lexer->line++;
    token->type = TOKEN_NEWLINE;
    token->length = 1;

    /* Skip empty lines and update indentation information */
    {
      const char *lineStart = lexer->current, *p = lexer->current;
      int newIndent;
      for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r') {
          p++;
        }
        if (*p == '\0') {
          /* If we hit EOF, we will handle this later anyway.
           * Just jump to the end and don't do anything else for now. */
          lexer->current = p;
          return UTRUE;
        } else if (*p == '\n') {
          lineStart = lexer->current = ++p;
          lexer->line++;
        } else {
          lexer->current = p;
          break;
        }
      }
      newIndent = (int)(lexer->current - lineStart);
      if (newIndent % 2 != 0) {
        runtimeError(
            "Indentations must always be an even number of spaces, but got %d",
            newIndent);
        return UFALSE;
      }
      newIndent /= 2;
      lexer->indentTokenQueue = newIndent - lexer->currentIndent;
      if (lexer->indentTokenQueue > 1) {
        runtimeError(
            "One level of indentation must be exactly 2 spaces, but got %d on line %d",
            (newIndent - lexer->currentIndent) * 2,
            lexer->line);
        return UFALSE;
      }
      lexer->currentIndent = newIndent;
    }
    return UTRUE;
  }

  /* raw string literals */
  if (first == 'r' && (second == '"' || second == '\'')) {
    ubool triple = UFALSE;
    lexer->current += 2;
    triple = PEEK == second && PEEK1 == second;
    if (triple) {
      lexer->current += 2;
    }

    while (PEEK != '\0' &&
           !(triple ? PEEK == second && PEEK1 == second && PEEK2 == second : PEEK == second)) {
      if (PEEK == '\n') {
        lexer->line++;
      }
      lexer->current++;
    }

    if (PEEK == '\0') {
      runtimeError("Unterminated (raw) string literal starting on line %d", token->line);
      return UFALSE;
    }
    lexer->current += triple ? 3 : 1;

    token->type = TOKEN_RAW_STRING;
    token->length = lexer->current - token->start;
    return UTRUE;
  }

  /* string literals */
  if (first == '"' || first == '\'') {
    ubool triple = UFALSE;
    lexer->current++;
    triple = PEEK == first && PEEK1 == first;
    if (triple) {
      lexer->current += 2;
    }

    while (PEEK != '\0' &&
           !(triple ? PEEK == first && PEEK1 == first && PEEK2 == first : PEEK == first)) {
      if (PEEK == '\n') {
        lexer->line++;
      } else if (PEEK == '\\' && PEEK1 == first) {
        lexer->current++;
      }
      lexer->current++;
    }

    if (PEEK == '\0') {
      runtimeError("Unterminated string literal starting on line %d", token->line);
      return UFALSE;
    }
    lexer->current += triple ? 3 : 1;

    token->type = TOKEN_STRING;
    token->length = lexer->current - token->start;
    return UTRUE;
  }

  /* hexadecimal numbers */
  if (first == '0' && second == 'x') {
    lexer->current += 2;
    while (isHexDigit(PEEK)) {
      lexer->current++;
    }
    token->type = TOKEN_NUMBER_HEX;
    token->length = lexer->current - token->start;
    return UTRUE;
  }

  /* binary numbers */
  if (first == '0' && second == 'b') {
    lexer->current += 2;
    while (PEEK == '0' || PEEK == '1') {
      lexer->current++;
    }
    token->type = TOKEN_NUMBER_BIN;
    token->length = lexer->current - token->start;
    return UTRUE;
  }

  /* decimal numbers */
  if (isDigit(first)) {
    lexer->current++;
    while (isDigit(PEEK)) {
      lexer->current++;
    }
    if (PEEK == '.') {
      lexer->current++;
      while (isDigit(PEEK)) {
        lexer->current++;
      }
    }
    token->type = TOKEN_NUMBER;
    token->length = lexer->current - token->start;
    return UTRUE;
  }

  /* identifiers and keywords */
  if (isUnderAlpha(first)) {
    lexer->current++;
    while (isUnderAlnum(PEEK)) {
      lexer->current++;
    }
    token->type = TOKEN_IDENTIFIER;
    token->length = lexer->current - token->start;
    {
      /* TODO: Consider a trie here */
      size_t i;
      for (i = 0; i < sizeof(keywords) / sizeof(KeywordInfo); i++) {
        if (token->length == keywords[i].len &&
            memcmp(token->start, keywords[i].name, keywords[i].len) == 0) {
          token->type = keywords[i].type;
          break;
        }
      }
    }
    return UTRUE;
  }

  /* two character symbols */
  {
    size_t i;
    for (i = 0; i < sizeof(twoCharSymbols) / sizeof(TwoCharSymbolInfo); i++) {
      if (twoCharSymbols[i].first == first && twoCharSymbols[i].second == second) {
        lexer->current += 2;
        token->type = twoCharSymbols[i].type;
        token->length = 2;
        return UTRUE;
      }
    }
  }

  /* one character symbols */
  {
    size_t i;
    for (i = 0; i < sizeof(oneCharSymbols) / sizeof(OneCharSymbolInfo); i++) {
      if (oneCharSymbols[i].first == first) {
        lexer->current++;
        token->type = oneCharSymbols[i].type;
        token->length = 1;
        switch (first) {
          case '(':
          case '[':
          case '{':
            lexer->groupingDepth++;
            break;
          case ')':
          case ']':
          case '}':
            lexer->groupingDepth--;
            if (lexer->groupingDepth < 0) {
              runtimeError("Unmatched '%c' token", first);
              return UFALSE;
            }
            break;
          default:
            break;
        }
        return UTRUE;
      }
    }
  }

  runtimeError("Unrecognized token (%c) on line %d", first, lexer->line);
  while (PEEK != '\0') {
    lexer->current++;
  }
  return UFALSE;
}

const char *tokenTypeToName(TokenType type) {
  switch (type) {
    case TOKEN_ERROR:
      return "ERROR";
    case TOKEN_LEFT_PAREN:
      return "LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:
      return "RIGHT_PAREN";
    case TOKEN_LEFT_BRACE:
      return "LEFT_BRACE";
    case TOKEN_RIGHT_BRACE:
      return "RIGHT_BRACE";
    case TOKEN_LEFT_BRACKET:
      return "LEFT_BRACKET";
    case TOKEN_RIGHT_BRACKET:
      return "RIGHT_BRACKET";
    case TOKEN_COMMA:
      return "COMMA";
    case TOKEN_DOT:
      return "DOT";
    case TOKEN_MINUS:
      return "MINUS";
    case TOKEN_PERCENT:
      return "PERCENT";
    case TOKEN_PLUS:
      return "PLUS";
    case TOKEN_COLON:
      return "COLON";
    case TOKEN_SEMICOLON:
      return "SEMICOLON";
    case TOKEN_SLASH:
      return "SLASH";
    case TOKEN_STAR:
      return "STAR";
    case TOKEN_AT:
      return "AT";
    case TOKEN_PIPE:
      return "PIPE";
    case TOKEN_AMPERSAND:
      return "AMPERSAND";
    case TOKEN_CARET:
      return "CARET";
    case TOKEN_TILDE:
      return "TILDE";
    case TOKEN_QMARK:
      return "QMARK";
    case TOKEN_SHIFT_LEFT:
      return "SHIFT_LEFT";
    case TOKEN_SHIFT_RIGHT:
      return "SHIFT_RIGHT";
    case TOKEN_BANG:
      return "BANG";
    case TOKEN_BANG_EQUAL:
      return "BANG_EQUAL";
    case TOKEN_EQUAL:
      return "EQUAL";
    case TOKEN_EQUAL_EQUAL:
      return "EQUAL_EQUAL";
    case TOKEN_GREATER:
      return "GREATER";
    case TOKEN_GREATER_EQUAL:
      return "GREATER_EQUAL";
    case TOKEN_LESS:
      return "LESS";
    case TOKEN_LESS_EQUAL:
      return "LESS_EQUAL";
    case TOKEN_SLASH_SLASH:
      return "SLASH_SLASH";
    case TOKEN_STAR_STAR:
      return "STAR_STAR";
    case TOKEN_IDENTIFIER:
      return "IDENTIFIER";
    case TOKEN_STRING:
      return "STRING";
    case TOKEN_RAW_STRING:
      return "RAW_STRING";
    case TOKEN_NUMBER:
      return "NUMBER";
    case TOKEN_NUMBER_HEX:
      return "NUMBER_HEX";
    case TOKEN_NUMBER_BIN:
      return "NUMBER_BIN";
    case TOKEN_AND:
      return "AND";
    case TOKEN_CLASS:
      return "CLASS";
    case TOKEN_DEF:
      return "DEF";
    case TOKEN_ELIF:
      return "ELIF";
    case TOKEN_ELSE:
      return "ELSE";
    case TOKEN_FALSE:
      return "FALSE";
    case TOKEN_FOR:
      return "FOR";
    case TOKEN_IF:
      return "IF";
    case TOKEN_NIL:
      return "NIL";
    case TOKEN_OR:
      return "OR";
    case TOKEN_RETURN:
      return "RETURN";
    case TOKEN_SUPER:
      return "SUPER";
    case TOKEN_THIS:
      return "THIS";
    case TOKEN_TRUE:
      return "TRUE";
    case TOKEN_VAR:
      return "VAR";
    case TOKEN_WHILE:
      return "WHILE";
    case TOKEN_AS:
      return "AS";
    case TOKEN_ASSERT:
      return "ASSERT";
    case TOKEN_ASYNC:
      return "ASYNC";
    case TOKEN_AWAIT:
      return "AWAIT";
    case TOKEN_BREAK:
      return "BREAK";
    case TOKEN_CONTINUE:
      return "CONTINUE";
    case TOKEN_DEL:
      return "DEL";
    case TOKEN_EXCEPT:
      return "EXCEPT";
    case TOKEN_FINAL:
      return "FINAL";
    case TOKEN_FINALLY:
      return "FINALLY";
    case TOKEN_FROM:
      return "FROM";
    case TOKEN_GLOBAL:
      return "GLOBAL";
    case TOKEN_IMPORT:
      return "IMPORT";
    case TOKEN_IN:
      return "IN";
    case TOKEN_IS:
      return "IS";
    case TOKEN_LAMBDA:
      return "LAMBDA";
    case TOKEN_NOT:
      return "NOT";
    case TOKEN_PASS:
      return "PASS";
    case TOKEN_RAISE:
      return "RAISE";
    case TOKEN_STATIC:
      return "STATIC";
    case TOKEN_THEN:
      return "THEN";
    case TOKEN_TRAIT:
      return "TRAIT";
    case TOKEN_TRY:
      return "TRY";
    case TOKEN_WITH:
      return "WITH";
    case TOKEN_YIELD:
      return "YIELD";
    case TOKEN_NEWLINE:
      return "NEWLINE";
    case TOKEN_INDENT:
      return "INDENT";
    case TOKEN_DEDENT:
      return "DEDENT";
    case TOKEN_EOF:
      return "EOF";
  }
  return "INVALID_TOKEN_TYPE";
}
