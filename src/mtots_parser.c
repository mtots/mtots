#include "mtots_parser.h"

#include "mtots_vm.h"

#include <stdlib.h>
#include <string.h>

#define MAX_CONST_PER_THUNK      255
#define AT(t)                    (atToken(parser,(t)))
#define WRAP(e)                  do { if (!(e)) { return UFALSE; } } while (0)
#define EXPECT(t)                WRAP(expectToken(parser,(t)))
#define ADVANCE()                WRAP(advance(parser))
#define ADD_CONST_VALUE(v,r)     WRAP(addConstValue(parser,(v),(r)))
#define ADD_CONST_STRING(v,r)    WRAP(addConstValue(parser,STRING_VAL(v),(r)))
#define ADD_CONST_SLICE(s,r)     WRAP(addConstSlice(parser,(s),(r)))
#define CHECK(f)                 WRAP((f)(parser))
#define CHECK1(f,a)              WRAP((f)(parser,(a)))
#define CHECK2(f,a,b)            WRAP((f)(parser,(a),(b)))
#define CHECK3(f,a,b,c)          WRAP((f)(parser,(a),(b),(c)))
#define CHECK4(f,a,b,c,d)        WRAP((f)(parser,(a),(b),(c),(d)))
#define EMIT1(a)                 WRAP(emit1(parser,(a)))
#define EMIT2(a,b)               WRAP(emit2(parser,(a),(b)))
#define EMIT3(a,b,c)             WRAP(emit3(parser,(a),(b),(c)))
#define EMIT_CONST(v)            WRAP(emitConst(parser,(v)))
#define TODO(n)                  do { runtimeError("TODO " # n); return UFALSE; } while (0)
#define ENV                      (parser->env)
#define THUNK                    (ENV->thunk)
#define THUNK_CONTEXT            (ENV->thunkContext)
#define MODULE_NAME_CHARS        (THUNK->moduleName->chars)
#define CURRENT_LINE             (parser->current.line)
#define PREVIOUS_LINE            (parser->previous.line)
#define MARK_LOCAL_READY(local)  ((local)->scopeDepth = ENV->scopeDepth)
#define CHUNK_POS                (THUNK->chunk.count)
#define SLICE_PREVIOUS()         (newSlice(parser->previous.start, parser->previous.length))
#define SLICE_CURRENT()          (newSlice(parser->current.start, parser->current.length))

#define ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(r) \
  ADD_CONST_SLICE(SLICE_PREVIOUS(),(r))

#define EXPECT_STATEMENT_DELIMITER() CHECK(expectStatementDelimiter)

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
    u8 global;     /* the constant ID containing the name of the variable */
    Local *local;
  } as;
} VariableDeclaration;

typedef struct Upvalue {
  u8 index;
  ubool isLocal;
} Upvalue;

/* Additional context about the Thunk as we are compiling the code */
typedef struct ThunkContext {
  ubool isInitializer;  /* if true, disallows explicit returns and returns 'this' */
  ubool isMethod;       /* if true, 'this' variable is set up */
  ubool isLambda;       /* if true, may not contain any statements */
} ThunkContext;

typedef struct Environment {
  struct Environment *enclosing;  /* the parent environment */
  ObjThunk *thunk;                /* active Thunk */
  ThunkContext *thunkContext;
  Local locals[U8_COUNT];
  i16 localsCount;                /* number of active local variables in this environment */
  i16 scopeDepth;                 /* scope depth */
  Upvalue upvalues[U8_COUNT];
} Environment;

typedef struct ClassInfo {
  struct ClassInfo *enclosing;
  ubool hasSuperClass;
} ClassInfo;

typedef struct Parser {
  Environment *env;      /* the deepest currently active environment */
  ClassInfo *classInfo;  /* information about the current class definition, if any */
  Lexer *lexer;          /* lexer to read tokens from */
  Token current;         /* currently considered token */
  Token previous;        /* previously considered token */

  /* A list to use as a scratch buffer when parsing default arguments */
  ObjList *defaultArgs;
} Parser;

typedef ubool (*ParseFn)(Parser*);

typedef struct ParseRule {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static Parser *activeParser;
static ParseRule rules[TOKEN_EOF + 1];
static ubool parseRulesInitialized;

static ubool parseDeclaration(Parser *parser);
static ubool parseStatement(Parser *parser);
static ubool parseExpression(Parser *parser);
static ubool parsePrec(Parser *parser, Precedence prec);
static void initParseRulesPrivate(void);
static ubool addConstValue(Parser *parser, Value value, u8 *ref);
static ubool stringTokenToObjString(Parser *parser, String **out);
static ubool parseBlock(Parser *parser, ubool newScope);
static ubool loadVariableByName(Parser *parser, StringSlice name);
static ubool storeVariableByName(Parser *parser, StringSlice name);
static ubool parseFunctionCore(Parser *parser, StringSlice name, ThunkContext *thunkContext);
static ubool parseArgumentList(Parser *parser, u8 *out);

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
  parser->defaultArgs = NULL;
}

static void initThunkContext(ThunkContext *thunkContext) {
  thunkContext->isInitializer = UFALSE;
  thunkContext->isMethod = UFALSE;
  thunkContext->isLambda = UFALSE;
}

static ubool sliceEquals(StringSlice slice, const char *string) {
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

static ubool newLocal(Parser *parser, StringSlice name, Local **out) {
  Local *local;
  if (ENV->localsCount == U8_MAX) {
    runtimeError(
      "[%s:%d] Too many local variables",
      MODULE_NAME_CHARS,
      PREVIOUS_LINE);
    return UFALSE;
  }
  local = *out = &ENV->locals[ENV->localsCount++];
  local->isCaptured = UFALSE;
  local->name = name;
  local->scopeDepth = -1;
  return UTRUE;
}

static ubool atToken(Parser *parser, TokenType type) {
  return parser->current.type == type;
}

static ubool maybeAtTypeExpression(Parser *parser) {
  return atToken(parser, TOKEN_IDENTIFIER) || atToken(parser, TOKEN_NIL);
}

static ubool advance(Parser *parser) {
  parser->previous = parser->current;
  return lexerNext(parser->lexer, &parser->current);
}

static ubool expectToken(Parser *parser, TokenType type) {
  if (!atToken(parser, type)) {
    runtimeError(
      "[%s:%d] Expected token %s but got %s",
      MODULE_NAME_CHARS,
      CURRENT_LINE,
      tokenTypeToName(type),
      tokenTypeToName(parser->current.type));
    return UFALSE;
  }
  return advance(parser);
}

static ubool expectStatementDelimiter(Parser *parser) {
  if (AT(TOKEN_NEWLINE)) {
    ADVANCE();
  } else {
    EXPECT(TOKEN_SEMICOLON);
  }
  return UTRUE;
}

static ubool emit1(Parser *parser, u8 byte1) {
  writeChunk(&THUNK->chunk, byte1, PREVIOUS_LINE);
  return UTRUE;
}

static ubool emit2(Parser *parser, u8 byte1, u8 byte2) {
  EMIT1(byte1);
  EMIT1(byte2);
  return UTRUE;
}

static ubool emit3(Parser *parser, u8 byte1, u8 byte2, u8 byte3) {
  EMIT1(byte1);
  EMIT1(byte2);
  EMIT1(byte3);
  return UTRUE;
}

static ubool emitLoop(Parser *parser, i32 loopStart) {
  i32 offset;

  EMIT1(OP_LOOP);

  offset = CHUNK_POS - loopStart + 2;
  if (offset > U16_MAX) {
    runtimeError(
      "[%s:%d] Loop body too large",
      MODULE_NAME_CHARS,
      PREVIOUS_LINE);
    return UFALSE;
  }

  EMIT1((offset >> 8) & 0xFF);
  EMIT1(offset & 0xFF);
  return UTRUE;
}

static ubool emitJump(Parser *parser, u8 instruction, i32 *jump) {
  EMIT3(instruction, 0xFF, 0xFF);
  *jump = CHUNK_POS - 2;
  return UTRUE;
}

static ubool patchJump(Parser *parser, i32 offset) {
  /* -2 to adjust for the bytecode for the jump offset itself */
  i32 jump = CHUNK_POS - offset - 2;

  if (jump > U16_MAX) {
    runtimeError(
      "[%s:%d] Too much code to jump over",
      MODULE_NAME_CHARS,
      PREVIOUS_LINE);
    return UFALSE;
  }

  THUNK->chunk.code[offset    ] = (jump >> 8) & 0xFF;
  THUNK->chunk.code[offset + 1] = jump & 0xFF;
  return UTRUE;
}

static ubool emitConst(Parser *parser, Value value) {
  u8 constID;
  ADD_CONST_VALUE(value, &constID);
  EMIT2(OP_CONSTANT, constID);
  return UTRUE;
}

/* Add a new constant to the constant pool. The reference to the given value
 * will be stored in `ref`. */
static ubool addConstValue(Parser *parser, Value value, u8 *ref) {
  size_t id = addConstant(&THUNK->chunk, value);
  if (id >= U8_MAX) {
    runtimeError("[%s:%d] Too many constants in thunk",
      MODULE_NAME_CHARS,
      PREVIOUS_LINE);
    return UFALSE;
  }
  *ref = (u8)id;
  return UTRUE;
}

static ubool addConstSlice(Parser *parser, StringSlice slice, u8 *ref) {
  return addConstValue(parser, STRING_VAL(internString(slice.chars, slice.length)), ref);
}

static ubool inGlobalScope(Parser *parser) {
  return parser->env->enclosing == NULL && parser->env->scopeDepth == 0;
}

static ubool beginScope(Parser *parser) {
  ENV->scopeDepth++;
  return UTRUE;
}

static ubool endScope(Parser *parser) {
  /* TODO: Coalesce multiple of these pops into a single instruction */
  ENV->scopeDepth--;
  while (ENV->localsCount > 0 &&
      ENV->locals[ENV->localsCount - 1].scopeDepth > ENV->scopeDepth) {
    if (ENV->locals[ENV->localsCount - 1].isCaptured) {
      EMIT1(OP_CLOSE_UPVALUE);
    } else {
      EMIT1(OP_POP);
    }
    ENV->localsCount--;
  }
  return UTRUE;
}

/* Declare a variable to exist, without yet being ready for use.
 *
 * For global varaibles, we save the name of the variable for later use.
 * For local variables, the next stack slot will be reserved for the variable.
 *
 * `isReady` indicates whether the variable is allowed to start appearing
 * in expressions.
 */
static ubool declareVariable(
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
  return UTRUE;
}

/* Initialize the declared variable the the value sitting at the top of the stack.
 *
 * For global variables, we emit opcode to pop and store the TOS to the variable.
 * For local variables, we emit not opcode since the TOS is already where the
 *   variable is meant to be stored. But we update the declaration to indicate
 *   that it is ready to be used (this is already the case if `declareVariable`
 *   was called with `isReady` set to true).
 */
static ubool defineVariable(Parser *parser, VariableDeclaration *decl) {
  if (decl->isLocal) {
    MARK_LOCAL_READY(decl->as.local);
  } else {
    EMIT2(OP_DEFINE_GLOBAL, decl->as.global);
  }
  return UTRUE;
}

static ubool parseTypeExpression(Parser *parser) {
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
  return UTRUE;
}

static ubool parseTypeParameters(Parser *parser) {
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
  return UTRUE;
}

static ubool parseImportStatement(Parser *parser) {
  /* Imports will always assume global scope */
  u8 moduleName, memberName, alias;
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

  EMIT2(OP_IMPORT, moduleName);
  if (fromStmt) {
    EMIT2(OP_GET_FIELD, memberName);
  }
  EMIT2(OP_DEFINE_GLOBAL, alias);

  EXPECT_STATEMENT_DELIMITER();

  return UTRUE;
}

static ubool parseFieldDeclaration(Parser *parser) {
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
  return UTRUE;
}

static ubool parseMethodDeclaration(Parser *parser) {
  ThunkContext thunkContext;
  StringSlice name;
  u8 nameID;
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
  EMIT2(OP_METHOD, nameID);
  return UTRUE;
}

static ubool parseStaticMethodDeclaration(Parser *parser) {
  ThunkContext thunkContext;
  StringSlice name;
  u8 nameID;
  initThunkContext(&thunkContext);
  EXPECT(TOKEN_STATIC);
  EXPECT(TOKEN_DEF);
  EXPECT(TOKEN_IDENTIFIER);
  name = SLICE_PREVIOUS();
  ADD_CONST_SLICE(name, &nameID);
  CHECK2(parseFunctionCore, name, &thunkContext);
  EMIT2(OP_STATIC_METHOD, nameID);
  return UTRUE;
}

static ubool parseClassDeclaration(Parser *parser) {
  u8 classNameID;
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
  EMIT2(OP_CLASS, classNameID);
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
  while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  EXPECT(TOKEN_INDENT);
  while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  if (AT(TOKEN_STRING) || AT(TOKEN_RAW_STRING)) { /* comments */
    ADVANCE();
    while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  }

  if (AT(TOKEN_PASS)) {
    ADVANCE();
    while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  }

  while (AT(TOKEN_STATIC)) {
    CHECK(parseStaticMethodDeclaration);
  }

  while (AT(TOKEN_VAR) || AT(TOKEN_FINAL)) {
    CHECK(parseFieldDeclaration);
  }

  while (AT(TOKEN_DEF)) {
    CHECK(parseMethodDeclaration);
    while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  }

  EXPECT(TOKEN_DEDENT);
  EMIT1(OP_POP);          /* class (from loadVariableByName) */

  if (classInfo.hasSuperClass) {
    CHECK(endScope);
  }

  parser->classInfo = classInfo.enclosing;

  return UTRUE;
}

static ubool parseTraitDeclaration(Parser *parser) {
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
        case TOKEN_INDENT: depth++; break;
        case TOKEN_DEDENT: depth--; break;
        default: break;
      }
      ADVANCE();
    }
  }
  return UTRUE;
}

static ubool parseDefaultArgument(Parser *parser, Value *out) {
  switch (parser->current.type) {
    case TOKEN_NIL:
      ADVANCE();
      *out = NIL_VAL();
      return UTRUE;
    case TOKEN_TRUE:
      ADVANCE();
      *out = BOOL_VAL(UTRUE);
      return UTRUE;
    case TOKEN_FALSE:
      ADVANCE();
      *out = BOOL_VAL(UFALSE);
      return UTRUE;
    case TOKEN_NUMBER:
      ADVANCE();
      *out = NUMBER_VAL(strtod(parser->previous.start, NULL));
      return UTRUE;
    case TOKEN_STRING: {
      String *str;
      ADVANCE();
      CHECK1(stringTokenToObjString, &str);
      *out = STRING_VAL(str);
      return UTRUE;
    }
    default: break;
  }
  runtimeError(
    "[%s:%d] Expected default argument expression but got %s",
    MODULE_NAME_CHARS,
    CURRENT_LINE,
    tokenTypeToName(parser->current.type));
  return UTRUE;
}

static ubool parseParameterList(Parser *parser, i16 *argCount) {
  i16 argc = 0;
  parser->defaultArgs->length = 0;
  EXPECT(TOKEN_LEFT_PAREN);
  while (!AT(TOKEN_RIGHT_PAREN)) {
    Local *local;
    argc++;
    EXPECT(TOKEN_IDENTIFIER);
    CHECK2(newLocal, SLICE_PREVIOUS(), &local);
    MARK_LOCAL_READY(local); /* all arguments are initialized at the start */

    /* argc and defaultArgc must not go over U8_MAX, but newLocal already limits the number
     * of local variables to that amount anyway, so there is no real need to do
     * a separate check. */
    if (maybeAtTypeExpression(parser)) {
      CHECK(parseTypeExpression);
    }
    if (parser->defaultArgs->length > 0 && !AT(TOKEN_EQUAL)) {
      runtimeError(
        "[%s:%d] Non-optional arguments may not follow any optional arguments",
        MODULE_NAME_CHARS,
        PREVIOUS_LINE);
      return UFALSE;
    }
    if (AT(TOKEN_EQUAL)) {
      ADVANCE();
      listAppend(parser->defaultArgs, NIL_VAL());
      CHECK1(
        parseDefaultArgument,
        &parser->defaultArgs->buffer[parser->defaultArgs->length - 1]);
    }
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }
  EXPECT(TOKEN_RIGHT_PAREN);
  *argCount = argc;
  return UTRUE;
}

/* Parses a function starting from the argument list and leaves a closure on the top of the satck */
static ubool parseFunctionCore(Parser *parser, StringSlice name, ThunkContext *thunkContext) {
  Environment env;
  ObjThunk *thunk;
  i16 i;
  u8 thunkID;

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
  CHECK1(parseParameterList, &thunk->arity);
  if (parser->defaultArgs->length > 0) {
    thunk->defaultArgs = ALLOCATE(Value, parser->defaultArgs->length);
    thunk->defaultArgsCount = parser->defaultArgs->length;
    memcpy(
      thunk->defaultArgs,
      parser->defaultArgs->buffer,
      sizeof(Value) * parser->defaultArgs->length);
    parser->defaultArgs->length = 0; /* clear the defaultArgs list for next use */
  }

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
  ADD_CONST_VALUE(THUNK_VAL(thunk), &thunkID);

  EMIT2(OP_CLOSURE, thunkID);
  for (i = 0; i < thunk->upvalueCount; i++) {
    EMIT1(env.upvalues[i].isLocal ? 1 : 0);
    EMIT1(env.upvalues[i].index);
  }

  return UTRUE;
}

static ubool parseDecoratorApplication(Parser *parser) {
  ThunkContext thunkContext;
  ubool methodCall = UFALSE;
  u8 nameID;
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
    EMIT3(OP_INVOKE, nameID, 1);
  } else {
    /* Otherwise, do a function call */
    EMIT2(OP_CALL, 1);
  }
  return UTRUE;
}

static ubool parseFunctionDeclaration(Parser *parser) {
  VariableDeclaration variable;
  ThunkContext thunkContext;

  initThunkContext(&thunkContext);

  EXPECT(TOKEN_DEF);
  EXPECT(TOKEN_IDENTIFIER);

  CHECK3(declareVariable, SLICE_PREVIOUS(), UTRUE, &variable);

  CHECK2(parseFunctionCore, SLICE_PREVIOUS(), &thunkContext);

  CHECK1(defineVariable, &variable);

  return UTRUE;
}

static ubool parseVariableDeclaration(Parser *parser) {
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

  return UTRUE;
}

static ParseRule *getRule(TokenType type) {
  return &rules[type];
}

static ubool parsePrec(Parser *parser, Precedence prec) {
  ParseFn prefixRule;
  prefixRule = getRule(parser->current.type)->prefix;
  if (prefixRule == NULL) {
    runtimeError(
      "[%s:%d] Expected expression but got %s",
      MODULE_NAME_CHARS,
      PREVIOUS_LINE,
      tokenTypeToName(parser->current.type));
    return UFALSE;
  }
  CHECK(prefixRule);

  while (prec <= getRule(parser->current.type)->precedence) {
    ParseFn infixRule;
    infixRule = getRule(parser->current.type)->infix;
    CHECK(infixRule);
  }

  return UTRUE;
}

static ubool parseExpression(Parser *parser) {
  return parsePrec(parser, PREC_OR);
}

static ubool parseRawStringLiteral(Parser *parser) {
  EXPECT(TOKEN_RAW_STRING);
  {
    char quote = parser->previous.start[1];
    if (quote == parser->previous.start[2] &&
        quote == parser->previous.start[3]) {
      EMIT_CONST(STRING_VAL(internString(
        parser->previous.start + 4,
        parser->previous.length - 7)));
    } else {
      EMIT_CONST(STRING_VAL(internString(
        parser->previous.start + 2,
        parser->previous.length - 3)));
    }
  }
  return UTRUE;
}

static ubool stringTokenToObjString(Parser *parser, String **out) {
  size_t quoteLen;
  char quoteChar = parser->previous.start[0];
  char quoteStr[4];
  Buffer buf;

  if (quoteChar == parser->previous.start[1] &&
      quoteChar == parser->previous.start[2]) {
    quoteStr[0] = quoteStr[1] = quoteStr[2] = quoteChar;
    quoteStr[3] = '\0';
    quoteLen = 3;
  } else {
    quoteStr[0] = quoteChar;
    quoteStr[1] = '\0';
    quoteLen = 1;
  }

  initBuffer(&buf);
  if (!unescapeString2(&buf, parser->previous.start + quoteLen, quoteStr, quoteLen)) {
    return UFALSE;
  }

  *out = bufferToString(&buf);

  freeBuffer(&buf);

  return UTRUE;
}

static ubool parseStringLiteral(Parser *parser) {
  String *str;

  EXPECT(TOKEN_STRING);

  if (!stringTokenToObjString(parser, &str)) {
    return UFALSE;
  }

  EMIT_CONST(STRING_VAL(str));
  return UTRUE;
}

static ubool parseNumber(Parser *parser) {
  double value;
  EXPECT(TOKEN_NUMBER);
  value = strtod(parser->previous.start, NULL);
  EMIT_CONST(NUMBER_VAL(value));
  return UTRUE;
}

static ubool parseNumberHex(Parser *parser) {
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
      return UFALSE;
    }
  }
  EMIT_CONST(NUMBER_VAL(value));
  return UTRUE;
}

static ubool parseNumberBin(Parser *parser) {
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
      return UFALSE;
    }
  }
  EMIT_CONST(NUMBER_VAL(value));
  return UTRUE;
}

static ubool resolveLocalForEnv(
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
        return UFALSE;
      }
      *out = i;
      return UTRUE;
    }
  }
  *out = -1;
  return UTRUE;
}

/* Looks for a local variable matching the given name and returns the locals slot index.
 * If not found, out will be set to -1. */
static ubool resolveLocal(Parser *parser, StringSlice name, i16 *out) {
  return resolveLocalForEnv(parser, ENV, name, out);
}

static ubool addUpvalue(
    Parser *parser, Environment *env, i16 enclosingIndex, ubool isLocal, i16 *upvalueIndex) {
  i16 i, upvalueCount = env->thunk->upvalueCount;
  for (i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &env->upvalues[i];
    if (upvalue->isLocal == isLocal && upvalue->index == enclosingIndex) {
      *upvalueIndex = i;
      return UTRUE;
    }
  }

  if (upvalueCount >= U8_COUNT) {
    runtimeError(
      "[%s:%d] Too many closure variables in thunk",
      MODULE_NAME_CHARS, PREVIOUS_LINE);
    return UFALSE;
  }

  env->upvalues[upvalueCount].isLocal = isLocal;
  env->upvalues[upvalueCount].index = enclosingIndex;
  *upvalueIndex = env->thunk->upvalueCount++;
  return UTRUE;
}

static ubool resolveUpvalueForEnv(
    Parser *parser, Environment *env, StringSlice name, i16 *outUpvalueIndex) {
  i16 parentLocalIndex, parentUpvalueIndex;
  if (env->enclosing == NULL) {
    *outUpvalueIndex = -1;
    return UTRUE;
  }

  /* Check if the upvalue we are looking for is a local variable in the
   * immediate enclosing environment */
  CHECK3(resolveLocalForEnv, env->enclosing, name, &parentLocalIndex);
  if (parentLocalIndex != -1) {
    env->enclosing->locals[parentLocalIndex].isCaptured = UTRUE;
    CHECK4(addUpvalue, env, parentLocalIndex, UTRUE, outUpvalueIndex);
    return UTRUE;
  }

  /* If it is not, extend the search to environments that enclose the environment that
   * encloses this env */
  CHECK3(resolveUpvalueForEnv, env->enclosing, name, &parentUpvalueIndex);
  if (parentUpvalueIndex != -1) {
    CHECK4(addUpvalue, env, parentUpvalueIndex, UFALSE, outUpvalueIndex);
    return UTRUE;
  }

  *outUpvalueIndex = -1;
  return UTRUE;
}

/* Like resolveLocal, but will instead check the enclosing environment for an
 * upvalue */
static ubool resolveUpvalue(Parser *parser, StringSlice name, i16 *out) {
  return resolveUpvalueForEnv(parser, ENV, name, out);
}

/* Emits opcode to load the value of a variable on the stack */
static ubool loadVariableByName(Parser *parser, StringSlice name) {
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
      u8 nameID;
      ADD_CONST_SLICE(name, &nameID);
      EMIT2(OP_GET_GLOBAL, nameID);
    }
  }
  return UTRUE;
}

/* Emits opcode to pop TOS and store it in the variable */
static ubool storeVariableByName(Parser *parser, StringSlice name) {
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
      u8 nameID;
      ADD_CONST_SLICE(name, &nameID);
      EMIT2(OP_SET_GLOBAL, nameID);
    }
  }
  return UTRUE;
}

static ubool parseName(Parser *parser, ubool canAssign) {
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

  return UTRUE;
}

static ubool parseNameWithAssignment(Parser *parser) {
  return parseName(parser, UTRUE);
}

static ubool parseThis(Parser *parser) {
  EXPECT(TOKEN_THIS);
  CHECK1(loadVariableByName, newSlice("this", 4));
  return UTRUE;
}

static ubool parseSuper(Parser *parser) {
  u8 methodNameID, argCount;
  EXPECT(TOKEN_SUPER);
  if (!parser->classInfo || !parser->classInfo->hasSuperClass) {
    runtimeError("'super' cannot be used outside a class with a super class");
    return UFALSE;
  }
  EXPECT(TOKEN_DOT);
  EXPECT(TOKEN_IDENTIFIER);
  ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&methodNameID);
  CHECK1(loadVariableByName, newSlice("this", 4));
  CHECK1(parseArgumentList, &argCount);
  CHECK1(loadVariableByName, newSlice("super", 5));
  EMIT3(OP_SUPER_INVOKE, methodNameID, argCount);
  return UTRUE;
}

static ubool parseLambda(Parser *parser) {
  ThunkContext thunkContext;
  initThunkContext(&thunkContext);
  thunkContext.isLambda = UTRUE;
  EXPECT(TOKEN_DEF);
  CHECK2(parseFunctionCore, newSlice("<lambda>", 8), &thunkContext);
  return UTRUE;
}

static ubool parseTry(Parser *parser) {
  i32 startJump, endJump;
  EXPECT(TOKEN_TRY);
  CHECK2(emitJump, OP_TRY_START, &startJump);
  CHECK(parseExpression);
  CHECK2(emitJump, OP_TRY_END, &endJump);
  EXPECT(TOKEN_ELSE);
  CHECK1(patchJump, startJump);
  CHECK(parseExpression);
  CHECK1(patchJump, endJump);
  return UTRUE;
}

static ubool parseRaise(Parser *parser) {
  EXPECT(TOKEN_RAISE);
  CHECK(parseExpression);
  EMIT1(OP_RAISE);
  return UTRUE;
}

static ubool parseLiteral(Parser *parser) {
  ADVANCE();
  switch (parser->previous.type) {
    case TOKEN_FALSE: EMIT1(OP_FALSE); break;
    case TOKEN_NIL: EMIT1(OP_NIL); break;
    case TOKEN_TRUE: EMIT1(OP_TRUE); break;
    default:
      assertionError("parseLiteral");
      return UFALSE; /* unreachable */
  }
  return UTRUE;
}

static ubool parseGrouping(Parser *parser) {
  EXPECT(TOKEN_LEFT_PAREN);
  CHECK(parseExpression);
  EXPECT(TOKEN_RIGHT_PAREN);
  return UTRUE;
}

static ubool parseUnary(Parser *parser) {
  TokenType operatorType = parser->current.type;
  ADVANCE(); /* operator */

  /* compile the operand */
  CHECK1(parsePrec, (operatorType == TOKEN_NOT ? PREC_NOT : PREC_UNARY));

  /* Emit the operator instruction */
  switch (operatorType) {
    case TOKEN_TILDE: EMIT1(OP_BITWISE_NOT); break;
    case TOKEN_NOT: EMIT1(OP_NOT); break;
    case TOKEN_MINUS: EMIT1(OP_NEGATE); break;
    default:
      assertionError("parseUnary");
      return UFALSE; /* Unreachable */
  }

  return UTRUE;
}

static ubool parseListDisplayBody(Parser *parser, u8 *out) {
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
      return UFALSE;
    }
    length++;
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }
  *out = length;
  return UTRUE;
}

static ubool parseListDisplay(Parser *parser) {
  u8 length;
  EXPECT(TOKEN_LEFT_BRACKET);
  CHECK1(parseListDisplayBody, &length);
  EXPECT(TOKEN_RIGHT_BRACKET);
  EMIT2(OP_NEW_LIST, length);
  return UTRUE;
}

static ubool parseMapDisplayBody(Parser *parser, u8 *out) {
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
      return UFALSE;
    }
    length++;
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }

  *out = length;
  return UTRUE;
}

static ubool parseMapDisplay(Parser *parser) {
  u8 length;
  EXPECT(TOKEN_LEFT_BRACE);
  CHECK1(parseMapDisplayBody, &length);
  EXPECT(TOKEN_RIGHT_BRACE);
  EMIT2(OP_NEW_DICT, length);
  return UTRUE;
}

static ubool parseFrozenDisplay(Parser *parser) {
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
    return UTRUE;
  }
  return UTRUE;
}

static ubool parseArgumentList(Parser *parser, u8 *out) {
  u8 argCount = 0;

  EXPECT(TOKEN_LEFT_PAREN);
  while (!AT(TOKEN_RIGHT_PAREN)) {
    CHECK(parseExpression);
    if (argCount == U8_MAX) {
      runtimeError(
        "[%s:%d] Too many arguments (no more than %d are allowed)",
        MODULE_NAME_CHARS,
        PREVIOUS_LINE,
        U8_MAX);
      return UFALSE;
    }
    argCount++;
    if (AT(TOKEN_COMMA)) {
      ADVANCE();
    } else {
      break;
    }
  }
  EXPECT(TOKEN_RIGHT_PAREN);

  *out = argCount;
  return UTRUE;
}

static ubool parseFunctionCall(Parser *parser) {
  u8 argCount;
  CHECK1(parseArgumentList, &argCount);
  EMIT2(OP_CALL, argCount);
  return UTRUE;
}

static ubool parseSubscript(Parser *parser) {
  EXPECT(TOKEN_LEFT_BRACKET);
  if (AT(TOKEN_COLON)) {
    /* Implicit nil when the first argument is missing */
    EMIT1(OP_NIL);
  } else {
    CHECK(parseExpression); /* index */
  }

  if (AT(TOKEN_COLON)) {
    u8 nameID;
    ADVANCE();
    ADD_CONST_STRING(vm.sliceString, &nameID);
    if (AT(TOKEN_RIGHT_BRACKET)) {
      EMIT1(OP_NIL); /* Implicit nil second argument, if missing */
    } else {
      CHECK(parseExpression);
    }
    EXPECT(TOKEN_RIGHT_BRACKET);
    EMIT3(OP_INVOKE, nameID, 2);
  } else {
    EXPECT(TOKEN_RIGHT_BRACKET);
    if (AT(TOKEN_EQUAL)) {
      u8 nameID;
      ADVANCE();
      ADD_CONST_STRING(vm.setitemString, &nameID);
      CHECK(parseExpression);
      EMIT3(OP_INVOKE, nameID, 2);
    } else {
      u8 nameID;
      ADD_CONST_STRING(vm.getitemString, &nameID);
      EMIT3(OP_INVOKE, nameID, 1);
    }
  }

  return UTRUE;
}

static ubool parseAs(Parser *parser) {
  /* 'as' expressions have zero runtime effect */
  EXPECT(TOKEN_AS);
  return parseTypeExpression(parser);
}


static ubool parseDot(Parser *parser) {
  u8 nameID;

  EXPECT(TOKEN_DOT);
  EXPECT(TOKEN_IDENTIFIER);
  ADD_CONST_NAME_FROM_PREVIOUS_TOKEN(&nameID);

  if (AT(TOKEN_EQUAL)) {
    ADVANCE();
    CHECK(parseExpression);
    EMIT2(OP_SET_FIELD, nameID);
  } else if (AT(TOKEN_LEFT_PAREN)) {
    u8 argCount;
    CHECK1(parseArgumentList, &argCount);
    EMIT3(OP_INVOKE, nameID, argCount);
  } else {
    EMIT2(OP_GET_FIELD, nameID);
  }

  return UTRUE;
}

static ubool parseAnd(Parser *parser) {
  i32 endJump;
  EXPECT(TOKEN_AND);
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &endJump);
  EMIT1(OP_POP);
  CHECK1(parsePrec, PREC_AND);
  CHECK1(patchJump, endJump);
  return UTRUE;
}

static ubool parseOr(Parser *parser) {
  i32 elseJump;
  i32 endJump;
  EXPECT(TOKEN_OR);
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &elseJump);
  CHECK2(emitJump, OP_JUMP, &endJump);
  CHECK1(patchJump, elseJump);
  EMIT1(OP_POP);
  CHECK1(parsePrec, PREC_OR);
  CHECK1(patchJump, endJump);
  return UTRUE;
}

static ubool parseBinary(Parser *parser) {
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

  CHECK1(parsePrec, rightAssociative ?
      rule->precedence :
      (Precedence) (rule->precedence + 1));

  switch (operatorType) {
    case TOKEN_IS: EMIT1(OP_IS); if (isNot) EMIT1(OP_NOT); break;
    case TOKEN_BANG_EQUAL: EMIT2(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL: EMIT1(OP_EQUAL); break;
    case TOKEN_GREATER: EMIT1(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: EMIT2(OP_LESS, OP_NOT); break;
    case TOKEN_LESS: EMIT1(OP_LESS); break;
    case TOKEN_LESS_EQUAL: EMIT2(OP_GREATER, OP_NOT); break;
    case TOKEN_IN: EMIT1(OP_IN); if (notIn) EMIT1(OP_NOT); break;
    case TOKEN_PLUS: EMIT1(OP_ADD); break;
    case TOKEN_MINUS: EMIT1(OP_SUBTRACT); break;
    case TOKEN_STAR: EMIT1(OP_MULTIPLY); break;
    case TOKEN_SLASH: EMIT1(OP_DIVIDE); break;
    case TOKEN_SLASH_SLASH: EMIT1(OP_FLOOR_DIVIDE); break;
    case TOKEN_PERCENT: EMIT1(OP_MODULO); break;
    case TOKEN_STAR_STAR: EMIT1(OP_POWER); break;
    case TOKEN_SHIFT_LEFT: EMIT1(OP_SHIFT_LEFT); break;
    case TOKEN_SHIFT_RIGHT: EMIT1(OP_SHIFT_RIGHT); break;
    case TOKEN_PIPE: EMIT1(OP_BITWISE_OR); break;
    case TOKEN_AMPERSAND: EMIT1(OP_BITWISE_AND); break;
    case TOKEN_CARET: EMIT1(OP_BITWISE_XOR); break;
    default:
      assertionError("parseBinary");
      return UFALSE; /* unreachable */
  }

  return UTRUE;
}

static ubool parseConditional(Parser *parser) {
  i32 elseJump, endJump;

  /* Condition */
  EXPECT(TOKEN_IF);
  CHECK(parseExpression); /* condition */
  CHECK2(emitJump, OP_JUMP_IF_FALSE, &elseJump);

  /* Left Branch */
  EXPECT(TOKEN_THEN);
  EMIT1(OP_POP); /* pop condition */
  CHECK(parseExpression); /* evaluate left branch */
  CHECK2(emitJump, OP_JUMP, &endJump);

  /* Right Branch */
  EXPECT(TOKEN_ELSE);
  CHECK1(patchJump, elseJump);
  EMIT1(OP_POP); /* pop condition */
  CHECK(parseExpression); /* evaluate right branch */
  CHECK1(patchJump, endJump);

  return UTRUE;
}

static ubool parseBlock(Parser *parser, ubool newScope) {
  ubool atLeastOneDeclaration = UFALSE;
  if (newScope) {
    CHECK(beginScope);
  }

  while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  EXPECT(TOKEN_INDENT);
  while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  while (!AT(TOKEN_DEDENT) && !AT(TOKEN_EOF)) {
    atLeastOneDeclaration = UTRUE;
    CHECK(parseDeclaration);
    while (AT(TOKEN_NEWLINE)) { ADVANCE(); }
  }
  EXPECT(TOKEN_DEDENT);

  if (!atLeastOneDeclaration) {
    runtimeError(
      "[%s:%d] Blocks require at least one declaration, but got none",
      MODULE_NAME_CHARS,
      PREVIOUS_LINE);
    return UFALSE;
  }

  if (newScope) {
    CHECK(endScope);
  }

  return UTRUE;
}

static ubool parseForInStatement(Parser *parser) {
  i32 jump, loopStart;
  VariableDeclaration itemVariable, iterator;
  StringSlice itemVariableName;

  EXPECT(TOKEN_FOR);

  CHECK(beginScope); /* also ensures inGlobalScope will always be false */

  EXPECT(TOKEN_IDENTIFIER);
  itemVariableName = SLICE_PREVIOUS();

  EXPECT(TOKEN_IN);
  CHECK(parseExpression);  /* iterable */
  EMIT1(OP_GET_ITER);      /* replace the iterable with an iterator */
  CHECK3(declareVariable, newSlice("@iterator", 9), UTRUE, &iterator);
  CHECK1(defineVariable, &iterator);
  loopStart = CHUNK_POS;
  EMIT1(OP_GET_NEXT);      /* gets next value returned by the iterator */
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
  return UTRUE;
}

static ubool parseIfStatement(Parser *parser) {
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
      return UFALSE;
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

  return UTRUE;
}

static ubool parseReturnStatement(Parser *parser) {
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

  return UTRUE;
}

static ubool parseWhileStatement(Parser *parser) {
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

  return UTRUE;
}

static ubool parseExpressionStatement(Parser *parser) {
  CHECK(parseExpression);
  EXPECT_STATEMENT_DELIMITER();
  EMIT1(OP_POP);
  return UTRUE;
}

static ubool parseStatement(Parser *parser) {
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
      return UTRUE;
    case TOKEN_PASS:
      ADVANCE();
      EXPECT_STATEMENT_DELIMITER();
      return UTRUE;
    default: break;
  }
  return parseExpressionStatement(parser);
}

static ubool parseDeclaration(Parser *parser) {
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

  parser.defaultArgs = newList(0);

  if (!lexerNext(&lexer, &parser.current)) {
    return UFALSE;
  }

  while (!atToken(&parser, TOKEN_EOF)) {
    if (!parseDeclaration(&parser)) {
      return UFALSE;
    }
  }

  if (!emit2(&parser, OP_NIL, OP_RETURN)) {
    return UFALSE;
  }

  activeParser = NULL;
  *out = thunk;
  return UTRUE;
}

static ParseRule newRule(ParseFn prefix, ParseFn infix, Precedence prec) {
  ParseRule rule;
  rule.prefix = prefix;
  rule.infix = infix;
  rule.precedence = prec;
  return rule;
}

static void initParseRulesPrivate(void) {
  if (!parseRulesInitialized) {
    parseRulesInitialized = UTRUE;
  }

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
  rules[TOKEN_TRY] = newRule(parseTry, NULL, PREC_NONE);
}

void markParserRoots(void) {
  Parser *parser = activeParser;
  if (parser) {
    Environment *env = parser->env;
    markObject((Obj*)parser->defaultArgs);
    for (; env; env = env->enclosing) {
      markObject((Obj*)env->thunk);
    }
  }
}
