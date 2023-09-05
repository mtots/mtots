#include "mtots3parser.h"

#include <stdio.h>
#include <stdlib.h>

#include "mtots1err.h"
#include "mtots3escape.h"
#include "mtots3lexer.h"

#define PEEK (parser->peek)
#define AT(tokenType) (PEEK.type == (tokenType))
#define NEXT()                                     \
  if (!lexerNext(&parser->lexer, &parser->peek)) { \
    return STATUS_ERR;                             \
  }
#define EXPECT(tokenType)                \
  if (!expectToken(parser, tokenType)) { \
    return STATUS_ERR;                   \
  }                                      \
  NEXT();

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
static Status parsePrec(Parser *parser, Precedence prec, Ast **out);
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

static Status parseStatementDelimiter(Parser *parser) {
  if (!AT(TOKEN_NEWLINE) && !AT(TOKEN_SEMICOLON) && !AT(TOKEN_EOF)) {
    runtimeError("Expected statement delimiter but got %s",
                 tokenTypeToName(parser->peek.type));
    return STATUS_ERR;
  }
  while (AT(TOKEN_NEWLINE) || AT(TOKEN_SEMICOLON)) {
    NEXT();
  }
  return STATUS_OK;
}

static Status parseStatement(Parser *parser, Ast **out) {
  switch (parser->peek.type) {
    default:
      break;
  }
  if (!parseExpression(parser, out)) {
    return STATUS_ERR;
  }
  return parseStatementDelimiter(parser);
}

static Status parseStatementList(Parser *parser, Ast **out) {
  Ast *first = NULL, **next = &first;
  while (AT(TOKEN_NEWLINE) || AT(TOKEN_SEMICOLON)) {
    NEXT();
  }
  while (!AT(TOKEN_EOF) && !AT(TOKEN_RIGHT_BRACE)) {
    Ast *stmt;
    if (!parseStatement(parser, &stmt)) {
      return STATUS_ERR;
    }
    *next = stmt;
    next = &stmt->next;
    while (AT(TOKEN_NEWLINE) || AT(TOKEN_SEMICOLON)) {
      NEXT();
    }
  }
  *out = first;
  return STATUS_OK;
}

static Status stringTokenToString(Token *token, String **out) {
  size_t quoteLen;
  char quoteChar = token->start[0];
  char quoteStr[4];
  String *string = newString("");

  if (quoteChar == token->start[1] &&
      quoteChar == token->start[2]) {
    quoteStr[0] = quoteStr[1] = quoteStr[2] = quoteChar;
    quoteStr[3] = '\0';
    quoteLen = 3;
  } else {
    quoteStr[0] = quoteChar;
    quoteStr[1] = '\0';
    quoteLen = 1;
  }

  if (!unescapeString(string, token->start + quoteLen, quoteStr, quoteLen)) {
    return STATUS_ERR;
  }

  freezeString(string);

  *out = string;

  return STATUS_OK;
}

static Status parsePrefix(Parser *parser, Ast **out) {
  size_t line = parser->peek.line;
  switch (parser->peek.type) {
    case TOKEN_MINUS: {
      Ast *arg;
      NEXT();
      if (!parsePrec(parser, PREC_UNARY, &arg)) {
        return STATUS_ERR;
      }
      *out = newAstUnop(line, UNOP_NEGATIVE, arg);
      return STATUS_OK;
    }
    case TOKEN_PLUS: {
      Ast *arg;
      NEXT();
      if (!parsePrec(parser, PREC_UNARY, &arg)) {
        return STATUS_ERR;
      }
      *out = newAstUnop(line, UNOP_POSITIVE, arg);
      return STATUS_OK;
    }
    case TOKEN_NOT: {
      Ast *arg;
      NEXT();
      if (!parsePrec(parser, PREC_NOT, &arg)) {
        return STATUS_ERR;
      }
      *out = newAstLogical(line, LOGICAL_NOT, arg);
      return STATUS_OK;
    }
    case TOKEN_NIL:
      *out = newAstLiteral(line, nilValue());
      NEXT();
      return STATUS_OK;
    case TOKEN_TRUE:
      *out = newAstLiteral(line, boolValue(UTRUE));
      NEXT();
      return STATUS_OK;
    case TOKEN_FALSE:
      *out = newAstLiteral(line, boolValue(UFALSE));
      NEXT();
      return STATUS_OK;
    case TOKEN_IF: {
      Ast *args;
      NEXT();
      if (!parseExpression(parser, &args)) {
        return STATUS_ERR;
      }
      EXPECT(TOKEN_THEN); /* TODO: handle `args` leaking */
      if (!parseExpression(parser, &args->next)) {
        freeAst(args);
        return STATUS_ERR;
      }
      EXPECT(TOKEN_ELSE); /* TODO: handle `args` leaking */
      if (!parseExpression(parser, &args->next->next)) {
        freeAst(args);
        return STATUS_ERR;
      }
      *out = newAstLogical(line, LOGICAL_IF, args);
      return STATUS_OK;
    }
    case TOKEN_NUMBER:
      *out = newAstLiteral(line, numberValue(strtod(parser->peek.start, 0)));
      NEXT();
      return STATUS_OK;
    case TOKEN_STRING: {
      String *string;
      if (!stringTokenToString(&parser->peek, &string)) {
        return STATUS_ERR;
      }
      *out = newAstLiteral(line, stringValue(string));
      releaseString(string); /* *out automatically holds a retain on string */
      NEXT();
      return STATUS_OK;
    }
    case TOKEN_IDENTIFIER:
      *out = newAstGetGlobal(
          line,
          newSymbolWithLength(parser->peek.start, parser->peek.length));
      NEXT();
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
  *out = lhs;
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
  EXPECT(TOKEN_RIGHT_PAREN);
  *out = newAstCall(line, lhs, NULL, args);
  return STATUS_OK;
}

static Status parseBinop(Parser *parser, Ast *lhs, Ast **out) {
  BinopType op;
  size_t line = parser->peek.line;
  ParseRule *rule = &rules[parser->peek.type];
  switch (parser->peek.type) {
    case TOKEN_PLUS:
      op = BINOP_ADD;
      break;
    case TOKEN_MINUS:
      op = BINOP_SUBTRACT;
      break;
    case TOKEN_STAR:
      op = BINOP_MULTIPLY;
      break;
    case TOKEN_PERCENT:
      op = BINOP_MODULO;
      break;
    case TOKEN_SLASH:
      op = BINOP_DIVIDE;
      break;
    case TOKEN_SLASH_SLASH:
      op = BINOP_FLOOR_DIVIDE;
      break;
    default:
      /* NOTE: this is an internal error, not an error with the input program */
      panic("parseBinop called without a binary operator");
  }
  NEXT();
  if (!parsePrec(parser, (Precedence)(rule->precedence + 1), &lhs->next)) {
    return STATUS_ERR;
  }
  *out = newAstBinop(line, op, lhs);
  return STATUS_OK;
}

static Status parseLogicalBinop(Parser *parser, Ast *lhs, Ast **out) {
  LogicalType op;
  size_t line = parser->peek.line;
  ParseRule *rule = &rules[parser->peek.type];
  switch (parser->peek.type) {
    case TOKEN_OR:
      op = LOGICAL_OR;
      break;
    case TOKEN_AND:
      op = LOGICAL_AND;
      break;
    default:
      /* NOTE: this is an internal error, not an error with the input program */
      panic("parseLogicalBinop called without a logical binary operator");
  }
  NEXT();
  if (!parsePrec(parser, rule->precedence, &lhs->next)) {
    return STATUS_ERR;
  }
  *out = newAstLogical(line, op, lhs);
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
  rules[TOKEN_PLUS] = newRule(parseBinop, PREC_TERM);
  rules[TOKEN_MINUS] = newRule(parseBinop, PREC_TERM);
  rules[TOKEN_STAR] = newRule(parseBinop, PREC_FACTOR);
  rules[TOKEN_PERCENT] = newRule(parseBinop, PREC_FACTOR);
  rules[TOKEN_SLASH] = newRule(parseBinop, PREC_FACTOR);
  rules[TOKEN_SLASH_SLASH] = newRule(parseBinop, PREC_FACTOR);
  rules[TOKEN_OR] = newRule(parseLogicalBinop, PREC_OR);
  rules[TOKEN_AND] = newRule(parseLogicalBinop, PREC_AND);
}
