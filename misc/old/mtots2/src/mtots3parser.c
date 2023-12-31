#include "mtots3parser.h"

#include <stdio.h>
#include <stdlib.h>

#include "mtots1err.h"
#include "mtots3escape.h"
#include "mtots3lexer.h"

#define PEEK (parser->peek)
#define AT(tokenType) (PEEK.type == (tokenType))
#define NEXT()              \
  if (!nextToken(parser)) { \
    return STATUS_ERR;      \
  }
#define EXPECT(tokenType)                \
  if (!expectToken(parser, tokenType)) { \
    return STATUS_ERR;                   \
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

static Status stringTokenToString(Token *token, String **out);
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

static Status nextToken(Parser *parser) {
  return lexerNext(&parser->lexer, &parser->peek);
}

static Status expectToken(Parser *parser, TokenType tokenType) {
  if (!AT(tokenType)) {
    runtimeError("Expected %s but got %s",
                 tokenTypeToName(tokenType),
                 tokenTypeToName(PEEK.type));
    return STATUS_ERR;
  }
  return nextToken(parser);
}

static Status getVariable(Parser *parser, u32 line, Symbol *name, Ast **out) {
  *out = newAstGetVar(line, name);
  return STATUS_OK;
}

static Status setVariable(Parser *parser, u32 line, Symbol *name, Ast *value, Ast **out) {
  *out = newAstSetVar(line, name, value);
  return STATUS_OK;
}

static Status consumeStatementDelimiter(Parser *parser) {
  while (AT(TOKEN_NEWLINE) || AT(TOKEN_SEMICOLON)) {
    NEXT();
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

static Status parseBlock(Parser *parser, Ast **out) {
  Ast *first;
  u32 line = parser->peek.line;
  EXPECT(TOKEN_COLON);
  if (!consumeStatementDelimiter(parser)) {
    return STATUS_ERR;
  }
  if (!expectToken(parser, TOKEN_INDENT)) {
    return STATUS_ERR;
  }
  if (!parseStatementList(parser, &first)) {
    return STATUS_ERR;
  }
  if (!expectToken(parser, TOKEN_DEDENT)) {
    freeAst(first);
    return STATUS_ERR;
  }
  *out = newAstBlock(line, first);
  return STATUS_OK;
}

static ubool maybeAtTypeExpression(Parser *parser) {
  return AT(TOKEN_IDENTIFIER) || AT(TOKEN_NIL);
}

static Status skipTypeExpression(Parser *parser) {
  if (AT(TOKEN_NIL)) {
    NEXT();
  } else {
    EXPECT(TOKEN_IDENTIFIER);
  }
  for (;;) {
    if (AT(TOKEN_QMARK)) {
      NEXT();
      continue;
    }
    if (AT(TOKEN_DOT)) {
      NEXT();
      EXPECT(TOKEN_IDENTIFIER);
      continue;
    }
    if (AT(TOKEN_PIPE)) {
      NEXT();
      if (!skipTypeExpression(parser)) {
        return STATUS_ERR;
      }
      continue;
    }
    if (AT(TOKEN_LEFT_BRACKET)) {
      NEXT();
      while (maybeAtTypeExpression(parser)) {
        if (!skipTypeExpression(parser)) {
          return STATUS_ERR;
        }
        if (AT(TOKEN_COMMA)) {
          NEXT();
        } else {
          break;
        }
      }
      EXPECT(TOKEN_RIGHT_BRACKET);
      continue;
    }
    break;
  }
  return STATUS_OK;
}

static Status parseVariableDeclaration(Parser *parser, Ast **out) {
  Symbol *name;
  Ast *rhs;
  u32 line = parser->peek.line;
  if (AT(TOKEN_FINAL)) {
    NEXT();
  } else {
    EXPECT(TOKEN_VAR);
  }
  if (!AT(TOKEN_IDENTIFIER)) {
    runtimeError("Expected variable name but got %s",
                 tokenTypeToName(parser->peek.type));
    return STATUS_ERR;
  }
  name = newSymbolWithLength(parser->peek.start, parser->peek.length);
  NEXT();

  /* Type annotation */
  if (maybeAtTypeExpression(parser)) {
    if (!skipTypeExpression(parser)) {
      return STATUS_ERR;
    }
  }

  /* Documentation */
  if (AT(TOKEN_STRING) || AT(TOKEN_RAW_STRING)) {
    NEXT();
  }

  EXPECT(TOKEN_EQUAL);

  if (!parseExpression(parser, &rhs)) {
    return STATUS_ERR;
  }

  if (!parseStatementDelimiter(parser)) {
    freeAst(rhs);
    return STATUS_ERR;
  }

  return setVariable(parser, line, name, rhs, out);
}

static Status parseIfRecursive(Parser *parser, Ast **out) {
  Ast *cond;
  u32 line = parser->peek.line;
  if (!parseExpression(parser, &cond)) {
    return STATUS_ERR;
  }
  if (!parseBlock(parser, &cond->next)) {
    freeAst(cond);
    return STATUS_ERR;
  }
  if (!consumeStatementDelimiter(parser)) {
    freeAst(cond);
    return STATUS_ERR;
  }
  if (AT(TOKEN_ELIF)) {
    if (!nextToken(parser)) {
      freeAst(cond);
      return STATUS_ERR;
    }
    if (!parseIfRecursive(parser, &cond->next->next)) {
      freeAst(cond);
      return STATUS_ERR;
    }
  } else if (AT(TOKEN_ELSE)) {
    if (!nextToken(parser)) {
      freeAst(cond);
      return STATUS_ERR;
    }
    if (!parseBlock(parser, &cond->next->next)) {
      freeAst(cond);
      return STATUS_ERR;
    }
  } else {
    cond->next->next = newAstLiteral(parser->peek.line, nilValue());
  }
  *out = newAstLogical(line, LOGICAL_IF, cond);
  return STATUS_OK;
}

static Status parseConstantValue(Parser *parser, Value *defaultValue) {
  switch (parser->peek.type) {
    case TOKEN_NIL:
      *defaultValue = nilValue();
      NEXT();
      return STATUS_OK;
    case TOKEN_TRUE:
      *defaultValue = boolValue(UTRUE);
      NEXT();
      return STATUS_OK;
    case TOKEN_FALSE:
      *defaultValue = boolValue(UFALSE);
      NEXT();
      return STATUS_OK;
    case TOKEN_NUMBER:
      *defaultValue = numberValue(strtod(parser->peek.start, 0));
      NEXT();
      return STATUS_OK;
    case TOKEN_STRING: {
      String *string;
      if (!stringTokenToString(&parser->peek, &string)) {
        return STATUS_ERR;
      }
      if (!nextToken(parser)) {
        releaseString(string);
        return STATUS_ERR;
      }
      *defaultValue = stringValue(string);
      return STATUS_OK;
    }
    default:
      break;
  }
  runtimeError("Expected literal value but got %s",
               tokenTypeToName(parser->peek.type));
  return STATUS_ERR;
}

static Status parseParameters(Parser *parser, Parameter **out) {
  Parameter *param = NULL, **next = &param;
  ubool seenDefault = UFALSE;
  while (!AT(TOKEN_RIGHT_PAREN)) {
    Symbol *name;
    Value defaultValue = sentinelValue();
    u32 line = parser->peek.line;
    if (!AT(TOKEN_IDENTIFIER)) {
      runtimeError("[line %lu] Expected parameter name but got %s",
                   (unsigned long)line,
                   tokenTypeToName(parser->peek.type));
      freeParameter(param);
      return STATUS_ERR;
    }
    name = newSymbolWithLength(parser->peek.start, parser->peek.length);
    if (maybeAtTypeExpression(parser)) {
      if (!skipTypeExpression(parser)) {
        freeParameter(param);
        return STATUS_ERR;
      }
    }
    if (AT(TOKEN_EQUAL)) {
      seenDefault = UTRUE;
      if (!parseConstantValue(parser, &defaultValue)) {
        freeParameter(param);
        return STATUS_ERR;
      }
    } else if (seenDefault) {
      runtimeError("[line %lu] Required parameters cannot come after required parameters",
                   (unsigned long)line);
      freeParameter(param);
      return STATUS_ERR;
    }
    *next = newParameter(name, defaultValue);
    releaseValue(defaultValue);
    next = &(*next)->next;
    if (!AT(TOKEN_COMMA)) {
      break;
    }
    if (!nextToken(parser)) {
      freeParameter(param);
      return STATUS_ERR;
    }
  }
  *out = param;
  return STATUS_OK;
}

static Status parseFunctionDefinition(Parser *parser, Ast **out) {
  Symbol *name;
  Parameter *parameters;
  Ast *body, *function;
  u32 line = parser->peek.line;
  EXPECT(TOKEN_DEF);
  if (!AT(TOKEN_IDENTIFIER)) {
    runtimeError("[line %lu] Expected function name but got %s",
                 (unsigned long)line,
                 tokenTypeToName(parser->peek.type));
    return STATUS_ERR;
  }
  name = newSymbolWithLength(parser->peek.start, parser->peek.length);
  NEXT();
  EXPECT(TOKEN_LEFT_PAREN);
  if (!parseParameters(parser, &parameters)) {
    return STATUS_ERR;
  }
  if (!expectToken(parser, TOKEN_RIGHT_PAREN)) {
    freeParameter(parameters);
    return STATUS_ERR;
  }
  if (!parseBlock(parser, &body)) {
    freeParameter(parameters);
    return STATUS_ERR;
  }
  function = newAstFunction(line, name, parameters, body);
  if (!setVariable(parser, line, name, function, out)) {
    freeAst((Ast *)function);
    return STATUS_ERR;
  }
  return STATUS_OK;
}

static Status parseStatement(Parser *parser, Ast **out) {
  switch (parser->peek.type) {
    case TOKEN_FINAL:
      return parseVariableDeclaration(parser, out);
    case TOKEN_VAR:
      return parseVariableDeclaration(parser, out);
    case TOKEN_DEF:
      return parseFunctionDefinition(parser, out);
    case TOKEN_IF:
      NEXT();
      return parseIfRecursive(parser, out);
    default:
      break;
  }
  if (!parseExpression(parser, out)) {
    return STATUS_ERR;
  }
  if (!parseStatementDelimiter(parser)) {
    freeAst(*out);
    return STATUS_ERR;
  }
  return STATUS_OK;
}

static Status parseStatementList(Parser *parser, Ast **out) {
  Ast *first = NULL, **next = &first;
  if (!consumeStatementDelimiter(parser)) {
    return STATUS_ERR;
  }
  while (!AT(TOKEN_EOF) && !AT(TOKEN_RIGHT_BRACE) && !AT(TOKEN_DEDENT)) {
    Ast *stmt;
    if (!parseStatement(parser, &stmt)) {
      freeAst(first);
      return STATUS_ERR;
    }
    *next = stmt;
    next = &stmt->next;
    if (!consumeStatementDelimiter(parser)) {
      freeAst(first);
      return STATUS_ERR;
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
  u32 line = parser->peek.line;
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
    case TOKEN_IDENTIFIER: {
      Symbol *name = newSymbolWithLength(parser->peek.start, parser->peek.length);
      NEXT();
      if (AT(TOKEN_EQUAL)) {
        Ast *rhs;
        NEXT();
        if (!parseExpression(parser, &rhs)) {
          return STATUS_ERR;
        }
        return setVariable(parser, line, name, rhs, out);
      }
      return getVariable(parser, line, name, out);
    }
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
  u32 line = parser->peek.line;
  EXPECT(TOKEN_LEFT_PAREN);
  if (!parseArgumentList(parser, &lhs->next)) {
    return STATUS_ERR;
  }
  EXPECT(TOKEN_RIGHT_PAREN);
  *out = newAstCall(line, NULL, lhs);
  return STATUS_OK;
}

static Status parseBinop(Parser *parser, Ast *lhs, Ast **out) {
  BinopType op;
  u32 line = parser->peek.line;
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
  u32 line = parser->peek.line;
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
