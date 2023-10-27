/// Intermediate Representation

import * as ast from "./ast";
import { MError } from "./error";
import { MPosition } from "./position";
import { Scope, Variable } from "./ir/scope";
import { Usage } from "./ir/usage";
import { SigHelp } from "./ir/sighelp";
import { warn } from "../debug";
import { BUILTIN_LOCATION } from "./location";
import { CompletionPoint } from "./ir/cpoint";


export class Module {
  readonly name: string;
  readonly file: ast.File;
  readonly scope: Scope;
  readonly errors: MError[];
  readonly usages: Usage[] = [];
  readonly sighelps: SigHelp[] = [];
  readonly cpoints: CompletionPoint[] = [];
  constructor(name: string, file: ast.File, scope: Scope) {
    this.name = name;
    this.file = file;
    this.scope = scope;
    this.errors = [...file.syntaxErrors];
  }

  findUsage(position: MPosition): Usage | null {
    for (const usage of this.usages) {
      if (usage.identifier.location.range.contains(position)) {
        return usage;
      }
    }
    return null;
  }

  findSignatureHelper(position: MPosition): SigHelp | null {
    let bestSoFar: SigHelp | null = null;
    for (const sh of this.sighelps) {
      const shRange = sh.location.range;
      if (shRange.start.le(position) && position.le(shRange.end) &&
        (bestSoFar == null ||
          bestSoFar.location.range.start.lt(shRange.start))) {
        bestSoFar = sh;
      }
    }
    return bestSoFar;
  }

  findCompletionPoint(position: MPosition): CompletionPoint | null {
    for (const cp of this.cpoints) {
      const cpRange = cp.location.range;
      if (cpRange.start.le(position) && position.le(cpRange.end)) {
        return cp;
      }
    }
    return null;
  }
}

export type ConstValue = null | boolean | number | string;

export abstract class Type {
  readonly __tagType = 0;
  _listType: ListType<this> | null = null;
  _frozenListType: FrozenListType<this> | null = null;
  _iterationType: IterationType<this> | null = null;
  _iterableType: IterableType<this> | null = null;
  abstract _equals(other: Type): boolean;
  abstract toString(): string;
  equals(other: Type): boolean {
    return this === other || this._equals(other);
  }
  getIterType(): Type | null {
    if (this instanceof AnyType) {
      return this;
    }
    if (this instanceof IterableType) {
      return this.itemType;
    }
    if (this instanceof FunctionType &&
      this.typeParameters.length === 0 &&
      this.parameters.length === 0 &&
      this.returnType instanceof IterationType) {
      return this.returnType.itemType;
    }
    const getter = this.getMethod('__iter__')?.type.asFunctionType();
    if (!getter || getter.typeParameters.length > 0 || getter.parameters.length > 0) {
      return null;
    }
    const iterator = getter.returnType.asFunctionType();
    if (!iterator) {
      return null;
    }
    if (iterator.typeParameters.length === 0 &&
      iterator.parameters.length === 0 &&
      iterator.returnType instanceof IterationType) {
      return iterator.returnType.itemType;
    }
    return null;
  }
  getField(fieldName: string): Variable | null {
    return null;
  }
  getFieldNames(): string[] {
    return [];
  }
  getMethod(methodName: string): Variable | null {
    return null;
  }
  getMethodNames(): string[] {
    return [];
  }
  getListType(): ListType<this> {
    const type = this._listType;
    if (type) {
      return type;
    }
    const listType = new ListType(this);
    this._listType = listType;
    return listType;
  }
  getFrozenListType(): FrozenListType<this> {
    const type = this._frozenListType;
    if (type) {
      return type;
    }
    const listType = new FrozenListType(this);
    this._frozenListType = listType;
    return listType;
  }
  getIterationType(): IterationType {
    const type = this._iterationType;
    if (type) {
      return type;
    }
    const iterationType = new IterationType(this);
    this._iterationType = iterationType;
    return iterationType;
  }
  getIterableType(): IterableType<this> {
    const type = this._iterableType;
    if (type) {
      return type;
    }
    const iterableType = new IterableType(this);
    this._iterableType = iterableType;
    return iterableType;
  }
  getOptionalType(): Type {
    return UnionType.of([this, NIL_TYPE]);
  }
  /** If an instance of this type is callable, return its function type.
   * If this type is not callable, returns null.
   * Functions are not the only callable types (e.g. Class instances
   * have their signatures controlled by __init__, and instances can
   * be callable if they have a __call__ method).
   */
  asFunctionType(): FunctionType | null {
    return null;
  }

  isAssignableTo(other: Type): boolean {
    if (this instanceof LiteralType) {
      return this.type.isAssignableTo(other);
    }
    if (other === ANY_TYPE || this === NEVER_TYPE || this.equals(other)) {
      return true;
    }
    if (other instanceof UnionType) {
      return other.types.some(entry => this.isAssignableTo(entry));
    }
    if (this instanceof UnionType) {
      return this.types.every(entry => entry.isAssignableTo(other));
    }
    if (other instanceof IterationType) {
      if (STOP_ITERATION_TYPE.equals(this)) {
        return true;
      } else if (this instanceof IterationType) {
        return this.itemType.isAssignableTo(other.itemType);
      } else {
        return this.isAssignableTo(other.itemType);
      }
    }
    if (other instanceof IterationType && (
      STOP_ITERATION_TYPE.equals(this) ||
      this.isAssignableTo(other.itemType))) {
      return true;
    }
    if (other === CLASS_TYPE && this instanceof ClassType) {
      return true;
    }
    if (other instanceof IterableType) {
      const iterType = this.getIterType();
      return iterType?.isAssignableTo(other.itemType) || false;
    }
    if (other instanceof FunctionType) {
      if (other.typeParameters.length > 0) {
        return false;
      }
      const func = this.asFunctionType();
      if (!func) {
        return false;
      }
      const funcMaxArgc = getMaxArgc(func.parameters);
      const funcMinArgc = getMinArgc(func.parameters);
      const otherMaxArgc = getMaxArgc(other.parameters);
      const otherMinArgc = getMinArgc(other.parameters);
      if (otherMaxArgc > funcMaxArgc || otherMinArgc < funcMinArgc) {
        return false;
      }
      if (!func.returnType.isAssignableTo(other.returnType)) {
        return false;
      }
      for (let i = 0; i < otherMaxArgc; i++) {
        if (!other.parameters[i].type.isAssignableTo(func.parameters[i].type)) {
          return false;
        }
      }
      return true;
    }
    if (other instanceof InstanceType && other.isTrait()) {
      for (const methodSpec of other.methods.values()) {
        const method = this.getMethod(methodSpec.identifier.name);
        if (!method) {
          return false;
        }
        if (!method.type.isAssignableTo(methodSpec.type)) {
          return false;
        }
      }
      return true;
    }
    if (other instanceof FrozenDictType && this instanceof FrozenDictLiteralType) {
      return this.type.isAssignableTo(other);
    }
    if (other instanceof PrimitiveType && this instanceof LiteralType) {
      return this.type.isAssignableTo(other);
    }
    if (this instanceof InstanceType) {
      for (const base of this.bases) {
        if (base.isAssignableTo(other)) {
          return true;
        }
      }
    }
    return false;
  }
  merge(other: Type): Type {
    const lhs = this.toNonLiteralType();
    const rhs = other.toNonLiteralType();
    if (lhs.isAssignableTo(rhs)) {
      return rhs;
    }
    if (rhs.isAssignableTo(lhs)) {
      return lhs;
    }
    return ANY_TYPE;
  }
  assumeTrue(): Type {
    return this;
  }
  assumeFalse(): Type {
    return this;
  }
  isClassType(): boolean {
    return this instanceof ClassType;
  }
  isModuleType(): boolean {
    return this instanceof ModuleType;
  }

  /**
   * Convert `this` into a Type that is not
   * `LiteralType` or `FrozenDictLiteralType`
   */
  toNonLiteralType(): Type {
    return this;
  }
}

class AnyType extends Type {
  static readonly INSTANCE = new AnyType();
  private constructor() {
    super();
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return 'Any';
  }
  getField(fieldName: string): Variable {
    return ANY_PROPERTY;
  }
  getMethod(methodName: string): Variable {
    return ANY_PROPERTY;
  }
  getIterType(): Type {
    return this;
  }
}

class NeverType extends Type {
  static readonly INSTANCE = new NeverType();
  private constructor() {
    super();
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return 'Never';
  }
}

export class PrimitiveType extends Type {
  readonly name: string;
  readonly methods = new Map<string, Variable<FunctionType>>();
  readonly staticMethods = new Map<string, Variable<FunctionType>>();
  readonly typeType = new PrimitiveTypeType(this);
  constructor(name: string) {
    super();
    this.name = name;
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return this.name;
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    return this.methods.get(methodName) || null;
  }
  getMethodNames(): string[] {
    return Array.from(this.methods.keys());
  }
}

export class LiteralType extends Type {
  readonly type: PrimitiveType;
  readonly value: ConstValue;
  constructor(type: PrimitiveType, value: ConstValue) {
    super();
    this.type = type;
    this.value = value;
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return this.type.toString();
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    return this.type.getMethod(methodName);
  }
  getMethodNames(): string[] {
    return this.type.getMethodNames();
  }
  toNonLiteralType(): Type {
    return this.type;
  }
}

export class PrimitiveTypeType extends Type {
  readonly instanceType: PrimitiveType;
  constructor(instanceType: PrimitiveType) {
    super();
    this.instanceType = instanceType;
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return `class ${this.instanceType.name}`;
  }
  getMethod(methodName: string): Variable<Type> | null {
    return this.instanceType.staticMethods.get(methodName) || null;
  }
}

export class ModuleType extends Type {
  readonly module: Module;
  constructor(module: Module) {
    super();
    this.module = module;
  }
  _equals(other: Type): boolean {
    return other instanceof ModuleType && this.module === other.module;
  }
  toString(): string {
    return `import ${this.module.name}`;
  }
  getField(fieldName: string): Variable | null {
    return this.module.scope.map.get(fieldName) || null;
  }
  getMethod(methodName: string): Variable | null {
    return this.module.scope.map.get(methodName) || null;
  }
  getFieldNames(): string[] {
    return Array.from(this.module.scope.map.keys());
  }
  getMethodNames(): string[] {
    return this.getFieldNames();
  }
}

export type BaseInstanceType = BoundInstanceType | InstanceType;

export class BoundInstanceType extends Type {
  readonly instanceType: InstanceType;
  readonly typeArgs: Type[];
  readonly typeBinder: TypeBinder;
  readonly boundMethods = new Map<string, Variable<FunctionType>>();
  readonly boundFields = new Map<string, Variable>();
  constructor(
    instanceType: InstanceType,
    typeArgs: Type[]) {
    super();
    this.instanceType = instanceType;
    this.typeArgs = typeArgs;
    this.typeBinder = (() => {
      const argc = Math.min(typeArgs.length, instanceType.typeParameters.length);
      const typeMap = new Map<TypeVariableInstanceType, Type>();
      for (let i = 0; i < argc; i++) {
        typeMap.set(instanceType.typeParameters[i].type.instanceType, typeArgs[i]);
      }
      const bindSet = new Set(
        instanceType.typeParameters.map(tp => tp.type.instanceType));
      return newTypeBinder(typeMap, bindSet);
    })();
  }
  _equals(other: Type): boolean {
    if (this === other) {
      return true;
    }
    if (!(other instanceof BoundInstanceType)) {
      return false;
    }
    if (this.typeArgs.length !== other.typeArgs.length) {
      return false;
    }
    for (let i = 0; i < this.typeArgs.length; i++) {
      if (!this.typeArgs[i].equals(other.typeArgs[i])) {
        return false;
      }
    }
    return true;
  }
  toString(): string {
    return `${this.instanceType.identifier.name}[${this.typeArgs.join(', ')}]`;
  }
  getField(fieldName: string): Variable | null {
    const cachedResult = this.boundFields.get(fieldName);
    if (cachedResult) {
      return cachedResult;
    }
    const unboundField = this.instanceType.fields.get(fieldName);
    if (!unboundField) {
      return null;
    }
    const boundFieldType = this.typeBinder.bind(unboundField.type);
    const boundField = new Variable(
      unboundField.final,
      unboundField.identifier,
      boundFieldType,
      unboundField.documentation);
    this.boundFields.set(boundField.identifier.name, boundField);
    return boundField;
  }
  getFieldNames(): string[] {
    return this.instanceType.getFieldNames();
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    const cachedResult = this.boundMethods.get(methodName);
    if (cachedResult) {
      return cachedResult;
    }
    const unboundMethod = this.instanceType.methods.get(methodName);
    if (!unboundMethod) {
      return null;
    }
    const boundMethodType =
      this.typeBinder.bind(unboundMethod.type);
    const boundMethodFunctionType = boundMethodType.asFunctionType();
    if (!boundMethodFunctionType) {
      // This is an assertion error
      warn(`bound method is no longer a function type ${boundMethodType}`);
      return null;
    }
    const boundMethod = new Variable(
      unboundMethod.final,
      unboundMethod.identifier,
      boundMethodFunctionType,
      unboundMethod.documentation);
    this.boundMethods.set(boundMethod.identifier.name, boundMethod);
    return boundMethod;
  }
  getMethodNames(): string[] {
    return this.instanceType.getMethodNames();
  }
}

export class InstanceType extends Type {
  private readonly _isTrait: boolean;
  readonly identifier: ast.Identifier;
  readonly typeParameters: Variable<TypeVariableTypeType>[];
  readonly bases: BaseInstanceType[];
  readonly asVariable: Variable<ClassType>;
  readonly classType: ClassType = new ClassType(this);
  readonly fields = new Map<string, Variable>();
  readonly staticMethods = new Map<string, Variable<FunctionType>>();
  readonly methods = new Map<string, Variable<FunctionType>>();
  readonly builtinType: PrimitiveType | null;
  constructor(
    _isTrait: boolean,
    identifier: ast.Identifier,
    typeParameters: Variable<TypeVariableTypeType>[],
    bases: BaseInstanceType[],
    documentation: string | null,
    builtinType: PrimitiveType | null) {
    super();
    this._isTrait = _isTrait;
    this.identifier = identifier;
    this.typeParameters = typeParameters;
    this.bases = bases;
    this.asVariable = new Variable(true, identifier, this.classType, documentation);
    this.builtinType = builtinType;
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return this.identifier.name;
  }
  getField(fieldName: string): Variable | null {
    if (this.typeParameters.length > 0) {
      return null;
    }
    return this.fields.get(fieldName) || null;
  }
  getFieldNames(): string[] {
    return Array.from(this.fields.keys());
  }
  getMethod(methodName: string): Variable | null {
    if (this.typeParameters.length > 0) {
      return null;
    }
    return this.methods.get(methodName) || null;
  }
  getMethodNames(): string[] {
    return Array.from(this.methods.keys());
  }
  isTrait(): boolean {
    return this._isTrait;
  }
  isInstantiable() {
    return !this.isTrait() && !this.builtinType;
  }
}

export class ClassType extends Type {
  readonly instanceType: InstanceType;
  private _funcType: FunctionType | null = null;
  constructor(instanceType: InstanceType) {
    super();
    this.instanceType = instanceType;
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return `class ${this.instanceType.identifier.name}`;
  }
  getMethodNames(): string[] {
    return Array.from(this.instanceType.staticMethods.keys());
  }
  getMethod(methodName: string): Variable | null {
    return this.instanceType.staticMethods.get(methodName) || null;
  }
  asFunctionType(): FunctionType | null {
    if (!this.instanceType.isInstantiable()) {
      return null;
    }
    const cached = this._funcType;
    if (cached) {
      return cached;
    }
    const init = this.instanceType.getMethod('__init__')?.type.asFunctionType();
    if (init && init.typeParameters.length === 0) {
      const func = new FunctionType(
        [], init.parameters, this.instanceType, init.documentation);
      this._funcType = func;
      return func;
    }
    const func = new FunctionType([], [], this.instanceType, null);
    this._funcType = func;
    return func;
  }
}

class ClassTypeType extends Type {
  static readonly INSTANCE = new ClassTypeType();
  private constructor() {
    super();
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return 'Class';
  }
}

export class TypeVariableInstanceType extends Type {
  readonly identifier: ast.Identifier;
  readonly bound: Type | null;
  readonly typeType = new TypeVariableTypeType(this);
  readonly asVariable: Variable<TypeVariableTypeType>;
  constructor(identifier: ast.Identifier, bound: Type | null) {
    super();
    this.identifier = identifier;
    this.bound = bound;
    this.asVariable = new Variable(true, identifier, this.typeType, null);
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return this.identifier.name;
  }
  getField(fieldName: string): Variable | null {
    return this.bound ? this.bound.getField(fieldName) : null;
  }
  getFieldNames(): string[] {
    return this.bound ? this.bound.getFieldNames() : [];
  }
  getMethod(methodName: string): Variable | null {
    return this.bound ? this.bound.getMethod(methodName) : null;
  }
  getMethodNames(): string[] {
    return this.bound ? this.bound.getMethodNames() : [];
  }
}

export class TypeVariableTypeType extends Type {
  readonly instanceType: TypeVariableInstanceType;
  constructor(instanceType: TypeVariableInstanceType) {
    super();
    this.instanceType = instanceType;
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return `typevar ${this.instanceType.identifier.name}`;
  }
}

export class IterationType<T extends Type = Type> extends Type {
  readonly itemType: T;
  constructor(itemType: T) {
    super();
    this.itemType = itemType;
  }
  _equals(other: Type): boolean {
    return other instanceof ListType && this.itemType.equals(other.itemType);
  }
  toString(): string {
    return `Iteration[${this.itemType}]`;
  }
  assumeTrue(): Type {
    return this.itemType;
  }
}

export class IterableType<T extends Type = Type> extends Type {
  readonly itemType: T;
  readonly iterMethod: Variable<FunctionType>;
  constructor(itemType: T) {
    super();
    this.itemType = itemType;
    this.iterMethod = new Variable(
      true,
      new ast.Identifier(BUILTIN_LOCATION, '__iter__'),
      new FunctionType([], [], itemType.getIterationType(), null),
      null);
  }
  _equals(other: Type): boolean {
    return other instanceof IterableType && this.itemType.equals(other.itemType);
  }
  toString(): string {
    return `Iterable[${this.itemType}]`;
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    if (methodName === '__iter__') {
      return this.iterMethod;
    }
    return null;
  }
}

export class ListType<T extends Type = Type> extends Type {
  readonly itemType: T;
  readonly methods: Map<string, Variable<FunctionType>>;
  constructor(itemType: T) {
    super();
    this.itemType = itemType;
    this.methods = mkmap([
      mkmethod('__bmon__', [], ANY_TYPE),
      mkmethod('__eq__', [['other', this]], BOOL_TYPE),
      mkmethod('__lt__', [['other', this]], BOOL_TYPE),
      mkmethod('__len__', [], NUMBER_TYPE),
      mkmethod('__mul__', [['n', NUMBER_TYPE]], this),
      mkmethod('__add__', [['other', this]], this),
      mkmethod('__getitem__', [['index', NUMBER_TYPE]], itemType),
      mkmethod('__setitem__',
        [['index', NUMBER_TYPE], ['value', itemType]], NIL_TYPE),
      mkmethod('__slice__',
        [
          ['start', NUMBER_TYPE.getOptionalType(), 0],
          ['end', NUMBER_TYPE.getOptionalType(), 0],
        ],
        this),
      mkmethod('__contains__', [['item', itemType]], BOOL_TYPE),
      mkmethod('clear', [], NIL_TYPE),
      mkmethod('append', [['item', itemType]], NIL_TYPE),
      mkmethod('extend', [['items', itemType.getIterableType()]], NIL_TYPE),
      mkmethod('pop', [], itemType),
      mkmethod('insert',
        [['index', NUMBER_TYPE.getOptionalType()]], itemType),
      mkmethod('reverse', [], NIL_TYPE),
      mkmethod('__iter__', [], new FunctionType([], [], itemType.getIterationType(), null)),
      ...(itemType instanceof ListType ? [
        mkmethod('flatten', [], itemType),
      ] : []),
    ]);
  }
  _equals(other: Type): boolean {
    return other instanceof ListType && this.itemType.equals(other.itemType);
  }
  toString(): string {
    return this === UNTYPED_LIST ? 'List' : `List[${this.itemType}]`;
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    return this.methods.get(methodName) || null;
  }
  getMethodNames(): string[] {
    return Array.from(this.methods.keys());
  }
}

export class FrozenListType<T extends Type = Type> extends Type {
  readonly itemType: T;
  readonly methods: Map<string, Variable<FunctionType>>;
  constructor(itemType: T) {
    super();
    this.itemType = itemType;
    this.methods = mkmap([
      mkmethod('__eq__', [['other', this]], BOOL_TYPE),
      mkmethod('__lt__', [['other', this]], BOOL_TYPE),
      mkmethod('__len__', [], NUMBER_TYPE),
      mkmethod('__mul__', [['n', NUMBER_TYPE]], this),
      mkmethod('__add__', [['other', this]], this),
      mkmethod('__getitem__', [['index', NUMBER_TYPE]], itemType),
      mkmethod('__slice__',
        [
          ['start', NUMBER_TYPE.getOptionalType(), 0],
          ['end', NUMBER_TYPE.getOptionalType(), 0],
        ],
        this),
      mkmethod('__contains__', [['item', itemType]], BOOL_TYPE),
      mkmethod('__iter__', [], new FunctionType([], [], itemType.getIterationType(), null)),
      mkmethod('get0', [], itemType),
      mkmethod('get1', [], itemType),
      mkmethod('get2', [], itemType),
      mkmethod('get3', [], itemType),
    ]);
  }
  _equals(other: Type): boolean {
    return other instanceof FrozenListType && this.itemType.equals(other.itemType);
  }
  toString(): string {
    return `FrozenList[${this.itemType}]`;
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    return this.methods.get(methodName) || null;
  }
}

export class TupleType extends Type {
  readonly itemTypes: Type[];
  readonly methods = new Map<string, Variable<FunctionType>>();
  constructor(itemTypes: Type[]) {
    super();
    this.itemTypes = itemTypes;
    const methods = [
      mkmethod('__eq__', [['other', this]], BOOL_TYPE),
      mkmethod('__lt__', [['other', this]], BOOL_TYPE),
      mkmethod('__len__', [], NUMBER_TYPE),
      mkmethod('__mul__', [['n', NUMBER_TYPE]], this),
      mkmethod('__contains__', [['item', ANY_TYPE]], BOOL_TYPE),
      mkmethod('__getitem__', [['index', NUMBER_TYPE]], ANY_TYPE),
      mkmethod('__iter__', [],
        new FunctionType([], [], ANY_TYPE.getIterationType(), null)),
    ];
    switch (itemTypes.length) {
      case 4:
        methods.push(mkmethod('get3', [], itemTypes[3]));
      case 3:
        methods.push(mkmethod('get2', [], itemTypes[2]));
      case 2:
        methods.push(mkmethod('get1', [], itemTypes[1]));
      case 1:
        methods.push(mkmethod('get0', [], itemTypes[0]));
    }
    for (const method of methods) {
      this.methods.set(method.identifier.name, method);
    }
  }
  _equals(other: Type): boolean {
    if (!(other instanceof TupleType)) {
      return false;
    }
    if (this.itemTypes.length !== other.itemTypes.length) {
      return false;
    }
    for (let i = 0; i < this.itemTypes.length; i++) {
      if (!this.itemTypes[i].equals(other.itemTypes[i])) {
        return false;
      }
    }
    return true;
  }
  toString(): string {
    return `Tuple[${this.itemTypes.join(', ')}]`;
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    return this.methods.get(methodName) || null;
  }
}

export class DictType<K extends Type = Type, V extends Type = Type> extends Type {
  static of<K extends Type, V extends Type>(k: K, v: V): DictType<K, V> {
    return new DictType(k, v);
  }
  readonly keyType: K;
  readonly valueType: V;
  readonly methods: Map<string, Variable<FunctionType>>;
  private constructor(keyType: K, valueType: V) {
    super();
    this.keyType = keyType;
    this.valueType = valueType;
    this.methods = mkmap([
      mkmethod('__len__', [], NUMBER_TYPE),
      mkmethod('__eq__', [['other', this]], BOOL_TYPE),
      mkmethod('getOrNil', [['key', keyType]], valueType.getOptionalType()),
      mkmethod('get', [['key', keyType], ['default', valueType]], valueType),
      mkmethod('__getitem__', [['key', keyType]], valueType),
      mkmethod('__contains__', [['key', keyType]], BOOL_TYPE),
      mkmethod('__iter__', [],
        new FunctionType([], [], keyType.getIterationType(), null)),
      mkmethod('rget',
        [['rkey', valueType], ['default', keyType, null]], keyType),

      mkmethod('__bmon__', [], ANY_TYPE),
      mkmethod('__setitem__', [['key', keyType]], valueType),
      mkmethod('freeze', [], FrozenDictType.of(keyType, valueType)),
    ]);
  }
  _equals(other: Type): boolean {
    return other instanceof DictType &&
      this.keyType.equals(other.keyType) &&
      this.valueType.equals(other.valueType);
  }
  toString(): string {
    return `Dict[${this.keyType}, ${this.valueType}]`;
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    return this.methods.get(methodName) || null;
  }
}

export class FrozenDictType<K extends Type = Type, V extends Type = Type> extends Type {
  static of<K extends Type, V extends Type>(k: K, v: V): FrozenDictType<K, V> {
    return new FrozenDictType(k, v);
  }
  readonly keyType: K;
  readonly valueType: V;
  readonly methods: Map<string, Variable<FunctionType>>;
  private constructor(keyType: K, valueType: V) {
    super();
    this.keyType = keyType;
    this.valueType = valueType;
    this.methods = mkmap([
      mkmethod('__len__', [], NUMBER_TYPE),
      mkmethod('__eq__', [['other', this]], BOOL_TYPE),
      mkmethod('getOrNil', [['key', keyType]], valueType.getOptionalType()),
      mkmethod('get', [['key', keyType], ['default', valueType]], valueType),
      mkmethod('__getitem__', [['key', keyType]], valueType),
      mkmethod('__contains__', [['key', keyType]], BOOL_TYPE),
      mkmethod('__iter__', [],
        new FunctionType([], [], keyType.getIterationType(), null)),
      mkmethod('rget',
        [['rkey', valueType], ['default', keyType, null]], keyType),
    ]);
  }
  _equals(other: Type): boolean {
    return other instanceof FrozenDictType &&
      this.keyType.equals(other.keyType) &&
      this.valueType.equals(other.valueType);
  }
  toString(): string {
    return `FrozenDict[${this.keyType}, ${this.valueType}]`;
  }
  getMethod(methodName: string): Variable<FunctionType> | null {
    return this.methods.get(methodName) || null;
  }
}

export class FrozenDictLiteralType<K extends Type = Type, V extends Type = Type> extends Type {
  readonly type: FrozenDictType<K, V>;
  readonly map = new Map<string, Variable<V>>();
  constructor(type: FrozenDictType<K, V>, variables: Variable<V>[]) {
    super();
    this.type = type;
    for (const v of variables) {
      this.map.set(v.identifier.name, v);
    }
  }
  _equals(other: Type): boolean {
    return this === other;
  }
  toString(): string {
    return this.type.toString();
  }
  getFieldNames(): string[] {
    return Array.from(this.map.keys()).concat(this.type.getFieldNames());
  }
  getField(fieldName: string): Variable | null {
    return this.map.get(fieldName) || this.type.getField(fieldName);
  }
  getMethod(methodName: string): Variable<Type> | null {
    return this.type.getMethod(methodName);
  }
  toNonLiteralType(): Type {
    return this.type;
  }
}

function stringifyConstValue(value: ConstValue): string {
  if (value === null) {
    return "nil";
  }
  switch (typeof value) {
    case "string": return JSON.stringify(value);
    case "boolean": return value ? "true" : "false";
    case "number": return '' + value;
  }
  return "*";
}

export class Parameter {
  readonly identifier: ast.Identifier;
  readonly type: Type;
  readonly defaultValue: ConstValue | undefined;
  constructor(
    identifier: ast.Identifier,
    type: Type,
    defaultValue: ConstValue | undefined) {
    this.identifier = identifier;
    this.type = type;
    this.defaultValue = defaultValue;
  }
  toString() {
    return `${this.identifier.name} ${this.type}` + (
      this.defaultValue === undefined ?
        '' :
        ('=' + stringifyConstValue(this.defaultValue)));
  }
}

export class FunctionType extends Type {
  readonly typeParameters: Variable<TypeVariableTypeType>[];
  readonly parameters: Parameter[];
  readonly returnType: Type;
  readonly documentation: string | null;
  constructor(
    typeParameters: Variable<TypeVariableTypeType>[],
    parameters: Parameter[],
    returnType: Type,
    documentation: string | null) {
    super();
    this.typeParameters = typeParameters;
    this.parameters = parameters;
    this.returnType = returnType;
    this.documentation = documentation;
  }
  _equals(other: Type): boolean {
    if (!(other instanceof FunctionType)) {
      return false;
    }
    if (this.typeParameters.length !== other.typeParameters.length ||
      this.parameters.length !== other.parameters.length) {
      return false;
    }
    for (let i = 0; i < this.typeParameters.length; i++) {
      if (this.typeParameters[i] !== other.typeParameters[i]) {
        return false;
      }
    }
    for (let i = 0; i < this.parameters.length; i++) {
      if (!this.parameters[i].type.equals(other.parameters[i].type)) {
        return false;
      }
    }
    if (!this.returnType.equals(other.returnType)) {
      return false;
    }
    return true;
  }
  toString(): string {
    const tparams = this.typeParameters.map(tp => tp.identifier.name).join(',');
    const namedParams = this.parameters.map(p => `${p.identifier.name} ${p.type}`).join(',');
    const typeOnlyParams = this.parameters.map(p => p.type).join(',');
    return this.typeParameters.length ?
      `def[${tparams}](${namedParams})${this.returnType}` :
      this.parameters.length ?
        `Function[${typeOnlyParams},${this.returnType}]` :
        `Function[${this.returnType}]`;
  }
  asFunctionType(): FunctionType | null {
    return this;
  }
}

export class UnionType extends Type {
  static of(types: Type[]): Type {
    const pairs: [Type, string][] = [];
    const seen = new Set<string>();
    let isOptional = false;

    types = [...types]; // make a copy so that we avoid modifying the original
    for (let entryType = types.pop(); entryType; entryType = types.pop()) {
      const entry = entryType;
      if (entry instanceof AnyType) {
        // If there is an Any type, none of the other types matter
        return ANY_TYPE;
      }
      if (entry instanceof NeverType) {
        // Never types don't matter
        continue;
      }
      if (entry instanceof UnionType) {
        // Unpack the union type
        types.push(...entry.types);
        continue;
      }
      if (pairs.some(pair => entry.isAssignableTo(pair[0]))) {
        // if any of the previous types is broader than the
        // current type, we can ignore the current type
        continue;
      }

      for (let i = 0; i < pairs.length; i++) {
        // If any of the already added types are a subtype of entry,
        // we should remove them from the array.
        if (pairs[i][0].isAssignableTo(entry)) {
          pairs[i] = [NEVER_TYPE, '' + NEVER_TYPE];
        }
      }

      // In all other cases, add the type if it's new
      const typeName = entry.toString();
      if (!seen.has(typeName)) {
        seen.add(typeName);
        pairs.push([entry, typeName]);
      }
    }
    const processedTypes =
      pairs
        .filter(pair => pair[0] !== NEVER_TYPE)
        .sort((a, b) => a[1] < b[1] ? -1 : a[1] > b[1] ? 1 : 0)
        .map(pair => pair[0]);
    if (processedTypes.length === 0) {
      return NEVER_TYPE;
    }
    if (processedTypes.length === 1) {
      return isOptional ? processedTypes[0].getOptionalType() : processedTypes[0];
    }
    const unionType = new UnionType(processedTypes);
    return isOptional ? unionType.getOptionalType() : unionType;
  }
  readonly types: Type[]
  private constructor(types: Type[]) {
    super();
    this.types = types;
  }
  _equals(other: Type): boolean {
    return other instanceof UnionType &&
      this.types.every((entry, i) => entry.equals(other.types[i]));
  }
  toString(): string {
    const noNilTypes = this.types.filter(t => t !== NIL_TYPE);
    let ret = noNilTypes.join('|');
    if (noNilTypes.length < this.types.length) {
      ret += '?';
    }
    return ret;
  }
}

export interface TypeBinder {
  bind(type: Type): Type;
};

export function newTypeBinder(
  typeMap: Map<TypeVariableInstanceType, Type>,
  bindSet: Set<TypeVariableInstanceType>) {
  function bind(type: Type): Type {
    if (type instanceof ListType) {
      return bind(type.itemType).getListType();
    } else if (type instanceof FrozenListType) {
      return bind(type.itemType).getFrozenListType();
    } else if (type instanceof UnionType) {
      return UnionType.of(type.types.map(bind));
    } else if (type instanceof DictType) {
      return DictType.of(bind(type.keyType), bind(type.valueType));
    } else if (type instanceof FrozenDictType) {
      return FrozenDictType.of(bind(type.keyType), bind(type.valueType));
    } else if (type instanceof IterableType) {
      return new IterableType(bind(type.itemType));
    } else if (type instanceof IterationType) {
      return new IterationType(bind(type.itemType));
    } else if (type instanceof FunctionType) {
      return new FunctionType(
        type.typeParameters,
        type.parameters.map(p =>
          new Parameter(p.identifier, bind(p.type), p.defaultValue)),
        bind(type.returnType),
        null);
    } else if (type instanceof TypeVariableInstanceType) {
      return bindSet.has(type) ? typeMap.get(type) || NEVER_TYPE : type;
    }
    return type;
  }
  return { bind };
}

export const CLASS_TYPE = ClassTypeType.INSTANCE;
export const ANY_TYPE = AnyType.INSTANCE;
export const NEVER_TYPE = NeverType.INSTANCE;
export const STOP_ITERATION_TYPE = new PrimitiveType('StopIteration');
export const NIL_TYPE = new PrimitiveType('nil');
export const BOOL_TYPE = new PrimitiveType('Bool');
export const NUMBER_TYPE = new PrimitiveType('Number');
export const STRING_TYPE = new PrimitiveType('String');

// TODO: Come up with a better more general 'unknown' type strategy
// rather than adhoc hacks with things like UNTYPED_LIST.

/**
 * UNTYPED_LIST is like ANY_TYPE.getListType(), except, sometimes
 * the solver will treat UNTYPED_LIST a bit differently
 */
export const UNTYPED_LIST = new ListType(ANY_TYPE);

const ANY_PROPERTY = new Variable(
  false, new ast.Identifier(BUILTIN_LOCATION, '(any)'), ANY_TYPE, null);

export function formatVariable(variable: Variable): string {
  const name = variable.identifier.name;
  const type = variable.type;
  const keyword = variable.final ? 'final' : 'var';
  if (type instanceof ClassType) {
    return `class ${type.instanceType.identifier.name}`;
  }
  if (type instanceof ModuleType) {
    return `import ${type.module.name}`;
  }
  if (type instanceof FunctionType) {
    const tparams = type.typeParameters.length === 0 ? '' :
      `[${type.typeParameters.map(tp => tp.identifier.name).join(',')}]`
    const params = type.parameters.join(', ');
    return `def ${name}${tparams}(${params}) ${type.returnType}`;
  }
  if (type instanceof LiteralType) {
    return `${keyword} ${name} ${type} = ${JSON.stringify(type.value)}`;
  }
  return `${keyword} ${name} ${type}`;
}

type ParamSpec = [string, Type] | [string, Type, ConstValue]

function mkmap(methods: Variable<FunctionType>[]):
  Map<string, Variable<FunctionType>> {
  const map = new Map<string, Variable<FunctionType>>();
  for (const method of methods) {
    map.set(method.identifier.name, method);
  }
  return map;
}

function mkmethod(
  name: string,
  paramSpecs: ParamSpec[],
  returnType: Type,
  documentation: string | null = null): Variable<FunctionType> {
  const identifier = new ast.Identifier(BUILTIN_LOCATION, name);
  const functype = new FunctionType(
    [],
    paramSpecs.map(spec =>
      new Parameter(new ast.Identifier(BUILTIN_LOCATION, spec[0]), spec[1], spec[2])),
    returnType,
    documentation);
  return new Variable(true, identifier, functype, documentation);
}

function getMinArgc(parameters: Parameter[]): number {
  let argc = 0;
  for (const param of parameters) {
    if (param.defaultValue !== undefined) {
      argc++;
    }
  }
  return argc;
}

function getMaxArgc(parameters: Parameter[]): number {
  return parameters.length;
}
