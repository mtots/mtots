#include "mtots_parser.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#define MAX_CONST_PER_THUNK 255
#define AT(t) (atToken(parser, (t)))
#define WRAP(e)            \
  do {                     \
    if (!(e)) {            \
      return STATUS_ERROR; \
    }                      \
  } while (0)
#define EXPECT(t) WRAP(expectToken(parser, (t)))
#define ADVANCE() WRAP(advance(parser))
#define ADD_CONST_VALUE(v, r) WRAP(addConstValue(parser, (v), (r)))
#define ADD_CONST_STRING(v, r) WRAP(addConstValue(parser, valString(v), (r)))
#define ADD_CONST_SLICE(s, r) WRAP(addConstSlice(parser, (s), (r)))
#define CHECK(f) WRAP((f)(parser))
#define CHECK1(f, a) WRAP((f)(parser, (a)))
#define CHECK2(f, a, b) WRAP((f)(parser, (a), (b)))
#define CHECK3(f, a, b, c) WRAP((f)(parser, (a), (b), (c)))
#define CHECK4(f, a, b, c, d) WRAP((f)(parser, (a), (b), (c), (d)))
#define EMIT1(a) WRAP(emit1(parser, (a)))
#define EMIT2(a, b) WRAP(emit2(parser, (a), (b)))
#define EMIT3(a, b, c) WRAP(emit3(parser, (a), (b), (c)))
#define EMIT_CONSTID(v) WRAP(emitConstID(parser, (v)))
#define EMIT1C(a, b) WRAP(emit1C(parser, (a), (b)))
#define EMIT1C1(a, b, c) WRAP(emit1C1(parser, (a), (b), (c)))
#define EMIT_CONST(v) WRAP(emitConst(parser, (v)))
#define TODO(n)               \
  do {                        \
    runtimeError("TODO " #n); \
    return STATUS_ERROR;      \
  } while (0)
#define ENV (parser->env)
#define THUNK (ENV->thunk)
#define THUNK_CONTEXT (ENV->thunkContext)
#define MODULE_NAME_CHARS (THUNK->moduleName->chars)
#define CURRENT_LINE (parser->current.line)
#define PREVIOUS_LINE (parser->previous.line)
#define MARK_LOCAL_READY(local) ((local)->scopeDepth = ENV->scopeDepth)
#define CHUNK_POS (THUNK->chunk.count)
#define SLICE_PREVIOUS() (newSlice(parser->previous.start, parser->previous.length))
#define SLICE_CURRENT() (newSlice(parser->current.start, parser->current.length))

#define ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(r) \
  ADD_CONST_SLICE(SLICE_PREVIOUS(), (r))

#define EXPECT_STATEMENT_DELIMITER() CHECK(expectStatementDelimiter)

#define CONST_ID_MAX U16_MAX

typedef struct ConstID {
  u16 value;
} ConstID;

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

typedef struct StringSlice {
  const char *chars;
  size_t length;
} StringSlice;

typedef struct Local {
  u8 index;
  StringSlice name;
  i16 scopeDepth;
  ubool isCaptured;
} Local;

typedef struct VariableDeclaration {
  ubool isLocal;
  union {
    ConstID global; /* the constant ID containing the name of the variable */
    Local *local;
  } as;
} VariableDeclaration;

typedef struct Upvalue {
  u8 index;
  ubool isLocal;
} Upvalue;

/* Additional context about the Thunk as we are compiling the code */
typedef struct ThunkContext {
  ubool isInitializer; /* if true, disallows explicit returns and returns 'this' */
  ubool isMethod;      /* if true, 'this' variable is set up */
  ubool isLambda;      /* if true, may not contain any statements */
} ThunkContext;

typedef struct Environment {
  struct Environment *enclosing; /* the parent environment */
  ObjThunk *thunk;               /* active Thunk */
  ThunkContext *thunkContext;
  Local locals[U8_COUNT];
  i16 localsCount; /* number of active local variables in this environment */
  i16 scopeDepth;  /* scope depth */
  Upvalue upvalues[U8_COUNT];
} Environment;

typedef struct ClassInfo {
  struct ClassInfo *enclosing;
  ubool hasSuperClass;
} ClassInfo;

typedef enum DefaultArgumentType {
  DEFARG_NIL,
  DEFARG_TRUE,
  DEFARG_FALSE,
  DEFARG_NUMBER,
  DEFARG_STRING
} DefaultArgumentType;

typedef struct DefaultArgument {
  DefaultArgumentType type;
  union {
    double number;
    Token string;
  } as;
} DefaultArgument;

typedef struct Parser {
  Environment *env;     /* the deepest currently active environment */
  ClassInfo *classInfo; /* information about the current class definition, if any */
  Lexer *lexer;         /* lexer to read tokens from */
  Token current;        /* currently considered token */
  Token previous;       /* previously considered token */
} Parser;

typedef Status (*ParseFn)(Parser *);

typedef struct ParseRule {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static Parser *activeParser;
static ParseRule rules[TOKEN_EOF + 1];
static Status parseRulesInitialized;

static Status parseDeclaration(Parser *parser);
static Status parseStatement(Parser *parser);
static Status parseExpression(Parser *parser);
static Status parsePrec(Parser *parser, Precedence prec);
static void initParseRulesPrivate(void);
static Status addConstValue(Parser *parser, Value value, ConstID *ref);
static Status stringTokenToObjString(Parser *parser, Token *token, String **out);
static Status lastStringTokenToObjString(Parser *parser, String **out);
static Status parseBlock(Parser *parser, ubool newScope);
static Status loadVariableByName(Parser *parser, StringSlice name);
static Status storeVariableByName(Parser *parser, StringSlice name);
static Status parseFunctionCore(Parser *parser, StringSlice name, ThunkContext *thunkContext);
static Status parseArgumentList(Parser *parser, u8 *out, ubool *hasKwArgs);

static StringSlice newSlice(const char *chars, size_t length) {
  StringSlice ss;
  ss.chars = chars;
  ss.length = length;
  return ss;
}

static void initParser(Parser *parser, Lexer *lexer, Environment *env) {
  parser->env = env;
  parser->classInfo = NULL;
  parser->lexer = lexer;
  parser->current.type = TOKEN_NIL;
  parser->previous.type = TOKEN_NIL;
}

static void initThunkContext(ThunkContext *thunkContext) {
  thunkContext->isInitializer = UFALSE;
  thunkContext->isMethod = UFALSE;
  thunkContext->isLambda = UFALSE;
}

static Status sliceEquals(StringSlice slice, const char *string) {
  return strlen(string) == slice.length && memcmp(slice.chars, string, slice.length) == 0;
}

static void initEnvironment(Environment *env, ObjThunk *thunk, ThunkContext *thunkContext) {
  Local *local;
  env->enclosing = NULL;
  env->thunk = thunk;
  env->thunkContext = thunkContext;
  env->localsCount = 0;
  env->scopeDepth = 0;

  /* 'this' */
  local = &env->locals[env->localsCount++];
  local->index = env->localsCount - 1;
  local->isCaptured = UFALSE;
  local->scopeDepth = 0;
  if (thunkContext->isMethod) {
    local->name.chars = "this";
    local->name.length = 4;
  } else {
    local->name.chars = "";
    local->name.length = 0;
  }
}

static Status newLocal(Parser *parser, StringSlice name, Local **out) {
  Local *local;
  if (ENV->localsCount == U8_MAX) {
    runtimeError(
        "[%s:%d] Too many local variables",
        MODULE_NAME_CHARS,
        PREVIOUS_LINE);
    return STATUS_ERROR;
  }
  local = *out = &ENV->locals[ENV->localsCount++];
  local->isCaptured = UFALSE;
  local->name = name;
  local->scopeDepth = -1;
  return STATUS_OK;
}

static ubool atToken(Parser *parser, TokenType type) {
  return parser->current.type == type;
}

static ubool maybeAtTypeExpression(Parser *parser) {
  return atToken(parser, TOKEN_IDENTIFIER) || atToken(parser, TOKEN_NIL);
}

static Status advance(Parser *parser) {
  parser->previous = parser->current;
  return lexerNext(parser->lexer, &parser->current);
}

static Status expectToken(Parser *parser, TokenType type) {
  if (!atToken(parser, type)) {
    runtimeError(
        "[%s:%d] Expected token %s but got %s",
        MODULE_NAME_CHARS,
        CURRENT_LINE,
        tokenTypeToName(type),
        tokenTypeToName(parser->current.type));
    return STATUS_ERROR;
  }
  return advance(parser);
}

static Status expectStatementDelimiter(Parser *parser) {
  if (AT(TOKEN_NEWLINE)) {
    ADVANCE();
  } else {
    EXPECT(TOKEN_SEMICOLON);
  }
  return STATUS_OK;
}

static Status emit1(Parser *parser, u8 byte1) {
  writeChunk(&THUNK->chunk, byte1, PREVIOUS_LINE);
  return STATUS_OK;
}

static Status emit2(Parser *parser, u8 byte1, u8 byte2) {
  EMIT1(byte1);
  EMIT1(byte2);
  return STATUS_OK;
}

static Status emit3(Parser *parser, u8 byte1, u8 byte2, u8 byte3) {
  EMIT1(byte1);
  EMIT1(byte2);
  EMIT1(byte3);
  return STATUS_OK;
}

static Status emitConstID(Parser *parser, ConstID id) {
  return emit2(parser, (id.value >> 8) & 0xFF, id.value & 0xFF);
}

/*
 * Emit:
 *   1 byte, followed by
 *   1 ConstID
 */
static Status emit1C(Parser *parser, u8 opcode, ConstID id) {
  EMIT1(opcode);
  return emitConstID(parser, id);
}

/*
 * Emit:
 *   1 byte, followed by
 *   1 ConstID, followed by
 *   1 byte
 */
static Status emit1C1(Parser *parser, u8 opcode, ConstID id, u8 arg) {
  EMIT1C(opcode, id);
  return emit1(parser, arg);
}

static Status emitLoop(Parser *parser, i32 loopStart) {
  i32 offset;

  EMIT1(OP_LOOP);

  offset = CHUNK_POS - loopStart + 2;
  if (offset > U16_MAX) {
    runtimeError(
        "[%s:%d] Loop body too large",
        MODULE_NAME_CHARS,
        PREVIOUS_LINE);
    return STATUS_ERROR;
  }

  EMIT1((offset >> 8) & 0xFF);
  EMIT1(offset & 0xFF);
  return STATUS_OK;
}

static Status emitJump(Parser *parser, u8 instruction, i32 *jump) {
  EMIT3(instruction, 0xFF, 0xFF);
  *jump = CHUNK_POS - 2;
  return STATUS_OK;
}

static Status patchJump(Parser *parser, i32 offset) {
  /* -2 to adjust for the bytecode for the jump offset itself */
  i32 jump = CHUNK_POS - offset - 2;

  if (jump > U16_MAX) {
    runtimeError(
        "[%s:%d] Too much code to jump over",
        MODULE_NAME_CHARS,
        PREVIOUS_LINE);
    return STATUS_ERROR;
  }

  THUNK->chunk.code[offset] = (jump >> 8) & 0xFF;
  THUNK->chunk.code[offset + 1] = jump & 0xFF;
  return STATUS_OK;
}

static Status emitConst(Parser *parser, Value value) {
  ConstID constID;
  ADD_CONST_VALUE(value, &constID);
  EMIT1C(OP_CONSTANT, constID);
  return STATUS_OK;
}

/* Add a new constant to the constant pool. The reference to the given value
 * will be stored in `ref`. */
static Status addConstValue(Parser *parser, Value value, ConstID *ref) {
  size_t id = addConstant(&THUNK->chunk, value);
  if (id >= CONST_ID_MAX) {
    runtimeError("[%s:%d] Too many constants in thunk (count=%d, max=%d)",
                 MODULE_NAME_CHARS,
                 PREVIOUS_LINE,
                 (int)(id + 1),
                 (int)CONST_ID_MAX);
    return STATUS_ERROR;
  }
  ref->value = (u16)id;
  return STATUS_OK;
}

static Status addConstSlice(Parser *parser, StringSlice slice, ConstID *ref) {
  return addConstValue(parser, valString(internString(slice.chars, slice.length)), ref);
}

static ubool inGlobalScope(Parser *parser) {
  return parser->env->enclosing == NULL && parser->env->scopeDepth == 0;
}

static Status beginScope(Parser *parser) {
  ENV->scopeDepth++;
  return STATUS_OK;
}

static Status endScope(Parser *parser) {
  /* TODO: Coalesce multiple of these pops into a single instruction */
  ubool hasUpvalue = UFALSE;
  u16 count = 0;
  ENV->scopeDepth--;
  while (ENV->localsCount > 0 &&
         ENV->locals[ENV->localsCount - 1].scopeDepth > ENV->scopeDepth) {
    if (ENV->locals[ENV->localsCount - 1].isCaptured) {
      hasUpvalue = UTRUE;
    }
    count++;
    ENV->localsCount--;
  }
  if (count > U8_MAX) {
    /* We *probably* won't ever get here, because of the limits on
     * local variable count, but not sure - better safe than sorry */
    panic("INTERNAL ERROR: Popping too many local variables in one go");
  }
  if (count == 0) {
    /* nothing to do */
  } else if (count == 1) {
    if (hasUpvalue) {
      EMIT1(OP_CLOSE_UPVALUE);
    } else {
      EMIT1(OP_POP);
    }
  } else {
    EMIT2(OP_CLOSE_UPVALUES, count);
  }
  return STATUS_OK;
}

/* Declare a variable to exist, without yet being ready for use.
 *
 * For global varaibles, we save the name of the variable for later use.
 * For local variables, the next stack slot will be reserved for the variable.
 *
 * `isReady` indicates whether the variable is allowed to start appearing
 * in expressions.
 */
static Status declareVariable(
    Parser *parser, StringSlice name, ubool isReady, VariableDeclaration *out) {
  if (inGlobalScope(parser)) {
    out->isLocal = UFALSE;
    ADD_CONST_SLICE(name, &out->as.global);
  } else {
    out->isLocal = UTRUE;
    CHECK2(newLocal, name, &out->as.local);
    if (isReady) {
      MARK_LOCAL_READY(out->as.local);
    }
  }
  return STATUS_OK;
}

/* Initialize the declared variable the the value sitting at the top of the stack.
 *
 * For global variables, we emit opcode to pop and store the TOS to the variable.
 * For local variables, we emit not opcode since the TOS is already where the
 *   variable is meant to be stored. But we update the declaration to indicate
 *   that it is ready to be used (this is already the case if `declareVariable`
 *   was called with `isReady` set to true).
 */
static Status defineVariable(Parser *parser, VariableDeclaration *decl) {
  if (decl->isLocal) {
    MARK_LOCAL_READY(decl->as.local);
  } else {
    EMIT1C(OP_DEFINE_GLOBAL, decl->as.global);
  }
  return STATUS_OK;
}

static Status parseTypeExpression(Parser *parser) {
  /* Type expressions are completely ignored by the runtime.
   * We just check for syntax here. */
  if (AT(TOKEN_NIL)) {
    ADVANCE();
  } else {
    EXPECT(TOKEN_IDENTIFIER);
  }
  for (;;) {
    if (AT(TOKEN_QMARK)) {
      ADVANCE();
      continue;
    }
    if (AT(TOKEN_DOT)) {
      ADVANCE();
      EXPECT(TOKEN_IDENTIFIER);
      continue;
    }
    if (AT(TOKEN_PIPE)) {
      ADVANCE();
      CHECK(parseTypeExpression);
      continue;
    }
    if (AT(TOKEN_LEFT_BRACKET)) {
      ADVANCE();
      while (maybeAtTypeExpression(parser)) {
        CHECK(parseTypeExpression);
        if (AT(TOKEN_COMMA)) {
          ADVANCE();
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

static Status parseTypeParameters(Parser *parser) {
  /* Type parameters - ignored at runtime */
  if (AT(TOKEN_LEFT_BRACKET)) {
    ADVANCE();
    while (AT(TOKEN_IDENTIFIER)) {
      ADVANCE();
      if (maybeAtTypeExpression(parser)) {
        CHECK(parseTypeExpression);
      }
      if (AT(TOKEN_COMMA)) {
        ADVANCE();
      } else {
        break;
      }
    }
    EXPECT(TOKEN_RIGHT_BRACKET);
  }
  return STATUS_OK;
}

static Status parseImportStatement(Parser *parser) {
  /* Imports will always assume global scope */
  ConstID moduleName, memberName, alias;
  ubool fromStmt;
  const char *moduleNameStart, *moduleNameEnd, *shortNameStart;
  size_t shortNameLen;

  fromStmt = AT(TOKEN_FROM);
  if (fromStmt) {
    ADVANCE();
  } else {
    EXPECT(TOKEN_IMPORT);
  }

  /* Module name */
  moduleNameStart = parser->current.start;
  EXPECT(TOKEN_IDENTIFIER);
  while (AT(TOKEN_DOT)) {
    ADVANCE();
    EXPECT(TOKEN_IDENTIFIER);
  }
  shortNameStart = parser->previous.start;
  shortNameLen = parser->previous.length;
  moduleNameEnd = shortNameStart + shortNameLen;
  ADD_CONST_SLICE(newSlice(moduleNameStart, moduleNameEnd - moduleNameStart), &moduleName);

  if (fromStmt) {
    EXPECT(TOKEN_IMPORT);
    EXPECT(TOKEN_IDENTIFIER);
    ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&memberName);
  }

  if (AT(TOKEN_AS)) {
    ADVANCE();
    EXPECT(TOKEN_IDENTIFIER);
    ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&alias);
  } else if (fromStmt) {
    alias = memberName;
  } else {
    ADD_CONST_SLICE(newSlice(shortNameStart, shortNameLen), &alias);
  }

  EMIT1C(OP_IMPORT, moduleName);
  if (fromStmt) {
    EMIT1C(OP_GET_FIELD, memberName);
  }
  EMIT1C(OP_DEFINE_GLOBAL, alias);

  EXPECT_STATEMENT_DELIMITER();

  return STATUS_OK;
}

static Status parseFieldDeclaration(Parser *parser) {
  /* Field declarations have no runtime effect whatsoever
   * They are purely for documentation */
  if (AT(TOKEN_FINAL)) {
    ADVANCE();
  } else {
    EXPECT(TOKEN_VAR);
  }
  EXPECT(TOKEN_IDENTIFIER);
  CHECK(parseTypeExpression);
  if (AT(TOKEN_STRING) || AT(TOKEN_RAW_STRING)) {
    ADVANCE();
  }
  EXPECT_STATEMENT_DELIMITER();
  return STATUS_OK;
}

static Status parseMethodDeclaration(Parser *parser) {
  ThunkContext thunkContext;
  StringSlice name;
  ConstID nameID;
  initThunkContext(&thunkContext);
  thunkContext.isMethod = UTRUE;
  EXPECT(TOKEN_DEF);
  EXPECT(TOKEN_IDENTIFIER);
  name = SLICE_PREVIOUS();
  if (sliceEquals(name, "__init__")) {
    thunkContext.isInitializer = UTRUE;
  }
  ADD_CONST_SLICE(name, &nameID);
  CHECK2(parseFunctionCore, name, &thunkContext);
  EMIT1C(OP_METHOD, nameID);
  return STATUS_OK;
}

static Status parseStaticMethodDeclaration(Parser *parser) {
  ThunkContext thunkContext;
  StringSlice name;
  ConstID nameID;
  initThunkContext(&thunkContext);
  EXPECT(TOKEN_STATIC);
  EXPECT(TOKEN_DEF);
  EXPECT(TOKEN_IDENTIFIER);
  name = SLICE_PREVIOUS();
  ADD_CONST_SLICE(name, &nameID);
  CHECK2(parseFunctionCore, name, &thunkContext);
  EMIT1C(OP_STATIC_METHOD, nameID);
  return STATUS_OK;
}

static Status parseClassDeclaration(Parser *parser) {
  ConstID classNameID;
  VariableDeclaration classVariable;
  StringSlice className;
  ClassInfo classInfo;

  classInfo.enclosing = parser->classInfo;
  classInfo.hasSuperClass = UFALSE;
  parser->classInfo = &classInfo;

  EXPECT(TOKEN_CLASS);
  EXPECT(TOKEN_IDENTIFIER);
  ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&classNameID);
  className = SLICE_PREVIOUS();
  EMIT1C(OP_CLASS, classNameID);
  CHECK3(declareVariable, className, UTRUE, &classVariable);
  CHECK1(defineVariable, &classVariable);

  CHECK(parseTypeParameters);

  if (AT(TOKEN_LEFT_PAREN)) {
    ADVANCE();
    if (!AT(TOKEN_RIGHT_PAREN)) {
      VariableDeclaration superVariable;
      CHECK(parseExpression);
      classInfo.hasSuperClass = UTRUE;
      CHECK(beginScope);
      CHECK3(declareVariable, newSlice("super", 5), UFALSE, &superVariable);
      CHECK1(defineVariable, &superVariable);
      CHECK1(loadVariableByName, className);
      EMIT1(OP_INHERIT);
    }
    EXPECT(TOKEN_RIGHT_PAREN);
  }

  CHECK1(loadVariableByName, className);
  EXPECT(TOKEN_COLON);
  while (AT(TOKEN_NEWLINE)) {
    ADVANCE();
  }
  EXPECT(TOKEN_INDENT);
  while (AT(TOKEN_NEWLINE)) {
    ADVANCE();
  }
  if (AT(TOKEN_STRING) || AT(TOKEN_RAW_STRING)) { /* comments */
    ADVANCE();
    while (AT(TOKEN_NEWLINE)) {
      ADVANCE();
    }
  }

  if (AT(TOKEN_PASS)) {
    ADVANCE();
    while (AT(TOKEN_NEWLINE)) {
      ADVANCE();
    }
  }

  while (AT(TOKEN_STATIC)) {
    CHECK(parseStaticMethodDeclaration);
  }

  while (AT(TOKEN_VAR) || AT(TOKEN_FINAL)) {
    CHECK(parseFieldDeclaration);
  }

  while (AT(TOKEN_DEF)) {
    CHECK(parseMethodDeclaration);
    while (AT(TOKEN_NEWLINE)) {
      ADVANCE();
    }
  }

  EXPECT(TOKEN_DEDENT);
  EMIT1(OP_POP); /* class (from loadVariableByName) */

  if (classInfo.hasSuperClass) {
    CHECK(endScope);
  }

  parser->classInfo = classInfo.enclosing;

  return STATUS_OK;
}

static Status parseTraitDeclaration(Parser *parser) {
  /* Traits have no runtime effect. So we skip them */
  EXPECT(TOKEN_TRAIT);
  while (!AT(TOKEN_EOF) && !AT(TOKEN_INDENT)) {
    ADVANCE();
  }
  if (AT(TOKEN_INDENT)) {
    i32 depth = 1;
    ADVANCE();
    while (!AT(TOKEN_EOF) && depth > 0) {
      switch (parser->current.type) {
        case TOKEN_INDENT:
          depth++;
          break;
        case TOKEN_DEDENT:
          depth--;
          break;
        default:
          break;
      }
      ADVANCE();
    }
  }
  return STATUS_OK;
}

static Status parseDefaultArgument(Parser *parser, DefaultArgument *out) {
  switch (parser->current.type) {
    case TOKEN_NIL:
      ADVANCE();
      out->type = DEFARG_NIL;
      return STATUS_OK;
    case TOKEN_TRUE:
      ADVANCE();
      out->type = DEFARG_TRUE;
      return STATUS_OK;
    case TOKEN_FALSE:
      ADVANCE();
      out->type = DEFARG_FALSE;
      return STATUS_OK;
    case TOKEN_NUMBER:
      ADVANCE();
      out->type = DEFARG_NUMBER;
      out->as.number = strtod(parser->previous.start, NULL);
      return STATUS_OK;
    case TOKEN_MINUS:
      ADVANCE();
      if (AT(TOKEN_NUMBER)) {
        ADVANCE();
        out->type = DEFARG_NUMBER;
        out->as.number = -strtod(parser->previous.start, NULL);
        return STATUS_OK;
      } else {
        runtimeError(
            "[%s:%d] Expected number (for negation of) default argument expression but got %s",
            MODULE_NAME_CHARS,
            CURRENT_LINE,
            tokenTypeToName(parser->current.type));
        return STATUS_ERROR;
      }
    case TOKEN_STRING:
      ADVANCE();
      out->type = DEFARG_STRING;
      out->as.string = parser->previous;
      return STATUS_OK;
    default:
      break;
  }
  runtimeError(
      "[%s:%d] Expected default argument expression but got %s",
      MODULE_NAME_CHARS,
      CURRENT_LINE,
      tokenTypeToName(parser->current.type));
  return STATUS_OK;
}

static Status defaultArgumentToValue(Parser *parser, DefaultArgument defArg, Value *out) {
  switch (defArg.type) {
    case DEFARG_NIL:
      *out = valNil();
      break;
    case DEFARG_FALSE:
      *out = valBool(UFALSE);
      break;
    case DEFARG_TRUE:
      *out = valBool(UTRUE);
      break;
    case DEFARG_NUMBER:
      *out = valNumber(defArg.as.number);
      break;
    case DEFARG_STRING: {
      String *string;
      CHECK2(stringTokenToObjString, &defArg.as.string, &string);
      *out = valString(string);
      break;
    }
    default:
      panic("Invalid DefaultArgumentType %d", defArg.type);
  }
  return STATUS_OK;
}

static Status parseParameterList(Parser *parser, ObjThunk *thunk) {
  i16 argc = 0, defArgc = 0;
  String *parameterNames[MAX_ARG_COUNT];
  DefaultArgument defArgs[MAX_ARG_COUNT];

  EXPECT(TOKEN_LEFT_PAREN);
  while (!AT(TOKEN_RIGHT_PAREN)) {
    Local *local;
    if (argc >= MAX_ARG_COUNT) {
      runtimeError("[%s:%d] Too many parameters in function",
                   MODULE_NAME_CHARS, CURRENT_LINE);
      return STATUS_ERROR;
    }
    argc++;
    EXPECT(TOKEN_IDENTIFIER);
    parameterNames[argc - 1] = internForeverString(
        parser->previous.start,
        parser->previous.length);
    CHECK2(newLocal, SLICE_PREVIOUS(), &local);
    MARK_LOCAL_READY(local); /* all arguments are initialized at the start */

    /* argc and defaultArgc must not go over U8_MAX, but newLocal already limits the number
     * of local variables to that amount anyway, so there is no real need to do
     * a separate check. */
    if (maybeAtTypeExpression(parser)) {
      CHECK(parseTypeExpression);
    }
    if (defArgc > 0 && !AT(TOKEN_EQUAL)) {
      runtimeError(
          "[%s:%d] Non-optional arguments may not follow any optional arguments",
          MODULE_NAME_CHARS,
          PREVIOUS_LINE);
      return STATUS_ERROR;
    }
    if (AT(TOKEN_EQUAL)) {
      /* NOTE: defArgc must be less than MAX_ARG_COUNT here, but
       * we don't need to check that because argc must always be
       * greater than or equal to defArgc, and we check that argc
       * is less than MAX_ARG_COUNT at the top of this loop */
      ADVANCE();
      defArgc++;
      CHECK1(parseDefaultArgument, &defArgs[defArgc - 1]);
    }
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }
  EXPECT(TOKEN_RIGHT_PAREN);

  /* NOTE: we assign `parameterNames` and `defaultArgs` before
   * `defaultArgsCount` and `arity` because ALLOCATE can trigger
   * a GC, and we do not want e.g. `arity` to be non-zero when
   * `parameterNames` is still `NULL` */
  thunk->parameterNames = ALLOCATE(String *, argc);
  thunk->defaultArgs = ALLOCATE(Value, defArgc);
  thunk->defaultArgsCount = defArgc;
  thunk->arity = argc;
  {
    i16 i;
    for (i = 0; i < argc; i++) {
      thunk->parameterNames[i] = parameterNames[i];
    }
    for (i = 0; i < defArgc; i++) {
      if (!defaultArgumentToValue(parser, defArgs[i], &thunk->defaultArgs[i])) {
        return STATUS_ERROR;
      }
    }
  }
  return STATUS_OK;
}

/* Parses a function starting from the argument list and leaves a closure on the top of the satck */
static Status parseFunctionCore(Parser *parser, StringSlice name, ThunkContext *thunkContext) {
  Environment env;
  ObjThunk *thunk;
  i16 i;
  ConstID thunkID;

  /* get the type parameters out of the way, if any */
  CHECK(parseTypeParameters);

  /* Push new Thunk and Environment for this function */
  thunk = newThunk();
  thunk->moduleName = THUNK->moduleName;
  initEnvironment(&env, thunk, thunkContext);
  env.enclosing = ENV;
  parser->env = &env;
  thunk->name = internString(name.chars, name.length);

  /* parameter list */
  CHECK1(parseParameterList, thunk);

  if (maybeAtTypeExpression(parser)) {
    CHECK(parseTypeExpression);
  }

  /* Function body */
  EXPECT(TOKEN_COLON);
  if (thunkContext->isLambda) {
    CHECK(parseExpression);
    EMIT1(OP_RETURN);
  } else {
    CHECK1(parseBlock, UFALSE);

    /* fallthrough return */
    if (thunkContext->isInitializer) {
      EMIT3(OP_GET_LOCAL, 0, OP_RETURN); /* return 'this' */
    } else {
      EMIT2(OP_NIL, OP_RETURN);
    }
  }

  /* Pop the Thunk and Environment for this function */
  parser->env = parser->env->enclosing;
  ADD_CONST_VALUE(valThunk(thunk), &thunkID);

  EMIT1C(OP_CLOSURE, thunkID);
  for (i = 0; i < thunk->upvalueCount; i++) {
    EMIT1(env.upvalues[i].isLocal ? 1 : 0);
    EMIT1(env.upvalues[i].index);
  }

  return STATUS_OK;
}

static Status parseDecoratorApplication(Parser *parser) {
  ThunkContext thunkContext;
  ubool methodCall = UFALSE;
  ConstID nameID;
  EXPECT(TOKEN_AT);
  CHECK1(parsePrec, PREC_PRIMARY);
  if (AT(TOKEN_DOT)) {
    methodCall = UTRUE;
    ADVANCE();
    EXPECT(TOKEN_IDENTIFIER);
    ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&nameID);
  }
  EXPECT_STATEMENT_DELIMITER();
  EXPECT(TOKEN_DEF);
  initThunkContext(&thunkContext);
  CHECK2(parseFunctionCore, newSlice("<def>", 5), &thunkContext);
  if (methodCall) {
    EMIT1C1(OP_INVOKE, nameID, 1);
  } else {
    /* Otherwise, do a function call */
    EMIT2(OP_CALL, 1);
  }
  return STATUS_OK;
}

static Status parseFunctionDeclaration(Parser *parser) {
  VariableDeclaration variable;
  ThunkContext thunkContext;

  initThunkContext(&thunkContext);

  EXPECT(TOKEN_DEF);
  EXPECT(TOKEN_IDENTIFIER);

  CHECK3(declareVariable, SLICE_PREVIOUS(), UTRUE, &variable);

  CHECK2(parseFunctionCore, SLICE_PREVIOUS(), &thunkContext);

  CHECK1(defineVariable, &variable);

  return STATUS_OK;
}

static Status parseVariableDeclaration(Parser *parser) {
  VariableDeclaration variable;
  ubool fin = AT(TOKEN_FINAL);

  EXPECT(fin ? TOKEN_FINAL : TOKEN_VAR);
  EXPECT(TOKEN_IDENTIFIER);

  CHECK3(declareVariable, SLICE_PREVIOUS(), UFALSE, &variable);

  /* Type annotation */
  if (maybeAtTypeExpression(parser)) {
    CHECK(parseTypeExpression);
  }

  /* Documentation */
  if (AT(TOKEN_STRING) || AT(TOKEN_RAW_STRING)) {
    ADVANCE();
  }

  EXPECT(TOKEN_EQUAL);

  CHECK(parseExpression);

  EXPECT_STATEMENT_DELIMITER();

  CHECK1(defineVariable, &variable);

  return STATUS_OK;
}

static ParseRule *getRule(TokenType type) {
  return &rules[type];
}

static Status parsePrec(Parser *parser, Precedence prec) {
  ParseFn prefixRule;
  prefixRule = getRule(parser->current.type)->prefix;
  if (prefixRule == NULL) {
    runtimeError(
        "[%s:%d] Expected expression but got %s",
        MODULE_NAME_CHARS,
        PREVIOUS_LINE,
        tokenTypeToName(parser->current.type));
    return STATUS_ERROR;
  }
  CHECK(prefixRule);

  while (prec <= getRule(parser->current.type)->precedence) {
    ParseFn infixRule;
    infixRule = getRule(parser->current.type)->infix;
    CHECK(infixRule);
  }

  return STATUS_OK;
}

static Status parseExpression(Parser *parser) {
  return parsePrec(parser, PREC_OR);
}

static Status parseRawStringLiteral(Parser *parser) {
  EXPECT(TOKEN_RAW_STRING);
  {
    char quote = parser->previous.start[1];
    if (quote == parser->previous.start[2] &&
        quote == parser->previous.start[3]) {
      EMIT_CONST(valString(internString(
          parser->previous.start + 4,
          parser->previous.length - 7)));
    } else {
      EMIT_CONST(valString(internString(
          parser->previous.start + 2,
          parser->previous.length - 3)));
    }
  }
  return STATUS_OK;
}

static Status stringTokenToObjString(Parser *parser, Token *token, String **out) {
  size_t quoteLen;
  char quoteChar = token->start[0];
  char quoteStr[4];
  StringBuilder sb;

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

  initStringBuilder(&sb);
  if (!unescapeString(&sb, token->start + quoteLen, quoteStr, quoteLen)) {
    return STATUS_ERROR;
  }

  *out = sbstring(&sb);

  freeStringBuilder(&sb);

  return STATUS_OK;
}

static Status lastStringTokenToObjString(Parser *parser, String **out) {
  return stringTokenToObjString(parser, &parser->previous, out);
}

static Status parseStringLiteral(Parser *parser) {
  String *str;

  EXPECT(TOKEN_STRING);

  if (!lastStringTokenToObjString(parser, &str)) {
    return STATUS_ERROR;
  }

  EMIT_CONST(valString(str));
  return STATUS_OK;
}

static Status parseNumber(Parser *parser) {
  double value;
  EXPECT(TOKEN_NUMBER);
  value = strtod(parser->previous.start, NULL);
  EMIT_CONST(valNumber(value));
  return STATUS_OK;
}

static Status parseNumberHex(Parser *parser) {
  double value = 0;
  size_t i, len;
  EXPECT(TOKEN_NUMBER_HEX);
  len = parser->previous.length;
  for (i = 2; i < len; i++) {
    char ch = parser->previous.start[i];
    value *= 16;
    if ('0' <= ch && ch <= '9') {
      value += ch - '0';
    } else if ('A' <= ch && ch <= 'F') {
      value += ch - 'A' + 10;
    } else if ('a' <= ch && ch <= 'f') {
      value += ch - 'a' + 10;
    } else {
      runtimeError("[%s:%d] Invalid hex digit %c", MODULE_NAME_CHARS, PREVIOUS_LINE, ch);
      return STATUS_ERROR;
    }
  }
  EMIT_CONST(valNumber(value));
  return STATUS_OK;
}

static Status parseNumberBin(Parser *parser) {
  double value = 0;
  size_t i, len;
  EXPECT(TOKEN_NUMBER_BIN);
  len = parser->previous.length;
  for (i = 2; i < len; i++) {
    char ch = parser->previous.start[i];
    value *= 2;
    if (ch == '1' || ch == '0') {
      value += ch - '0';
    } else {
      runtimeError("[%s:%d] Invalid binary digit %c", MODULE_NAME_CHARS, PREVIOUS_LINE, ch);
      return STATUS_ERROR;
    }
  }
  EMIT_CONST(valNumber(value));
  return STATUS_OK;
}

static Status resolveLocalForEnv(
    Parser *parser, Environment *env, StringSlice name, i16 *out) {
  i16 i;
  for (i = 0; i < env->localsCount; i++) {
    Local *local = &env->locals[i];
    if (local->name.length == name.length &&
        memcmp(local->name.chars, name.chars, name.length) == 0) {
      if (local->scopeDepth == -1) {
        runtimeError(
            "[%s:%d] Reading a local variable in its own initializer is not allowed",
            MODULE_NAME_CHARS,
            PREVIOUS_LINE);
        return STATUS_ERROR;
      }
      *out = i;
      return STATUS_OK;
    }
  }
  *out = -1;
  return STATUS_OK;
}

/* Looks for a local variable matching the given name and returns the locals slot index.
 * If not found, out will be set to -1. */
static Status resolveLocal(Parser *parser, StringSlice name, i16 *out) {
  return resolveLocalForEnv(parser, ENV, name, out);
}

static Status addUpvalue(
    Parser *parser, Environment *env, i16 enclosingIndex, ubool isLocal, i16 *upvalueIndex) {
  i16 i, upvalueCount = env->thunk->upvalueCount;
  for (i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &env->upvalues[i];
    if (upvalue->isLocal == isLocal && upvalue->index == enclosingIndex) {
      *upvalueIndex = i;
      return STATUS_OK;
    }
  }

  if (upvalueCount >= U8_COUNT) {
    runtimeError(
        "[%s:%d] Too many closure variables in thunk",
        MODULE_NAME_CHARS, PREVIOUS_LINE);
    return STATUS_ERROR;
  }

  env->upvalues[upvalueCount].isLocal = isLocal;
  env->upvalues[upvalueCount].index = enclosingIndex;
  *upvalueIndex = env->thunk->upvalueCount++;
  return STATUS_OK;
}

static Status resolveUpvalueForEnv(
    Parser *parser, Environment *env, StringSlice name, i16 *outUpvalueIndex) {
  i16 parentLocalIndex, parentUpvalueIndex;
  if (env->enclosing == NULL) {
    *outUpvalueIndex = -1;
    return STATUS_OK;
  }

  /* Check if the upvalue we are looking for is a local variable in the
   * immediate enclosing environment */
  CHECK3(resolveLocalForEnv, env->enclosing, name, &parentLocalIndex);
  if (parentLocalIndex != -1) {
    env->enclosing->locals[parentLocalIndex].isCaptured = UTRUE;
    CHECK4(addUpvalue, env, parentLocalIndex, UTRUE, outUpvalueIndex);
    return STATUS_OK;
  }

  /* If it is not, extend the search to environments that enclose the environment that
   * encloses this env */
  CHECK3(resolveUpvalueForEnv, env->enclosing, name, &parentUpvalueIndex);
  if (parentUpvalueIndex != -1) {
    CHECK4(addUpvalue, env, parentUpvalueIndex, UFALSE, outUpvalueIndex);
    return STATUS_OK;
  }

  *outUpvalueIndex = -1;
  return STATUS_OK;
}

/* Like resolveLocal, but will instead check the enclosing environment for an
 * upvalue */
static Status resolveUpvalue(Parser *parser, StringSlice name, i16 *out) {
  return resolveUpvalueForEnv(parser, ENV, name, out);
}

/* Emits opcode to load the value of a variable on the stack */
static Status loadVariableByName(Parser *parser, StringSlice name) {
  i16 localIndex;
  CHECK2(resolveLocal, name, &localIndex);
  if (localIndex != -1) {
    EMIT2(OP_GET_LOCAL, (u8)localIndex);
  } else {
    i16 upvalueIndex;
    CHECK2(resolveUpvalue, name, &upvalueIndex);
    if (upvalueIndex != -1) {
      EMIT2(OP_GET_UPVALUE, (u8)upvalueIndex);
    } else {
      ConstID nameID;
      ADD_CONST_SLICE(name, &nameID);
      EMIT1C(OP_GET_GLOBAL, nameID);
    }
  }
  return STATUS_OK;
}

/* Emits opcode to pop TOS and store it in the variable */
static Status storeVariableByName(Parser *parser, StringSlice name) {
  i16 localIndex;
  CHECK2(resolveLocal, name, &localIndex);
  if (localIndex != -1) {
    EMIT2(OP_SET_LOCAL, (u8)localIndex);
  } else {
    i16 upvalueIndex;
    CHECK2(resolveUpvalue, name, &upvalueIndex);
    if (upvalueIndex != -1) {
      EMIT2(OP_SET_UPVALUE, (u8)upvalueIndex);
    } else {
      ConstID nameID;
      ADD_CONST_SLICE(name, &nameID);
      EMIT1C(OP_SET_GLOBAL, nameID);
    }
  }
  return STATUS_OK;
}

static Status parseName(Parser *parser, ubool canAssign) {
  StringSlice name;

  EXPECT(TOKEN_IDENTIFIER);
  name = SLICE_PREVIOUS();

  if (canAssign && AT(TOKEN_EQUAL)) {
    ADVANCE();
    CHECK(parseExpression);
    CHECK1(storeVariableByName, name);
  } else {
    CHECK1(loadVariableByName, name);
  }

  return STATUS_OK;
}

static Status parseNameWithAssignment(Parser *parser) {
  return parseName(parser, UTRUE);
}

static Status parseThis(Parser *parser) {
  EXPECT(TOKEN_THIS);
  CHECK1(loadVariableByName, newSlice("this", 4));
  return STATUS_OK;
}

static Status parseSuper(Parser *parser) {
  ConstID methodNameID;
  u8 argCount;
  ubool hasKwArgs;
  EXPECT(TOKEN_SUPER);
  if (!parser->classInfo || !parser->classInfo->hasSuperClass) {
    runtimeError("'super' cannot be used outside a class with a super class");
    return STATUS_ERROR;
  }
  EXPECT(TOKEN_DOT);
  EXPECT(TOKEN_IDENTIFIER);
  ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&methodNameID);
  CHECK1(loadVariableByName, newSlice("this", 4));
  CHECK2(parseArgumentList, &argCount, &hasKwArgs);
  if (hasKwArgs) {
    runtimeError("Keyword arguments are not supported here");
    return STATUS_ERROR;
  }
  CHECK1(loadVariableByName, newSlice("super", 5));
  EMIT1C1(OP_SUPER_INVOKE, methodNameID, argCount);
  return STATUS_OK;
}

static Status parseLambda(Parser *parser) {
  ThunkContext thunkContext;
  initThunkContext(&thunkContext);
  thunkContext.isLambda = UTRUE;
  EXPECT(TOKEN_DEF);
  CHECK2(parseFunctionCore, newSlice("<lambda>", 8), &thunkContext);
  return STATUS_OK;
}

static Status parseRaise(Parser *parser) {
  EXPECT(TOKEN_RAISE);
  CHECK(parseExpression);
  EMIT1(OP_RAISE);
  return STATUS_OK;
}

static Status parseLiteral(Parser *parser) {
  ADVANCE();
  switch (parser->previous.type) {
    case TOKEN_FALSE:
      EMIT1(OP_FALSE);
      break;
    case TOKEN_NIL:
      EMIT1(OP_NIL);
      break;
    case TOKEN_TRUE:
      EMIT1(OP_TRUE);
      break;
    default:
      assertionError("parseLiteral");
  }
  return STATUS_OK;
}

static Status parseGrouping(Parser *parser) {
  EXPECT(TOKEN_LEFT_PAREN);
  CHECK(parseExpression);
  EXPECT(TOKEN_RIGHT_PAREN);
  return STATUS_OK;
}

static Status parseUnary(Parser *parser) {
  TokenType operatorType = parser->current.type;
  ADVANCE(); /* operator */

  /* compile the operand */
  CHECK1(parsePrec, (operatorType == TOKEN_NOT ? PREC_NOT : PREC_UNARY));

  /* Emit the operator instruction */
  switch (operatorType) {
    case TOKEN_TILDE:
      EMIT1(OP_BITWISE_NOT);
      break;
    case TOKEN_NOT:
      EMIT1(OP_NOT);
      break;
    case TOKEN_MINUS:
      EMIT1(OP_NEGATE);
      break;
    default:
      assertionError("parseUnary");
  }

  return STATUS_OK;
}

static Status parseListDisplayBody(Parser *parser, u8 *out) {
  u8 length = 0;
  for (;;) {
    if (AT(TOKEN_RIGHT_BRACKET)) {
      break;
    }
    CHECK(parseExpression);
    if (length == U8_MAX) {
      runtimeError(
          "[%s:%d] The number of items in a list display cannot exceed %d",
          MODULE_NAME_CHARS,
          PREVIOUS_LINE,
          U8_MAX);
      return STATUS_ERROR;
    }
    length++;
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }
  *out = length;
  return STATUS_OK;
}

static Status parseListDisplay(Parser *parser) {
  u8 length;
  EXPECT(TOKEN_LEFT_BRACKET);
  CHECK1(parseListDisplayBody, &length);
  EXPECT(TOKEN_RIGHT_BRACKET);
  EMIT2(OP_NEW_LIST, length);
  return STATUS_OK;
}

static Status parseMapDisplayBody(Parser *parser, u8 *out) {
  u8 length = 0;

  for (;;) {
    if (AT(TOKEN_RIGHT_BRACE)) {
      break;
    }

    CHECK(parseExpression);
    if (AT(TOKEN_COLON)) {
      ADVANCE();
      CHECK(parseExpression);
    } else {
      /* If ':' is omitted, the value is implied to be nil */
      EMIT1(OP_NIL);
    }

    if (length == U8_MAX) {
      runtimeError(
          "[%s:%d] The number of pairs in a map display cannot exceed %d",
          MODULE_NAME_CHARS,
          PREVIOUS_LINE,
          U8_MAX);
      return STATUS_ERROR;
    }
    length++;
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }

  *out = length;
  return STATUS_OK;
}

static Status parseMapDisplay(Parser *parser) {
  u8 length;
  EXPECT(TOKEN_LEFT_BRACE);
  CHECK1(parseMapDisplayBody, &length);
  EXPECT(TOKEN_RIGHT_BRACE);
  EMIT2(OP_NEW_DICT, length);
  return STATUS_OK;
}

static Status parseFrozenDisplay(Parser *parser) {
  u8 length;
  EXPECT(TOKEN_FINAL);
  if (AT(TOKEN_LEFT_BRACE)) {
    ADVANCE();
    CHECK1(parseMapDisplayBody, &length);
    EXPECT(TOKEN_RIGHT_BRACE);
    EMIT2(OP_NEW_FROZEN_DICT, length);
  } else {
    EXPECT(TOKEN_LEFT_BRACKET);
    CHECK1(parseListDisplayBody, &length);
    EXPECT(TOKEN_RIGHT_BRACKET);
    EMIT2(OP_NEW_FROZEN_LIST, length);
    return STATUS_OK;
  }
  return STATUS_OK;
}

/* A hack to do a sort of lookahead and check whether the current
 * token is followed by '=' */
static ubool peekEqual(Parser *parser) {
  const char *ptr = parser->lexer->current;
  while (*ptr == ' ') {
    ptr++;
  }
  return *ptr == '=';
}

static Status parseArgumentList(Parser *parser, u8 *out, ubool *hasKwArgs) {
  u8 argCount = 0;
  u8 kwargc = 0;

  EXPECT(TOKEN_LEFT_PAREN);
  while (!AT(TOKEN_RIGHT_PAREN)) {
    if (argCount == U8_MAX) {
      runtimeError(
          "[%s:%d] Too many arguments (no more than %d are allowed)",
          MODULE_NAME_CHARS,
          PREVIOUS_LINE,
          U8_MAX);
      return STATUS_ERROR;
    }
    if (AT(TOKEN_IDENTIFIER) && peekEqual(parser)) {
      ADVANCE();
      EMIT_CONST(valString(internString(
          parser->previous.start,
          parser->previous.length)));
      EXPECT(TOKEN_EQUAL);
      kwargc++;
    } else if (kwargc > 0) {
      runtimeError(
          "[%s:%d] Positional arguments may not come after keyword arguments",
          MODULE_NAME_CHARS,
          PREVIOUS_LINE);
      return STATUS_ERROR;
    }
    CHECK(parseExpression);
    argCount++;
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }
  EXPECT(TOKEN_RIGHT_PAREN);

  if (kwargc > 0) {
    EMIT2(OP_NEW_DICT, kwargc);
  }

  *out = argCount - kwargc;
  *hasKwArgs = kwargc > 0;
  return STATUS_OK;
}

static Status parseFunctionCall(Parser *parser) {
  u8 argCount;
  ubool hasKwArgs;
  CHECK2(parseArgumentList, &argCount, &hasKwArgs);
  if (hasKwArgs) {
    EMIT2(OP_CALL_KW, argCount);
  } else {
    EMIT2(OP_CALL, argCount);
  }
  return STATUS_OK;
}

static Status parseSubscript(Parser *parser) {
  EXPECT(TOKEN_LEFT_BRACKET);
  if (AT(TOKEN_COLON)) {
    /* Implicit nil when the first argument is missing */
    EMIT1(OP_NIL);
  } else {
    CHECK(parseExpression); /* index */
  }

  if (AT(TOKEN_COLON)) {
    ConstID nameID;
    ADVANCE();
    ADD_CONST_STRING(vm.sliceString, &nameID);
    if (AT(TOKEN_RIGHT_BRACKET)) {
      EMIT1(OP_NIL); /* Implicit nil second argument, if missing */
    } else {
      CHECK(parseExpression);
    }
    EXPECT(TOKEN_RIGHT_BRACKET);
    EMIT1C1(OP_INVOKE, nameID, 2);
  } else {
    EXPECT(TOKEN_RIGHT_BRACKET);
    if (AT(TOKEN_EQUAL)) {
      ConstID nameID;
      ADVANCE();
      ADD_CONST_STRING(vm.setitemString, &nameID);
      CHECK(parseExpression);
      EMIT1C1(OP_INVOKE, nameID, 2);
    } else {
      ConstID nameID;
      ADD_CONST_STRING(vm.getitemString, &nameID);
      EMIT1C1(OP_INVOKE, nameID, 1);
    }
  }

  return STATUS_OK;
}

static Status parseAs(Parser *parser) {
  /* 'as' expressions have zero runtime effect */
  EXPECT(TOKEN_AS);
  return parseTypeExpression(parser);
}

static Status parseDot(Parser *parser) {
  ConstID nameID;

  EXPECT(TOKEN_DOT);
  EXPECT(TOKEN_IDENTIFIER);
  ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&nameID);

  if (AT(TOKEN_EQUAL)) {
    ADVANCE();
    CHECK(parseExpression);
    EMIT1C(OP_SET_FIELD, nameID);
  } else if (AT(TOKEN_LEFT_PAREN)) {
    u8 argCount;
    ubool hasKwArgs;
    CHECK2(parseArgumentList, &argCount, &hasKwArgs);
    if (hasKwArgs) {
      EMIT1C1(OP_INVOKE_KW, nameID, argCount);
    } else {
      EMIT1C1(OP_INVOKE, nameID, argCount);
    }
  } else {
    EMIT1C(OP_GET_FIELD, nameID);
  }

  return STATUS_OK;
}

static Status parseAnd(Parser *parser) {
  i32 endJump;
  EXPECT(TOKEN_AND);
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &endJump);
  EMIT1(OP_POP);
  CHECK1(parsePrec, PREC_AND);
  CHECK1(patchJump, endJump);
  return STATUS_OK;
}

static Status parseOr(Parser *parser) {
  i32 elseJump;
  i32 endJump;
  EXPECT(TOKEN_OR);
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &elseJump);
  CHECK2(emitJump, OP_JUMP, &endJump);
  CHECK1(patchJump, elseJump);
  EMIT1(OP_POP);
  CHECK1(parsePrec, PREC_OR);
  CHECK1(patchJump, endJump);
  return STATUS_OK;
}

static Status parseBinary(Parser *parser) {
  TokenType operatorType = parser->current.type;
  ParseRule *rule = getRule(operatorType);
  ubool isNot = UFALSE, notIn = UFALSE;
  ubool rightAssociative = (operatorType == TOKEN_STAR_STAR);

  ADVANCE(); /* operator */

  if (operatorType == TOKEN_IS && AT(TOKEN_NOT)) {
    ADVANCE();
    isNot = UTRUE;
  } else if (operatorType == TOKEN_NOT) {
    EXPECT(TOKEN_IN);
    notIn = UTRUE;
    operatorType = TOKEN_IN;
  }

  CHECK1(parsePrec, rightAssociative ? rule->precedence : (Precedence)(rule->precedence + 1));

  switch (operatorType) {
    case TOKEN_IS:
      EMIT1(OP_IS);
      if (isNot) EMIT1(OP_NOT);
      break;
    case TOKEN_BANG_EQUAL:
      EMIT2(OP_EQUAL, OP_NOT);
      break;
    case TOKEN_EQUAL_EQUAL:
      EMIT1(OP_EQUAL);
      break;
    case TOKEN_GREATER:
      EMIT1(OP_GREATER);
      break;
    case TOKEN_GREATER_EQUAL:
      EMIT2(OP_LESS, OP_NOT);
      break;
    case TOKEN_LESS:
      EMIT1(OP_LESS);
      break;
    case TOKEN_LESS_EQUAL:
      EMIT2(OP_GREATER, OP_NOT);
      break;
    case TOKEN_IN:
      EMIT1(OP_IN);
      if (notIn) EMIT1(OP_NOT);
      break;
    case TOKEN_PLUS:
      EMIT1(OP_ADD);
      break;
    case TOKEN_MINUS:
      EMIT1(OP_SUBTRACT);
      break;
    case TOKEN_STAR:
      EMIT1(OP_MULTIPLY);
      break;
    case TOKEN_SLASH:
      EMIT1(OP_DIVIDE);
      break;
    case TOKEN_SLASH_SLASH:
      EMIT1(OP_FLOOR_DIVIDE);
      break;
    case TOKEN_PERCENT:
      EMIT1(OP_MODULO);
      break;
    case TOKEN_STAR_STAR:
      EMIT1(OP_POWER);
      break;
    case TOKEN_SHIFT_LEFT:
      EMIT1(OP_SHIFT_LEFT);
      break;
    case TOKEN_SHIFT_RIGHT:
      EMIT1(OP_SHIFT_RIGHT);
      break;
    case TOKEN_PIPE:
      EMIT1(OP_BITWISE_OR);
      break;
    case TOKEN_AMPERSAND:
      EMIT1(OP_BITWISE_AND);
      break;
    case TOKEN_CARET:
      EMIT1(OP_BITWISE_XOR);
      break;
    default:
      assertionError("parseBinary");
  }

  return STATUS_OK;
}

static Status parseConditional(Parser *parser) {
  i32 elseJump, endJump;

  /* Condition */
  EXPECT(TOKEN_IF);
  CHECK(parseExpression); /* condition */
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &elseJump);

  /* Left Branch */
  EXPECT(TOKEN_THEN);
  EMIT1(OP_POP);          /* pop condition */
  CHECK(parseExpression); /* evaluate left branch */
  CHECK2(emitJump, OP_JUMP, &endJump);

  /* Right Branch */
  EXPECT(TOKEN_ELSE);
  CHECK1(patchJump, elseJump);
  EMIT1(OP_POP);          /* pop condition */
  CHECK(parseExpression); /* evaluate right branch */
  CHECK1(patchJump, endJump);

  return STATUS_OK;
}

static Status parseBlock(Parser *parser, ubool newScope) {
  ubool atLeastOneDeclaration = UFALSE;
  if (newScope) {
    CHECK(beginScope);
  }

  while (AT(TOKEN_NEWLINE)) {
    ADVANCE();
  }
  EXPECT(TOKEN_INDENT);
  while (AT(TOKEN_NEWLINE)) {
    ADVANCE();
  }
  while (!AT(TOKEN_DEDENT) && !AT(TOKEN_EOF)) {
    atLeastOneDeclaration = UTRUE;
    CHECK(parseDeclaration);
    while (AT(TOKEN_NEWLINE)) {
      ADVANCE();
    }
  }
  EXPECT(TOKEN_DEDENT);

  if (!atLeastOneDeclaration) {
    runtimeError(
        "[%s:%d] Blocks require at least one declaration, but got none",
        MODULE_NAME_CHARS,
        PREVIOUS_LINE);
    return STATUS_ERROR;
  }

  if (newScope) {
    CHECK(endScope);
  }

  return STATUS_OK;
}

static Status parseForInStatement(Parser *parser) {
  i32 jump, loopStart;
  VariableDeclaration itemVariable, iterator;
  StringSlice itemVariableName;

  EXPECT(TOKEN_FOR);

  CHECK(beginScope); /* also ensures inGlobalScope will always be false */

  EXPECT(TOKEN_IDENTIFIER);
  itemVariableName = SLICE_PREVIOUS();

  EXPECT(TOKEN_IN);
  CHECK(parseExpression); /* iterable */
  EMIT1(OP_GET_ITER);     /* replace the iterable with an iterator */
  CHECK3(declareVariable, newSlice("@iterator", 9), UTRUE, &iterator);
  CHECK1(defineVariable, &iterator);
  loopStart = CHUNK_POS;
  EMIT1(OP_GET_NEXT); /* gets next value returned by the iterator */
  CHECK2(emitJump, OP_JUMP_IF_STOP_ITERATION, &jump);

  CHECK(beginScope);
  CHECK3(declareVariable, itemVariableName, UTRUE, &itemVariable);
  CHECK1(defineVariable, &itemVariable);
  EXPECT(TOKEN_COLON);
  CHECK1(parseBlock, UFALSE);
  CHECK(endScope);

  CHECK1(emitLoop, loopStart);
  CHECK1(patchJump, jump);
  EMIT1(OP_POP); /* StopIteration */

  CHECK(endScope);
  return STATUS_OK;
}

static Status parseIfStatement(Parser *parser) {
  i32 thenJump;
  i32 endJumps[MAX_ELIF_CHAIN_COUNT], endJumpsCount = 0;

  EXPECT(TOKEN_IF);
  CHECK(parseExpression);
  EXPECT(TOKEN_COLON);
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &thenJump);
  EMIT1(OP_POP); /* condition (if) */
  CHECK1(parseBlock, UTRUE);
  CHECK2(emitJump, OP_JUMP, &endJumps[endJumpsCount++]);
  CHECK1(patchJump, thenJump);
  EMIT1(OP_POP); /* condition (if) */

  while (AT(TOKEN_ELIF)) {
    ADVANCE();
    CHECK(parseExpression);
    EXPECT(TOKEN_COLON);
    CHECK2(emitJump, OP_JUMP_IF_FALSE, &thenJump);
    EMIT1(OP_POP); /* condition (elif) */
    CHECK1(parseBlock, UTRUE);
    if (endJumpsCount >= MAX_ELIF_CHAIN_COUNT) {
      runtimeError(
          "[%s:%d] Too many chained 'elif' clauses",
          MODULE_NAME_CHARS, PREVIOUS_LINE);
      return STATUS_ERROR;
    }
    CHECK2(emitJump, OP_JUMP, &endJumps[endJumpsCount++]);
    CHECK1(patchJump, thenJump);
    EMIT1(OP_POP); /* condition (elif) */
  }

  if (AT(TOKEN_ELSE)) {
    ADVANCE();
    EXPECT(TOKEN_COLON);
    CHECK1(parseBlock, UTRUE);
  }

  {
    i32 i;
    for (i = 0; i < endJumpsCount; i++) {
      CHECK1(patchJump, endJumps[i]);
    }
  }

  return STATUS_OK;
}

static Status parseReturnStatement(Parser *parser) {
  ubool isInitializer = THUNK_CONTEXT->isInitializer;

  EXPECT(TOKEN_RETURN);

  if (AT(TOKEN_SEMICOLON) || AT(TOKEN_NEWLINE)) {
    if (isInitializer) {
      EMIT2(OP_GET_LOCAL, 0);
    } else {
      EMIT1(OP_NIL);
    }
  } else {
    CHECK(parseExpression);
  }
  EXPECT_STATEMENT_DELIMITER();
  EMIT1(OP_RETURN);

  return STATUS_OK;
}

static Status parseWhileStatement(Parser *parser) {
  i32 exitJump, loopStart;

  EXPECT(TOKEN_WHILE);

  /* condition */
  loopStart = CHUNK_POS;
  CHECK(parseExpression);
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &exitJump);
  EXPECT(TOKEN_COLON);

  /* body */
  EMIT1(OP_POP);
  CHECK1(parseBlock, UTRUE);
  CHECK1(emitLoop, loopStart);

  /* exit */
  CHECK1(patchJump, exitJump);
  EMIT1(OP_POP);

  return STATUS_OK;
}

static Status parseExpressionStatement(Parser *parser) {
  CHECK(parseExpression);
  EXPECT_STATEMENT_DELIMITER();
  EMIT1(OP_POP);
  return STATUS_OK;
}

static Status parseStatement(Parser *parser) {
  switch (parser->current.type) {
    case TOKEN_FOR:
      return parseForInStatement(parser);
    case TOKEN_IF:
      return parseIfStatement(parser);
    case TOKEN_RETURN:
      return parseReturnStatement(parser);
    case TOKEN_WHILE:
      return parseWhileStatement(parser);
    case TOKEN_IMPORT:
    case TOKEN_FROM:
      return parseImportStatement(parser);
    case TOKEN_NEWLINE:
    case TOKEN_SEMICOLON:
      ADVANCE();
      return STATUS_OK;
    case TOKEN_PASS:
      ADVANCE();
      EXPECT_STATEMENT_DELIMITER();
      return STATUS_OK;
    default:
      break;
  }
  return parseExpressionStatement(parser);
}

static Status parseDeclaration(Parser *parser) {
  switch (parser->current.type) {
    case TOKEN_CLASS:
      return parseClassDeclaration(parser);
    case TOKEN_TRAIT:
      return parseTraitDeclaration(parser);
    case TOKEN_DEF:
      return parseFunctionDeclaration(parser);
    case TOKEN_AT:
      return parseDecoratorApplication(parser);
    case TOKEN_VAR:
    case TOKEN_FINAL:
      return parseVariableDeclaration(parser);
    default:
      break;
  }
  return parseStatement(parser);
}

ubool parse(const char *source, String *moduleName, ObjThunk **out) {
  Lexer lexer;
  Parser parser;
  Environment env;
  ObjThunk *thunk;
  ThunkContext thunkContext;

  initParseRulesPrivate();

  initThunkContext(&thunkContext);
  thunk = newThunk();
  thunk->moduleName = moduleName;

  initEnvironment(&env, thunk, &thunkContext);
  initLexer(&lexer, source);
  initParser(&parser, &lexer, &env);
  activeParser = &parser;

  if (!lexerNext(&lexer, &parser.current)) {
    return STATUS_ERROR;
  }

  while (!atToken(&parser, TOKEN_EOF)) {
    if (!parseDeclaration(&parser)) {
      return STATUS_ERROR;
    }
  }

  if (!emit2(&parser, OP_NIL, OP_RETURN)) {
    return STATUS_ERROR;
  }

  activeParser = NULL;
  *out = thunk;
  return STATUS_OK;
}

static ParseRule newRule(ParseFn prefix, ParseFn infix, Precedence prec) {
  ParseRule rule;
  rule.prefix = prefix;
  rule.infix = infix;
  rule.precedence = prec;
  return rule;
}

static void initParseRulesPrivate(void) {
  if (parseRulesInitialized) {
    return;
  }
  parseRulesInitialized = UTRUE;

  rules[TOKEN_LEFT_PAREN] = newRule(parseGrouping, parseFunctionCall, PREC_CALL);
  rules[TOKEN_LEFT_BRACE] = newRule(parseMapDisplay, NULL, PREC_NONE);
  rules[TOKEN_LEFT_BRACKET] = newRule(parseListDisplay, parseSubscript, PREC_CALL);
  rules[TOKEN_DOT] = newRule(NULL, parseDot, PREC_CALL);
  rules[TOKEN_MINUS] = newRule(parseUnary, parseBinary, PREC_TERM);
  rules[TOKEN_PERCENT] = newRule(NULL, parseBinary, PREC_FACTOR);
  rules[TOKEN_PLUS] = newRule(NULL, parseBinary, PREC_TERM);
  rules[TOKEN_SLASH] = newRule(NULL, parseBinary, PREC_FACTOR);
  rules[TOKEN_STAR] = newRule(NULL, parseBinary, PREC_FACTOR);
  rules[TOKEN_PIPE] = newRule(NULL, parseBinary, PREC_BITWISE_OR);
  rules[TOKEN_AMPERSAND] = newRule(NULL, parseBinary, PREC_BITWISE_AND);
  rules[TOKEN_CARET] = newRule(NULL, parseBinary, PREC_BITWISE_XOR);
  rules[TOKEN_TILDE] = newRule(parseUnary, NULL, PREC_NONE);
  rules[TOKEN_SHIFT_LEFT] = newRule(NULL, parseBinary, PREC_SHIFT);
  rules[TOKEN_SHIFT_RIGHT] = newRule(NULL, parseBinary, PREC_SHIFT);
  rules[TOKEN_BANG_EQUAL] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_EQUAL_EQUAL] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_GREATER] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_GREATER_EQUAL] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_LESS] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_LESS_EQUAL] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_SLASH_SLASH] = newRule(NULL, parseBinary, PREC_FACTOR);
  rules[TOKEN_STAR_STAR] = newRule(NULL, parseBinary, PREC_POWER);
  rules[TOKEN_IDENTIFIER] = newRule(parseNameWithAssignment, NULL, PREC_NONE);
  rules[TOKEN_STRING] = newRule(parseStringLiteral, NULL, PREC_NONE);
  rules[TOKEN_RAW_STRING] = newRule(parseRawStringLiteral, NULL, PREC_NONE);
  rules[TOKEN_NUMBER] = newRule(parseNumber, NULL, PREC_NONE);
  rules[TOKEN_NUMBER_HEX] = newRule(parseNumberHex, NULL, PREC_NONE);
  rules[TOKEN_NUMBER_BIN] = newRule(parseNumberBin, NULL, PREC_NONE);
  rules[TOKEN_AND] = newRule(NULL, parseAnd, PREC_AND);
  rules[TOKEN_DEF] = newRule(parseLambda, NULL, PREC_NONE);
  rules[TOKEN_FALSE] = newRule(parseLiteral, NULL, PREC_NONE);
  rules[TOKEN_IF] = newRule(parseConditional, NULL, PREC_NONE);
  rules[TOKEN_NIL] = newRule(parseLiteral, NULL, PREC_NONE);
  rules[TOKEN_OR] = newRule(NULL, parseOr, PREC_OR);
  rules[TOKEN_SUPER] = newRule(parseSuper, NULL, PREC_NONE);
  rules[TOKEN_THIS] = newRule(parseThis, NULL, PREC_NONE);
  rules[TOKEN_TRUE] = newRule(parseLiteral, NULL, PREC_NONE);
  rules[TOKEN_AS] = newRule(NULL, parseAs, PREC_COMPARISON);
  rules[TOKEN_FINAL] = newRule(parseFrozenDisplay, NULL, PREC_NONE);
  rules[TOKEN_IN] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_IS] = newRule(NULL, parseBinary, PREC_COMPARISON);
  rules[TOKEN_NOT] = newRule(parseUnary, parseBinary, PREC_COMPARISON);
  rules[TOKEN_RAISE] = newRule(parseRaise, NULL, PREC_NONE);
}

void markParserRoots(void) {
  Parser *parser = activeParser;
  if (parser) {
    Environment *env = parser->env;
    for (; env; env = env->enclosing) {
      markObject((Obj *)env->thunk);
    }
  }
}
