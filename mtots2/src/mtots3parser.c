#include "mtots3parser.h"

#include <stdlib.h>

#include "mtots1err.h"
#include "mtots3lexer.h"

#define PEEK (parser->peek)
#define AT(tokenType) (PEEK.type == (tokenType))
#define EXPECT(tokenType)                          \
  if (!expectToken(parser, tokenType)) {           \
    return STATUS_ERR;                             \
  }                                                \
  if (!lexerNext(&parser->lexer, &parser->peek)) { \
    return STATUS_ERR;                             \
  }

typedef enum Precedence {
  PREC_NONE,
  PREC_OR,          /* or */
  PREC_AND,         /* and */
  PREC_NOT,         /* not */
  PREC_COMPARISON,  /* == != < > <= >= in not-in is is-not as */
  PREC_SHIFT,       /* << >> */
  PREC_BITWISE_AND, /* & */
  PREC_BITWISE_XOR, /* ^ */
  PREC_BITWISE_OR,  /* | */
  PREC_TERM,        /* + - */
  PREC_FACTOR,      /* * / // % */
  PREC_UNARY,       /* - ~ */
  PREC_POWER,       /* ** */
  PREC_CALL,        /* . () [] */
  PREC_PRIMARY
} Precedence;

typedef struct Parser {
  Lexer lexer;
  Token peek;
} Parser;

typedef Status (*ParseFn)(Parser *parser, Ast *lhs, Ast **out);

typedef struct ParseRule {
  ParseFn fn;
  Precedence precedence;
} ParseRule;

static ubool parserRulesInitialized;
static ParseRule rules[TOKEN_EOF + 1];

static void initParserRules(void);
static Status parseStatement(Parser *parser, Ast **out);
static Status parseStatementList(Parser *parser, Ast **out);
static Status parseExpression(Parser *parser, Ast **out);
static Status parseArgumentList(Parser *parser, Ast **out);

Status parse(const char *source, Ast **out) {
  Parser parser;
  Ast *stmts;
  initParserRules();
  initLexer(&parser.lexer, source);
  if (!lexerNext(&parser.lexer, &parser.peek)) {
    return STATUS_ERR;
  }
  if (!parseStatementList(&parser, &stmts)) {
    return STATUS_ERR;
  }
  *out = (Ast *)newAstBlock(1, stmts);
  return STATUS_OK;
}

static Status expectToken(Parser *parser, TokenType tokenType) {
  if (!AT(tokenType)) {
    runtimeError("Expected %s but got %s",
                 tokenTypeToName(tokenType),
                 tokenTypeToName(PEEK.type));
    return STATUS_ERR;
  }
  return STATUS_OK;
}

static Status parseStatement(Parser *parser, Ast **out) {
  if (!parseExpression(parser, out)) {
    return STATUS_ERR;
  }
  return STATUS_OK;
}

static Status parseStatementList(Parser *parser, Ast **out) {
  Ast *first = NULL, **next = &first;
  while (!AT(TOKEN_EOF) && !AT(TOKEN_RIGHT_BRACE)) {
    Ast *stmt;
    if (!parseStatement(parser, &stmt)) {
      return STATUS_ERR;
    }
    *next = stmt;
    next = &stmt->next;
  }
  *out = first;
  return STATUS_OK;
}

static Status parsePrefix(Parser *parser, Ast **out) {
  size_t line = parser->peek.line;
  switch (parser->peek.type) {
    case TOKEN_NUMBER:
      *out = (Ast *)newAstLiteral(line, numberValue(strtod(parser->peek.start, 0)));
      return STATUS_OK;
    default:
      break;
  }
  runtimeError("Expected Expression but got %s",
               tokenTypeToName(parser->peek.type));
  return STATUS_ERR;
}

static Status parsePrec(Parser *parser, Precedence prec, Ast **out) {
  Ast *lhs;
  if (!parsePrefix(parser, &lhs)) {
    return STATUS_ERR;
  }
  while (prec <= rules[parser->peek.type].precedence) {
    ParseFn rule = rules[parser->peek.type].fn;
    Ast *expr;
    if (!rule(parser, lhs, &expr)) {
      return STATUS_ERR;
    }
    lhs = expr;
  }
  return STATUS_OK;
}

static Status parseExpression(Parser *parser, Ast **out) {
  return parsePrec(parser, PREC_OR, out);
}

static Status parseArgumentList(Parser *parser, Ast **out) {
  Ast *first = NULL, **next = &first;
  while (!AT(TOKEN_EOF) &&
         !AT(TOKEN_RIGHT_BRACE) &&
         !AT(TOKEN_RIGHT_BRACKET) &&
         !AT(TOKEN_RIGHT_PAREN)) {
    Ast *expr;
    if (!parseExpression(parser, &expr)) {
      return STATUS_ERR;
    }
    *next = expr;
    next = &expr->next;
  }
  *out = first;
  return STATUS_OK;
}

static Status parseFunctionCall(Parser *parser, Ast *lhs, Ast **out) {
  Ast *args;
  size_t line = parser->peek.line;
  EXPECT(TOKEN_LEFT_PAREN);
  if (!parseArgumentList(parser, &args)) {
    return STATUS_ERR;
  }
  *out = (Ast *)newAstCall(line, lhs, NULL, args);
  return STATUS_OK;
}

static ParseRule newRule(ParseFn fn, Precedence precedence) {
  ParseRule rule;
  rule.fn = fn;
  rule.precedence = precedence;
  return rule;
}

static void initParserRules(void) {
  if (parserRulesInitialized) {
    return;
  }
  parserRulesInitialized = UTRUE;
  rules[TOKEN_LEFT_PAREN] = newRule(parseFunctionCall, PREC_CALL);
}
