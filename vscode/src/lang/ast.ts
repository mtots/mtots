import { MError } from "./error";
import { MLocation } from "./location";

type Operator = (
  'is'      |
  'not'     |
  'and'     |
  'or'      |
  'if'
)

export abstract class Ast {
  readonly location: MLocation;
  constructor(location: MLocation) {
    this.location = location;
  }
  toJSON() {
    const blob: any = {
      className: this.constructor.name,
      ...this
    };
    delete blob.location;
    return blob;
  }
}

export class Identifier extends Ast {
  readonly name: string;
  constructor(location: MLocation, name: string) {
    super(location);
    this.name = name;
  }
  toString() {
    return `Identifier(${this.name})`;
  }
}

export class QualifiedIdentifier extends Ast {
  readonly parent: QualifiedIdentifier | null;
  readonly identifier: Identifier;
  constructor(
      location: MLocation,
      parent: QualifiedIdentifier | null,
      identifier: Identifier) {
    super(location);
    this.parent = parent;
    this.identifier = identifier;
  }
  toString() {
    const parent = this.parent;
    return parent ?
      `${parent}.${this.identifier.name}` :
      this.identifier.name;
  }
}

export abstract class Statement extends Ast {
  abstract accept(visitor: StatementVisitor): void;
}

export abstract class Expression extends Ast {
  abstract accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R;
}

/**
 * All type expressions boil down to operator[Arg1, Arg2, ...]
 * in some form.
 *
 * Some syntactic sugar:
 *   SomeType?  => Optional[SomeType]
 *   A | B | C  => Union[A, Union[B, C]]
 *
 */
export class TypeExpression extends Ast {
  readonly parentIdentifier: Identifier | null;
  readonly baseIdentifier: Identifier;
  readonly args: TypeExpression[];
  constructor(
      location: MLocation,
      parentIdentifier: Identifier | null,
      baseIdentifier: Identifier,
      args: TypeExpression[]) {
    super(location);
    this.parentIdentifier = parentIdentifier;
    this.baseIdentifier = baseIdentifier;
    this.args = args;
  }
}

export class File extends Ast {
  readonly imports: Import[];
  readonly documentation: string | null;
  readonly statements: Statement[];
  readonly syntaxErrors: MError[];
  constructor(
      location: MLocation,
      documentation: string | null,
      imports: Import[],
      statements: Statement[],
      syntaxErrors: MError[]) {
    super(location);
    this.documentation = documentation;
    this.imports = imports;
    this.statements = statements;
    this.syntaxErrors = syntaxErrors;
  }
}

export class Nop extends Statement {
  constructor(location: MLocation) {
    super(location);
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitNop(this);
  }
}

export class TypeParameter extends Ast {
  readonly identifier: Identifier
  readonly bound: TypeExpression | null;
  constructor(
      location: MLocation,
      identifier: Identifier,
      bound: TypeExpression | null) {
    super(location);
    this.identifier = identifier;
    this.bound = bound;
  }
}

export class Parameter extends Ast {
  readonly identifier: Identifier;
  readonly typeExpression: TypeExpression | null;
  readonly defaultValue: Expression | null;
  constructor(
      location: MLocation,
      identifier: Identifier,
      typeExpression: TypeExpression | null,
      defaultValue: Expression | null) {
    super(location);
    this.identifier = identifier;
    this.typeExpression = typeExpression;
    this.defaultValue = defaultValue;
  }
}

export class Function extends Statement {
  readonly identifier: Identifier;
  readonly typeParameters: TypeParameter[];
  readonly parameters: Parameter[];
  readonly returnType: TypeExpression | null;
  readonly documentation: StringLiteral | null;
  readonly body: Block;
  constructor(
      location: MLocation,
      identifier: Identifier,
      typeParameters: TypeParameter[],
      parameters: Parameter[],
      returnType: TypeExpression | null,
      documentation: StringLiteral | null,
      body: Block) {
    super(location);
    this.identifier = identifier;
    this.typeParameters = typeParameters;
    this.parameters = parameters;
    this.returnType = returnType;
    this.documentation = documentation;
    this.body = body;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitFunction(this);
  }
}

export class Field extends Ast {
  readonly final: boolean;
  readonly identifier: Identifier;
  readonly typeExpression: TypeExpression;
  readonly documentation: StringLiteral | null;
  constructor(
      location: MLocation,
      final: boolean,
      identifier: Identifier,
      typeExpression: TypeExpression,
      documentation: StringLiteral | null) {
    super(location);
    this.final = final;
    this.identifier = identifier;
    this.typeExpression = typeExpression;
    this.documentation = documentation;
  }
}

export class Class extends Statement {
  readonly identifier: Identifier;
  readonly typeParameters: TypeParameter[];
  readonly bases: TypeExpression[];
  readonly documentation: StringLiteral | null;
  readonly staticMethods: Function[];
  readonly fields: Field[];
  readonly methods: Function[];
  constructor(
      location: MLocation,
      identifier: Identifier,
      typeParameters: TypeParameter[],
      bases: TypeExpression[],
      documentation: StringLiteral | null,
      staticMethods: Function[],
      fields: Field[],
      methods: Function[]) {
    super(location);
    this.identifier = identifier;
    this.typeParameters = typeParameters;
    this.bases = bases;
    this.documentation = documentation;
    this.staticMethods = staticMethods;
    this.fields = fields;
    this.methods = methods;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitClass(this);
  }
}

export class Trait extends Statement {
  readonly identifier: Identifier;
  readonly typeParameters: TypeParameter[];
  readonly bases: TypeExpression[];
  readonly documentation: StringLiteral | null;
  readonly fields: Field[];
  readonly methods: Function[];
  constructor(
      location: MLocation,
      identifier: Identifier,
      typeParameters: TypeParameter[],
      bases: TypeExpression[],
      documentation: StringLiteral | null,
      fields: Field[],
      methods: Function[]) {
    super(location);
    this.identifier = identifier;
    this.typeParameters = typeParameters;
    this.bases = bases;
    this.documentation = documentation;
    this.fields = fields;
    this.methods = methods;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitTrait(this);
  }
}

export class Import extends Ast {
  readonly module: QualifiedIdentifier;
  readonly member: Identifier | null;
  readonly alias: Identifier;
  constructor(
      location: MLocation,
      module: QualifiedIdentifier,
      member: Identifier | null,
      alias: Identifier | null) {
    super(location);
    this.module = module;
    this.member = member;
    this.alias = alias || member || module.identifier;
  }
}

/** Variable Declaration */
export class Variable extends Statement {
  readonly documentation: string | null;
  readonly final: boolean;
  readonly identifier: Identifier;
  readonly typeExpression: TypeExpression | null;
  readonly valueExpression: Expression;
  constructor(
      location: MLocation,
      documentation: string | null,
      final: boolean,
      identifier: Identifier,
      typeExpression: TypeExpression | null,
      valueExpression: Expression) {
    super(location);
    this.documentation = documentation;
    this.final = final;
    this.identifier = identifier;
    this.typeExpression = typeExpression;
    this.valueExpression = valueExpression;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitVariable(this);
  }
}

export class While extends Statement {
  readonly condition: Expression;
  readonly body: Block;
  constructor(location: MLocation, condition: Expression, body: Block) {
    super(location);
    this.condition = condition;
    this.body = body;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitWhile(this);
  }
}

export class For extends Statement {
  readonly variable: Identifier;
  readonly container: Expression;
  readonly body: Block;
  constructor(
      location: MLocation,
      variable: Identifier,
      container: Expression,
      body: Block) {
    super(location);
    this.variable = variable;
    this.container = container;
    this.body = body;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitFor(this);
  }
}

export class If extends Statement {
  readonly pairs: [Expression, Block][];
  readonly fallback: Block | null;
  constructor(
      location: MLocation,
      pairs: [Expression, Block][],
      fallback: Block | null) {
    super(location);
    this.pairs = pairs;
    this.fallback = fallback;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitIf(this);
  }
}

export class Block extends Statement {
  readonly statements: Statement[];
  constructor(location: MLocation, statements: Statement[]) {
    super(location);
    this.statements = statements;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitBlock(this);
  }
}

export class Return extends Statement {
  readonly expression: Expression;
  constructor(location: MLocation, expression: Expression) {
    super(location);
    this.expression = expression;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitReturn(this);
  }
}

export class ExpressionStatement extends Statement {
  readonly expression: Expression;
  constructor(location: MLocation, expression: Expression) {
    super(location);
    this.expression = expression;
  }

  accept(visitor: StatementVisitor): void {
    return visitor.visitExpressionStatement(this);
  }
}

export class GetVariable extends Expression {
  readonly identifier: Identifier;
  constructor(location: MLocation, identifier: Identifier) {
    super(location);
    this.identifier = identifier;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitGetVariable(this, arg);
  }
}

export class SetVariable extends Expression {
  readonly identifier: Identifier;
  readonly value: Expression;
  constructor(location: MLocation, identifier: Identifier, value: Expression) {
    super(location);
    this.identifier = identifier;
    this.value = value;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitSetVariable(this, arg);
  }
}

export class ErrorExpression extends Expression {
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitErrorExpression(this, arg);
  }
}

export abstract class Literal<T> extends Expression {
  readonly value: T;
  constructor(location: MLocation, value: T) {
    super(location);
    this.value = value;
  }
}

export class NilLiteral extends Literal<null> {
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitNilLiteral(this, arg);
  }
}
export class BoolLiteral extends Literal<boolean> {
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitBoolLiteral(this, arg);
  }
}
export class NumberLiteral extends Literal<number> {
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitNumberLiteral(this, arg);
  }
}
export class StringLiteral extends Literal<string> {
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitStringLiteral(this, arg);
  }
}

export class TypeAssertion extends Expression {
  readonly expression: Expression;
  readonly typeExpression: TypeExpression;
  constructor(
      location: MLocation,
      expression: Expression,
      typeExpression: TypeExpression) {
    super(location);
    this.expression = expression;
    this.typeExpression = typeExpression;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitTypeAssertion(this, arg);
  }
}

export class ListDisplay extends Expression {
  readonly items: Expression[];
  constructor(location: MLocation, items: Expression[]) {
    super(location);
    this.items = items;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitListDisplay(this, arg);
  }
}

export class FrozenListDisplay extends Expression {
  readonly items: Expression[];
  constructor(location: MLocation, items: Expression[]) {
    super(location);
    this.items = items;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitFrozenListDisplay(this, arg);
  }
}

export class TupleDisplay extends Expression {
  readonly identifier: Identifier;
  readonly items: Expression[];
  constructor(location: MLocation, identifier: Identifier, items: Expression[]) {
    super(location);
    this.identifier = identifier;
    this.items = items;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitTupleDisplay(this, arg);
  }
}

export class DictDisplay extends Expression {
  readonly pairs: [Expression, Expression][];
  constructor(location: MLocation, pairs: [Expression, Expression][]) {
    super(location);
    this.pairs = pairs;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitDictDisplay(this, arg);
  }
}

export class FrozenDictDisplay extends Expression {
  readonly pairs: [Expression, Expression][];
  constructor(location: MLocation, pairs: [Expression, Expression][]) {
    super(location);
    this.pairs = pairs;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitFrozenDictDisplay(this, arg);
  }
}

export class Lambda extends Expression {
  readonly parameters: Parameter[];
  readonly returnType: TypeExpression | null;
  readonly body: Expression;
  constructor(
      location: MLocation,
      parameters: Parameter[],
      returnType: TypeExpression | null,
      body: Expression) {
    super(location);
    this.parameters = parameters;
    this.returnType = returnType;
    this.body = body;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitLambda(this, arg);
  }
}

export class FunctionCall extends Expression {
  readonly func: Expression;
  readonly args: Expression[];
  readonly kwargs: [Identifier, Expression][];

  /**
   * argLocations are the spaces where the arguments are.
   * This overlaps with, but are generally broader than the
   * locations of the expressions themselves.
   * When available, these values are used to generate signature
   * help providers.
   */
  readonly argLocations: MLocation[] | null;

  constructor(
      location: MLocation,
      func: Expression,
      args: Expression[],
      kwargs: [Identifier, Expression][],
      argLocations: MLocation[] | null) {
    super(location);
    this.func = func;
    this.args = args;
    this.kwargs = kwargs;
    this.argLocations = argLocations;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitFunctionCall(this, arg);
  }
  toJSON() {
    const blob = super.toJSON();
    delete blob.argLocations;
    return blob;
  }
}

export class MethodCall extends Expression {
  readonly owner: Expression;
  readonly identifier: Identifier;
  readonly args: Expression[];
  readonly kwargs: [Identifier, Expression][];

  /**
   * argLocations are the spaces where the arguments are.
   * This overlaps with, but are generally broader than the
   * locations of the expressions themselves.
   * When available, these values are used to generate signature
   * help providers.
   */
  readonly argLocations: MLocation[] | null;

  constructor(
      location: MLocation,
      owner: Expression,
      identifier: Identifier,
      args: Expression[],
      kwargs: [Identifier, Expression][] = [],
      argLocations: MLocation[] | null = null) {
    super(location);
    this.owner = owner;
    this.identifier = identifier;
    this.args = args;
    this.kwargs = kwargs;
    this.argLocations = argLocations;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitMethodCall(this, arg);
  }
  toJSON() {
    const blob = super.toJSON();
    delete blob.argLocations;
    return blob;
  }
}

export class GetField extends Expression {
  readonly owner: Expression;
  readonly identifier: Identifier;
  constructor(location: MLocation, owner: Expression, identifier: Identifier) {
    super(location);
    this.owner = owner;
    this.identifier = identifier;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitGetField(this, arg);
  }
}

export class SetField extends Expression {
  readonly owner: Expression;
  readonly identifier: Identifier;
  readonly value: Expression;
  constructor(
      location: MLocation,
      owner: Expression,
      identifier: Identifier,
      value: Expression) {
    super(location);
    this.owner = owner;
    this.identifier = identifier;
    this.value = value;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitSetField(this, arg);
  }
}

/**
 * Syntax error - this AST node indicates a '.' after an expression
 * but IDENTIFIER is missing after the '.'.
 * This node is purely for implementing autocomplete.
 */
export class Dot extends Expression {
  readonly owner: Expression;
  readonly dotLocation: MLocation;

  /** Location of the token immediately following the '.' */
  readonly followLocation: MLocation;
  constructor(
      location: MLocation,
      owner: Expression,
      dotLocation: MLocation,
      followLocation: MLocation) {
    super(location);
    this.owner = owner;
    this.dotLocation = dotLocation;
    this.followLocation = followLocation;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitDot(this, arg);
  }
}

export class Operation extends Expression {
  readonly op: Operator;
  readonly args: Expression[];
  constructor(
      location: MLocation,
      op: Operator,
      args: Expression[]) {
    super(location);
    this.op = op;
    this.args = args;
  }
  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitOperation(this, arg);
  }
}

export class Raise extends Expression {
  readonly exception: Expression;
  constructor(location: MLocation, exception: Expression) {
    super(location);
    this.exception = exception;
  }

  accept<A, R>(visitor: ExpressionVisitor<A, R>, arg: A): R {
    return visitor.visitRaise(this, arg);
  }
}

export interface ExpressionVisitor<A, R> {
  visitErrorExpression(e: ErrorExpression, arg: A): R;
  visitGetVariable(e: GetVariable, arg: A): R;
  visitSetVariable(e: SetVariable, arg: A): R;
  visitNilLiteral(e: NilLiteral, arg: A): R;
  visitBoolLiteral(e: BoolLiteral, arg: A): R;
  visitNumberLiteral(e: NumberLiteral, arg: A): R;
  visitStringLiteral(e: StringLiteral, arg: A): R;
  visitTypeAssertion(e: TypeAssertion, arg: A): R;
  visitListDisplay(e: ListDisplay, arg: A): R;
  visitFrozenListDisplay(e: FrozenListDisplay, arg: A): R;
  visitTupleDisplay(e: TupleDisplay, arg: A): R;
  visitDictDisplay(e: DictDisplay, arg: A): R;
  visitFrozenDictDisplay(e: FrozenDictDisplay, arg: A): R;
  visitLambda(e: Lambda, arg: A): R;
  visitFunctionCall(e: FunctionCall, arg: A): R;
  visitMethodCall(e: MethodCall, arg: A): R;
  visitGetField(e: GetField, arg: A): R;
  visitSetField(e: SetField, arg: A): R;
  visitDot(e: Dot, arg: A): R;
  visitOperation(e: Operation, arg: A): R;
  visitRaise(e: Raise, arg: A): R;
}

export interface StatementVisitor {
  visitNop(s: Nop): void;
  visitFunction(s: Function): void;
  visitClass(s: Class): void;
  visitTrait(s: Trait): void;
  visitVariable(s: Variable): void;
  visitWhile(s: While): void;
  visitFor(s: For): void;
  visitIf(s: If): void;
  visitBlock(s: Block): void;
  visitReturn(s: Return): void;
  visitExpressionStatement(s: ExpressionStatement): void;
}
