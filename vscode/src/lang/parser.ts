import { Uri } from "vscode";
import { MError } from "./error";
import { MToken, MTokenType } from "./token";
import * as ast from "./ast";
import { lex } from "./lexer";
import { MLocation } from "./location";
import { MRange } from "./range";
import { swrap } from "../debug";


const PrecList: MTokenType[][] = [
  [],
  ['or'],
  ['and'],
  [],        // precedence for unary operator 'not'
  ['==', '!=', '<', '>', '<=', '>=', 'in', 'not', 'is', 'as'],
  ['<<', '>>'],
  ['&'],
  ['^'],
  ['|'],
  ['+', '-'],
  ['*', '/', '//', '%'],
  [],        // precedence for unary operators '-', '+' and '~'
  ['**'],
  ['.', '(', '['],
];
const PrecMap: Map<MTokenType, number> = new Map();
for (let i = 0; i < PrecList.length; i++) {
  for (const tokenType of PrecList[i]) {
    PrecMap.set(tokenType, i);
  }
}
const PREC_UNARY_NOT = PrecMap.get('and')! + 1;
const PREC_UNARY_MINUS = PrecMap.get('*')! + 1;
const PREC_PRIMARY = PrecMap.get('.')! + 1;
const BinopMethodMap: Map<MTokenType, string> = new Map([
  ['==', '__eq__'],
  ['!=', '__eq__'],
  ['<', '__lt__'],
  ['<=', '__lt__'],
  ['>', '__lt__'],
  ['>=', '__lt__'],
  ['<<', '__lshift__'],
  ['>>', '__rshift__'],
  ['&', '__and__'],
  ['^', '__xor__'],
  ['|', '__or__'],
  ['+', '__add__'],
  ['-', '__sub__'],
  ['*', '__mul__'],
  ['/', '__div__'],
  ['//', '__floordiv__'],
  ['%', '__mod__'],
  ['**', '__pow__'],
]);
const BinopSwapSet = new Set<MTokenType>(['>', '<=']);
const BinopNegateSet = new Set<MTokenType>(['!=', '>=', '<=']);


export function parse(filePath: string | Uri, s: string): ast.File {
  return swrap('parse', filePath, () => _parse(filePath, s));
}

function _parse(filePath: string | Uri, s: string): ast.File {
  const tokens = lex(filePath, s);
  const errors: MError[] = [];
  var i = 0;
  var peek = tokens[0];
  function incr(): MToken {
    const token = peek;
    if (i < tokens.length) {
      peek = tokens[i++];
      while (i < tokens.length && peek.type === 'ERROR') {
        err(peek.value as string);
        peek = tokens[i++];
      }
    }
    return token;
  }
  incr();
  function getloc(): MLocation {
    return peek.location;
  }
  function err(message: string) {
    errors.push(new MError(getloc(), message));
  }
  function at(type: MTokenType) {
    return peek.type === type;
  }
  function consume(type: MTokenType): boolean {
    if (at(type)) {
      incr();
      return true;
    }
    return false;
  }
  function expect(type: MTokenType, sync=false): MToken {
    if (at(type)) {
      return incr();
    }
    err(`Expected ${type} but got ${peek.type}`);
    const value =
      type === 'NUMBER' ? 0 :
      type === 'STRING' || type == 'IDENTIFIER' || type == 'ERROR' ? '(missing)' :
      null;
    const clone = new MToken(peek.location, type, value);
    if (sync) {
      while (i < tokens.length && peek.type !== type) {
        incr();
      }
      incr();
    }
    return clone;
  }

  function expectStatementDelimiter() {
    if (!consume(';')) {
      expect('NEWLINE', true);
    }
  }

  function parseIdentifier(): ast.Identifier {
    const token = expect('IDENTIFIER');
    return new ast.Identifier(token.location, <string>token.value);
  }

  function parseQualifiedIdentifier(): ast.QualifiedIdentifier {
    const rawIdentifier = parseIdentifier();
    let qualifiedIdentifier = new ast.QualifiedIdentifier(
      rawIdentifier.location, null, rawIdentifier);
    while (consume('.')) {
      const memberIdentifier = parseIdentifier();
      qualifiedIdentifier = new ast.QualifiedIdentifier(
        qualifiedIdentifier.location.merge(memberIdentifier.location),
        qualifiedIdentifier,
        memberIdentifier);
    }
    return qualifiedIdentifier;
  }

  function newNil(location: MLocation): ast.NilLiteral {
    return new ast.NilLiteral(location, null);
  }

  function newErr(location: MLocation): ast.ErrorExpression {
    return new ast.ErrorExpression(location);
  }

  function maybeAtTypeExpression(): boolean {
    return at('IDENTIFIER') || at('nil');
  }

  function logicalNot(location: MLocation, arg: ast.Expression): ast.Operation {
    return new ast.Operation(location, 'not', [arg]);
  }

  function parseTypeExpression(): ast.TypeExpression {
    if (at('nil')) {
      const location = incr().location;
      return new ast.TypeExpression(
        location,
        null,
        new ast.Identifier(location, 'nil'),
        []);
    }
    const firstIdentifier = parseIdentifier();
    let secondIdentifier: ast.Identifier | null = null;
    const maybeDotLocation = peek.location;
    if (consume('.')) {
      if (at('IDENTIFIER')) {
        secondIdentifier = parseIdentifier();
      } else {
        // while this should technically be a syntax error,
        // allowing this form to create the AST in some form allows
        // for better completions.
        // So we fake it here.
        secondIdentifier = new ast.Identifier(maybeDotLocation, ' ');
      }
    }
    const args: ast.TypeExpression[] = [];
    let endLocation = (secondIdentifier || firstIdentifier).location;
    if (consume('[')) {
      while (!at(']')) {
        args.push(parseTypeExpression());
        if (!consume(',')) {
          break;
        }
      }
      endLocation = expect(']').location;
    }
    let location = firstIdentifier.location.merge(endLocation);
    let te = secondIdentifier ?
      new ast.TypeExpression(location, firstIdentifier, secondIdentifier, args) :
      new ast.TypeExpression(location, null, firstIdentifier, args);
    if (at('?')) {
      const qmarkLocation = expect('?').location;
      location = location.merge(qmarkLocation);
      te = new ast.TypeExpression(
        location, null, new ast.Identifier(qmarkLocation, 'Optional'), [te]);
    }
    if (at('|')) {
      const pipeLocation = expect('|').location;
      const rhs = parseTypeExpression();
      location = location.merge(rhs.location);
      te = new ast.TypeExpression(
        location, null, new ast.Identifier(pipeLocation, 'Union'), [te, rhs]);
    }
    return te;
  }

  function parseArguments(): ast.Expression[] {
    return parseArgumentsAndLocations()[0];
  }

  function parseArgumentsAndLocations(): [ast.Expression[], [ast.Identifier, ast.Expression][], MLocation[]] {
    const args: ast.Expression[] = [];
    const kwargs: [ast.Identifier, ast.Expression][] = [];
    const argLocations: MLocation[] = [];
    while (true) {
      const argStartPosition = peek.location.range.start;
      if (at(')')) {
        const argEndPosition = peek.location.range.start;
        argLocations.push(new MLocation(
          tokens[0].location.filePath,
          new MRange(argStartPosition, argEndPosition)));
        break;
      }
      if (peek.type == 'IDENTIFIER' && tokens[i].type === '=') {
        const argname = parseIdentifier();
        expect('=');
        kwargs.push([argname, parseExpression()]);
      } else if (kwargs.length > 0) {
        err(`Positional arguments cannot follow keyword arguments`);
        kwargs.push([
          new ast.Identifier(peek.location, '<positional>'),
          parseExpression()]);
      } else {
        args.push(parseExpression());
      }
      const argEndPosition = peek.location.range.start;
      argLocations.push(new MLocation(
        tokens[0].location.filePath,
        new MRange(argStartPosition, argEndPosition)));
      if (!consume(',')) {
        break;
      }
    }
    while (!at(')')) {
      args.push(parseExpression());
      if (!consume(',')) {
        break;
      }
    }
    return [args, kwargs, argLocations];
  }

  function parsePrefix(): ast.Expression {
    const startLocation = peek.location;
    switch (peek.type) {
      case '(': {
        incr();
        const expression = parseExpression();
        expect(')');
        return expression;
      }
      case '[': {
        incr();
        const items: ast.Expression[] = [];
        while (!at(']')) {
          items.push(parseExpression());
          if (!consume(',')) {
            break;
          }
        }
        const endLocation = expect(']').location;
        const location = startLocation.merge(endLocation);
        return new ast.ListDisplay(location, items);
      }
      case '{': {
        incr();
        const pairs: [ast.Expression, ast.Expression][] = [];
        while (!at('}')) {
          const key = parseExpression();
          const value = consume(':') ?
            parseExpression() :
            newNil(key.location);
          pairs.push([key, value]);
          if (!consume(',')) {
            break;
          }
        }
        const endLocation = expect('}').location;
        const location = startLocation.merge(endLocation);
        return new ast.DictDisplay(location, pairs);
      }
      case 'final': {
        incr();
        if (consume('[')) {
          const items: ast.Expression[] = [];
          while (!at(']')) {
            items.push(parseExpression());
            if (!consume(',')) {
              break;
            }
          }
          const endLocation = expect(']').location;
          const location = startLocation.merge(endLocation);
          return new ast.FrozenListDisplay(location, items);
        } else if (consume('{')) {
          const pairs: [ast.Expression, ast.Expression][] = [];
          while (!at('}')) {
            const key = parseExpression();
            const value = consume(':') ?
              parseExpression() :
              newNil(key.location);
            pairs.push([key, value]);
            if (!consume(',')) {
              break;
            }
          }
          const endLocation = expect('}').location;
          const location = startLocation.merge(endLocation);
          return new ast.FrozenDictDisplay(location, pairs);
        } else {
          err(`Expected '[' or '{' following 'final' in expression`);
          return newErr(peek.location);
        }
      }
      case 'NUMBER': {
        const expression = new ast.NumberLiteral(
          startLocation, <number>peek.value);
        incr();
        return expression;
      }
      case 'STRING': {
        const expression = new ast.StringLiteral(
          startLocation, <string>peek.value);
        incr();
        return expression;
      }
      case 'true': {
        incr();
        return new ast.BoolLiteral(startLocation, true);
      }
      case 'false': {
        incr();
        return new ast.BoolLiteral(startLocation, false);
      }
      case 'nil': {
        incr();
        return newNil(startLocation);
      }
      case 'def': {
        incr();
        const parameters = parseParameters();
        const returnType = at(':') ? null : parseTypeExpression();
        expect(':');
        const body = parseExpression();
        const location = startLocation.merge(body.location);
        return new ast.Lambda(location, parameters, returnType, body);
      }
      case 'this':
      case 'super': {
        const name = incr().type;
        return new ast.GetVariable(
          startLocation,
          new ast.Identifier(startLocation, name));
      }
      case 'IDENTIFIER': {
        const identifier = parseIdentifier();
        // TODO: If the type system ever is capable of handling it,
        // stop special-casing Tuple.
        if (identifier.name === 'Tuple') {
          expect('(');
          const args = parseArguments();
          expect(')');
          return new ast.TupleDisplay(identifier.location, identifier, args);
        }
        if (consume('=')) {
          const value = parseExpression();
          return new ast.SetVariable(startLocation, identifier, value);
        } else {
          return new ast.GetVariable(startLocation, identifier);
        }
      }
      case 'not': {
        incr();
        const arg = parsePrec(PREC_UNARY_NOT);
        const location = startLocation.merge(arg.location);
        return logicalNot(location, arg);
      }
      case 'if': {
        const location = incr().location;
        const cond = parseExpression();
        if (!consume('then')) {
          err("Expected 'then' for 'if' expression");
          return new ast.Operation(location, 'if', [cond, newErr(location), newErr(location)]);
        }
        const left = parseExpression();
        if (!consume('else')) {
          err("Expected 'else' for 'if' expression");
          return new ast.Operation(location, 'if', [cond, left, newErr(location)]);
        }
        const right = parseExpression();
        return new ast.Operation(location, 'if', [cond, left, right]);
      }
      case 'raise': {
        incr();
        const exc = parseExpression();
        const location = startLocation.merge(exc.location);
        return new ast.Raise(location, exc);
      }
      case '~':
      case '-':
      case '+':
        const tokenType = peek.type;
        const methodIdentifier = new ast.Identifier(
          startLocation,
          tokenType === '~' ? '__not__' :
          tokenType === '-' ? '__neg__' :
          tokenType === '+' ? '__pos__' : 'invalid');
        incr();
        const arg = parsePrec(PREC_UNARY_MINUS);
        const location = startLocation.merge(arg.location);
        return new ast.MethodCall(location, arg, methodIdentifier, []);
    }
    err(`Expected expression but got '${peek.type}'`);
    return newErr(peek.location);
  }

  function parseInfix(lhs: ast.Expression, startLocation: MLocation): ast.Expression {
    const tokenType = peek.type;
    const precedence = PrecMap.get(tokenType);
    const methodName = BinopMethodMap.get(tokenType);
    if (!precedence) {
      err(`Invalid infix token ${tokenType} (invalid precedence, possibly internal error)`);
      return newErr(peek.location);
    }

    switch (tokenType) {
      case '.': {
        const dotLocation = incr().location;
        if (!at('IDENTIFIER')) {
          const followLocation = peek.location;
          const location = startLocation.merge(dotLocation);
          return new ast.Dot(location, lhs, dotLocation, followLocation);
        }
        const identifier = parseIdentifier();
        if (at('(')) {
          incr();
          const [args, kwargs, argLocations] = parseArgumentsAndLocations();
          const endLocation = expect(')').location;
          const location = startLocation.merge(endLocation);
          return new ast.MethodCall(location, lhs, identifier, args, kwargs, argLocations);
        } else if (consume('=')) {
          const value = parseExpression();
          const location = startLocation.merge(value.location);
          return new ast.SetField(location, lhs, identifier, value);
        }
        const location = startLocation.merge(identifier.location);
        return new ast.GetField(location, lhs, identifier);
      }
      case '[': {
        const openBracketLocation = incr().location;
        if (at(':')) {
          const colonLocation = incr().location;
          const low = newNil(colonLocation);
          const high = at(']') ? newNil(colonLocation) : parseExpression();
          const closeBracketLocation = expect(']').location;
          const location = startLocation.merge(closeBracketLocation);
          const identifier = new ast.Identifier(openBracketLocation, '__slice__');
          return new ast.MethodCall(location, lhs, identifier, [low, high]);
        }
        const index = parseExpression();
        if (at(':')) {
          const colonLocation = incr().location;
          const high = at(']') ? newNil(colonLocation) : parseExpression();
          const closeBracketLocation = expect(']').location;
          const location = startLocation.merge(closeBracketLocation);
          const identifier = new ast.Identifier(openBracketLocation, '__slice__');
          return new ast.MethodCall(location, lhs, identifier, [index, high]);
        }
        const closeBracketLocation = expect(']').location;
        if (consume('=')) {
          const value = parseExpression();
          const location = startLocation.merge(value.location);
          const identifier = new ast.Identifier(
            openBracketLocation, '__setitem__');
          return new ast.MethodCall(
            location, lhs, identifier, [index, value]);
        }
        const location = startLocation.merge(closeBracketLocation);
        const identifier = new ast.Identifier(
          openBracketLocation, '__getitem__');
        return new ast.MethodCall(location, lhs, identifier, [index]);
      }
      case '(': {
        incr();
        const [args, kwargs, argLocations] = parseArgumentsAndLocations();
        const closeParenLocation = expect(')').location;
        const location = startLocation.merge(closeParenLocation);
        return new ast.FunctionCall(location, lhs, args, kwargs, argLocations);
      }
      case 'as': {
        incr();
        const assertType = parseTypeExpression();
        const location = startLocation.merge(assertType.location);
        return new ast.TypeAssertion(location, lhs, assertType);
      }
    }

    if (methodName) {
      const rightAssociative = methodName === '__pow__';
      const operatorLocation = incr().location;
      const rhs = rightAssociative ?
        parsePrec(precedence) :
        parsePrec(precedence + 1);
      const location = startLocation.merge(rhs.location);
      const methodIdentifier = new ast.Identifier(
        operatorLocation, methodName);
      const negate = BinopNegateSet.has(tokenType);
      const swap = BinopSwapSet.has(tokenType);

      // We swap arguments here for the operators that need it,
      // but this might mess with execution order down the line
      // with the solver.
      // TODO: Figure out something more elegant and "correct".
      const core = swap ?
        new ast.MethodCall(location, rhs, methodIdentifier, [lhs]) :
        new ast.MethodCall(location, lhs, methodIdentifier, [rhs]);
      return negate ? logicalNot(location, core) : core;
    }
    if (tokenType === 'in') {
      let operatorLocation = incr().location;
      const rhs = parsePrec(precedence + 1);
      const location = startLocation.merge(rhs.location);
      const methodIdentifier = new ast.Identifier(
        operatorLocation, '__contains__');
      return new ast.MethodCall(location, rhs, methodIdentifier, [lhs]);
    }
    if (tokenType === 'not') {
      let operatorLocation = incr().location;
      operatorLocation = operatorLocation.merge(expect('in').location);
      const rhs = parsePrec(precedence + 1);
      const location = startLocation.merge(rhs.location);
      const methodIdentifier = new ast.Identifier(
        operatorLocation, '__contains__');
      return logicalNot(location, new ast.MethodCall(location, rhs, methodIdentifier, [lhs]));
    }
    if (tokenType === 'is') {
      let operatorLocation = incr().location;
      let negate = false;
      if (at('not')) {
        operatorLocation = operatorLocation.merge(incr().location);
        negate = true;
      }
      const rhs = parsePrec(precedence + 1);
      const location = startLocation.merge(rhs.location);
      const core = new ast.Operation(location, 'is', [lhs, rhs]);
      return negate ? logicalNot(location, core) : core;
    }
    if (tokenType === 'and' || tokenType === 'or') {
      incr();
      const rhs = parsePrec(precedence + 1);
      const location = startLocation.merge(rhs.location);
      return new ast.Operation(location, tokenType, [lhs, rhs]);
    }
    err(`Invalid infix token ${tokenType} (possibly internal error)`);
    return newErr(peek.location);
  }

  function parsePrec(precedence: number): ast.Expression {
    const startLocation = peek.location;
    let expr = parsePrefix();
    while (precedence <= (PrecMap.get(peek.type) || 0)) {
      expr = parseInfix(expr, startLocation);
    }
    return expr;
  }

  function parseExpression(): ast.Expression {
    return parsePrec(1);
  }

  function parseForStatement(): ast.For {
    const startLocation = expect('for').location;
    const identifier = parseIdentifier();
    expect('in');
    const container = parseExpression();
    const body = parseBlock();
    const location = startLocation.merge(body.location);
    return new ast.For(location, identifier, container, body);
  }

  function parseIfStatement(): ast.If {
    const startLocation = expect('if').location;
    const pairs: [ast.Expression, ast.Block][] = [];
    const condition = parseExpression();
    pairs.push([condition, parseBlock()]);
    while (consume('elif')) {
      const condition = parseExpression();
      pairs.push([condition, parseBlock()]);
    }
    const fallback = consume('else') ? parseBlock() : null;
    const location = startLocation.merge(
      fallback ? fallback.location : pairs[pairs.length - 1][1].location);
    return new ast.If(location, pairs, fallback);
  }

  function parseReturnStatement(): ast.Return {
    const startLocation = expect('return').location;
    const expression = at('NEWLINE') || at(';') ?
      newNil(startLocation) :
      parseExpression();
    const location = startLocation.merge(expression.location);
    expectStatementDelimiter();
    return new ast.Return(location, expression);
  }

  function parseWhileStatement(): ast.While {
    const startLocation = expect('while').location;
    const condition = parseExpression();
    const body = parseBlock();
    const location = startLocation.merge(body.location);
    return new ast.While(location, condition, body);
  }

  function parseImportStatement(): ast.Import {
    const startLocation = peek.location;
    let member: ast.Identifier | null = null;
    const fromStmt = consume('from');
    if (!fromStmt) {
      expect('import');
    }
    const moduleID = parseQualifiedIdentifier();
    if (fromStmt) {
      expect('import');
      member = parseIdentifier();
    }
    const alias = consume('as') ? parseIdentifier() : null;
    const location = startLocation.merge(
      alias === null ? moduleID.location : alias.location);
    const importModule = new ast.Import(location, moduleID, member, alias);
    return importModule;
  }

  function parseExpressionStatement(): ast.ExpressionStatement {
    const startLocation = peek.location;
    const expression = parseExpression();
    const location = startLocation.merge(expression.location);
    expectStatementDelimiter();
    return new ast.ExpressionStatement(location, expression);
  }

  function parseBlock(): ast.Block {
    const startLocation = expect(':', true).location;
    while (consume('NEWLINE') || consume(';'));
    if (!consume('INDENT')) {
      // ':' must be followed by an indented block. However,
      // while writing code, it's fairly common to not have a block yet.
      // We should mark this as an error, but this should not kill the parse
      // completely.
      err('Expected INDENT after ":"');
      return new ast.Block(startLocation, []);
    }
    const statements: ast.Statement[] = [];
    while (consume('NEWLINE') || consume(';'));
    while (!at('DEDENT') && !at('EOF')) {
      statements.push(parseDeclaration());
      while (consume('NEWLINE') || consume(';'));
    }
    const location = startLocation.merge(expect('DEDENT').location);
    while (consume('NEWLINE') || consume(';'));
    return new ast.Block(location, statements);
  }

  function parseStatement(): ast.Statement {
    switch (peek.type) {
      case 'for': return parseForStatement();
      case 'if': return parseIfStatement();
      case 'return': return parseReturnStatement();
      case 'while': return parseWhileStatement();
      case 'NEWLINE':
      case ';':
      case 'pass':
        const node = new ast.Nop(peek.location);
        while (consume('pass'));
        expectStatementDelimiter();
        while (consume('NEWLINE') || consume(';'));
        return node;
      default: return parseExpressionStatement();
    }
  }

  function parseDocumentation(): ast.StringLiteral | null {
    if (at('STRING')) {
      const token = expect('STRING');
      return new ast.StringLiteral(
        token.location, <string>token.value);
    }
    return null;
  }

  function parseFieldDeclaration(): ast.Field {
    const startLocation = peek.location;
    const final = consume('final');
    if (!final) {
      expect('var');
    }
    const identifier = parseIdentifier();
    const typeExpression = parseTypeExpression();
    const documentation = parseDocumentation();
    const location = startLocation.merge(typeExpression.location);
    expectStatementDelimiter();
    return new ast.Field(location, final, identifier, typeExpression, documentation);
  }

  function parseClassDeclaration(): ast.Class {
    const startLocation = expect('class').location;
    const identifier = parseIdentifier();
    const typeParameters = at('[') ? parseTypeParameters() : [];
    const bases = [];
    if (consume('(')) {
      if (!at(')')) {
        do {
          bases.push(parseTypeExpression());
        } while (consume(','));
      }
      expect(')');
    }
    expect(':');
    while (consume('NEWLINE') || consume(';'));
    expect('INDENT');
    while (consume('NEWLINE') || consume(';'));
    let documentation: ast.StringLiteral | null = null;
    if (at('STRING')) {
      const token = expect('STRING');
      documentation = new ast.StringLiteral(
        token.location, <string>token.value);
    }
    while (consume('NEWLINE') || consume(';'));
    while (consume('pass'));
    while (consume('NEWLINE') || consume(';'));
    const staticMethods = [];
    while (at('static')) {
      expect('static');
      staticMethods.push(parseFunctionDeclaration());
      while (consume('NEWLINE') || consume(';'));
    }
    const fields = [];
    while (at('var') || at('final')) {
      fields.push(parseFieldDeclaration());
      while (consume('NEWLINE') || consume(';'));
    }
    const methods = [];
    while (at('def')) {
      methods.push(parseFunctionDeclaration());
      while (consume('NEWLINE') || consume(';'));
    }
    const endLocation = expect('DEDENT').location;
    while (consume('NEWLINE') || consume(';'));
    const location = startLocation.merge(endLocation);

    return new ast.Class(
      location,
      identifier,
      typeParameters,
      bases,
      documentation,
      staticMethods,
      fields,
      methods);
  }

  function parseFunctionStub(): ast.Function {
    const startLocation = expect('def').location;
    const identifier = parseIdentifier();
    const typeParameters = at('[') ? parseTypeParameters() : [];
    const parameters = parseParameters();
    const returnType =
      (at(':') || at('NEWLINE') || at('EOF')) ?
        null : parseTypeExpression();
    const afterParametersLocation = peek.location;
    let body = new ast.Block(startLocation, []);
    let documentation: ast.StringLiteral | null = null;
    if (at(':')) {
      body = parseBlock();
      const bodyStatements = body.statements;
      if (bodyStatements.length > 0) {
        const firstStatement = body.statements[0];
        if (firstStatement instanceof ast.ExpressionStatement) {
          const innerExpression = firstStatement.expression;
          if (innerExpression instanceof ast.StringLiteral) {
            documentation = innerExpression;
            body.statements.shift();
          }
        }
      }
      if (body.statements.length > 0) {
        err(
          `Body of a function stub must be empty (potentially aside ` +
          `from documentation), but found a non-empty body`);
      }
    }
    let location = startLocation.
      merge(afterParametersLocation).
      merge(returnType?.location).
      merge(body.location);
    if (documentation) {
      location = location.merge(documentation.location);
    }
    return new ast.Function(
      location,
      identifier,
      typeParameters,
      parameters,
      returnType,
      documentation,
      body);
  }

  function parseTraitDeclaration(): ast.Trait {
    const startLocation = expect('trait').location;
    const identifier = parseIdentifier();
    const typeParameters = at('[') ? parseTypeParameters() : [];
    const bases = [];
    if (consume('(')) {
      if (!at(')')) {
        do {
          bases.push(parseTypeExpression());
        } while (consume(','));
      }
      expect(')');
    }
    expect(':');
    while (consume('NEWLINE') || consume(';'));
    expect('INDENT');
    while (consume('NEWLINE') || consume(';'));
    let documentation: ast.StringLiteral | null = null;
    if (at('STRING')) {
      const token = expect('STRING');
      documentation = new ast.StringLiteral(
        token.location, <string>token.value);
    }
    while (consume('NEWLINE') || consume(';'));
    while (consume('pass'));
    while (consume('NEWLINE') || consume(';'));
    const fields = [];
    while (at('var') || at('final')) {
      fields.push(parseFieldDeclaration());
      while (consume('NEWLINE') || consume(';'));
    }
    const methods = [];
    while (at('def')) {
      methods.push(parseFunctionStub());
      while (consume('NEWLINE') || consume(';'));
    }
    const endLocation = expect('DEDENT').location;
    while (consume('NEWLINE') || consume(';'));
    const location = startLocation.merge(endLocation);

    return new ast.Trait(
      location, identifier, typeParameters, bases, documentation, fields, methods);
  }

  function parseParameter(): ast.Parameter {
    const identifier = parseIdentifier();
    const type = maybeAtTypeExpression() ? parseTypeExpression() : null;
    const defaultValue = consume('=') ? parseExpression() : null;
    const location = identifier.location.merge(
      defaultValue?.location || type?.location || identifier.location);
    return new ast.Parameter(location, identifier, type, defaultValue);
  }

  function parseParameters(): ast.Parameter[] {
    const parameters = [];
    expect('(');
    while (!at(')') && !at('EOF')) {
      parameters.push(parseParameter());
      if (!consume(',')) {
        break;
      }
    }
    expect(')');
    return parameters;
  }

  function parseTypeParameter(): ast.TypeParameter {
    const startLocation = peek.location;
    const identifier = parseIdentifier();
    const boundExpression = at('IDENTIFIER') ? parseTypeExpression() : null;
    return new ast.TypeParameter(startLocation, identifier, boundExpression);
  }

  function parseTypeParameters(): ast.TypeParameter[] {
    const tps = [];
    expect('[');
    while (at('IDENTIFIER')) {
      tps.push(parseTypeParameter());
      if (!consume(',')) {
        break;
      }
    }
    expect(']');
    return tps;
  }

  function parseDecoratorApplication(): ast.Statement {
    const atLocation = expect('@').location;
    let decorator = parsePrec(PREC_PRIMARY);
    let methodName: ast.Identifier|null = null;
    const dotLocation = peek.location;
    if (consume('.')) {
      if (at('IDENTIFIER')) {
        methodName = parseIdentifier();
      } else {
        // This is technically an error, but we don't want parse to fail completely
        // because a failed parse means no completions
        const followLocation = peek.location;
        decorator = new ast.Dot(
          dotLocation.merge(followLocation),
          decorator,
          dotLocation,
          followLocation);
      }
    }
    expectStatementDelimiter();
    const func = parseFunctionDeclaration(false);
    const getfunc = new ast.GetVariable(func.identifier.location, func.identifier);
    const location = atLocation.merge(func.location);
    return new ast.Block(atLocation, [
      func,
      new ast.ExpressionStatement(location,
        methodName ?
          new ast.MethodCall(location, decorator, methodName, [getfunc], []) :
          new ast.FunctionCall(location, decorator, [getfunc], [], null)),
    ]);
  }

  function parseFunctionDeclaration(nameRequired=true): ast.Function {
    const startLocation = expect('def').location;
    const identifier =
      (at('IDENTIFIER') || nameRequired) ?
        parseIdentifier() : new ast.Identifier(startLocation, '<def>');
    const typeParameters = at('[') ? parseTypeParameters() : [];
    const parameters = parseParameters();
    const returnType =
      (at(':') || at('NEWLINE') || at('EOF')) ?
        null : parseTypeExpression();
    const body = parseBlock();
    let documentation: ast.StringLiteral | null = null;
    const bodyStatements = body.statements;
    if (bodyStatements.length > 0) {
      const firstStatement = body.statements[0];
      if (firstStatement instanceof ast.ExpressionStatement) {
        const innerExpression = firstStatement.expression;
        if (innerExpression instanceof ast.StringLiteral) {
          documentation = innerExpression;
        }
      }
    }
    const location = startLocation.merge(body.location);
    return new ast.Function(
      location,
      identifier,
      typeParameters,
      parameters,
      returnType,
      documentation,
      body);
  }

  function parseVariableDeclaration(): ast.Variable {
    const startLocation = peek.location;
    const final = consume('final');
    if (!final) expect('var');
    const identifier = parseIdentifier();
    const typeExpression = at('=') || at('STRING') ?
      null :
      parseTypeExpression();
    const itemDoc = parseDocumentation()?.value || null;
    expect('=');
    const value = parseExpression();
    const location = startLocation.merge(value.location);
    return new ast.Variable(location, itemDoc, final, identifier, typeExpression, value);
  }

  function parseDeclaration(): ast.Statement {
    switch (peek.type) {
      case 'def': return parseFunctionDeclaration();
      case 'final': case 'var': return parseVariableDeclaration();
      case '@': return parseDecoratorApplication();
      default: return parseStatement();
    }
  }

  function parseModuleLevelDeclaration(): ast.Statement {
    switch (peek.type) {
      case 'class': return parseClassDeclaration();
      case 'trait': return parseTraitDeclaration();
      default: return parseDeclaration();
    }
  }

  const startLocation = getloc();
  var documentation: string | null = null;
  const imports: ast.Import[] = [];
  const statements: ast.Statement[] = [];

  while (consume('NEWLINE') || consume(';'));
  if (at('STRING')) {
    documentation = incr().value as string;
  }
  while (consume('NEWLINE') || consume(';'));
  while (at('import') || at('from')) {
    imports.push(parseImportStatement());
    expectStatementDelimiter();
  }
  while (!at('EOF')) {
    statements.push(parseModuleLevelDeclaration());
  }

  const location = startLocation.merge(getloc());
  return new ast.File(location, documentation, imports, statements, errors);
}
