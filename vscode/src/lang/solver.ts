import { debug, warn, swrap } from "../debug";
import * as ast from "./ast";
import { MError } from "./error";
import * as ir from "./ir";
import { MemberCompletionPoint, ScopeCompletionPoint } from "./ir/cpoint";
import { Scope, Variable } from "./ir/scope";
import { SigHelp } from "./ir/sighelp";
import { Usage } from "./ir/usage";
import { MLocation } from "./location";


export function solve(
    moduleName: string,
    file: ast.File,
    moduleMap: Map<string, ir.Module>): ir.Module {
  return swrap('solve', moduleName, () => _solve(moduleName, file, moduleMap));
}

function _solve(
    moduleName: string,
    file: ast.File,
    moduleMap: Map<string, ir.Module>): ir.Module {
  const artificialBuiltinsScope = new Scope(null);
  const moduleScope = new Scope(artificialBuiltinsScope);
  const module = new ir.Module(moduleName, file, moduleScope);
  mergeBuiltinScope(moduleName, artificialBuiltinsScope, moduleMap);

  const usageMap = new Map<ast.Identifier, number>();
  const errors = module.errors;
  let scope = moduleScope;

  function err(location: MLocation, message: string) {
    errors.push(new MError(location, message));
  }
  function declareUsage(identifier: ast.Identifier, variable: Variable) {
    if (!usageMap.has(identifier)) {
      usageMap.set(identifier, module.usages.length);
      module.usages.push(new Usage(identifier, variable));
    }
  }
  function declareVariable(
      variable: Variable,
      explicitRefName: string | null = null) {
    const refName = explicitRefName || variable.identifier.name;
    const prev = scope.map.get(refName);
    if (prev) {
      errors.push(new MError(
        variable.identifier.location,
        `Variable with name "${refName}" ` +
        `already declared in this scope`));
    }
    scope.map.set(refName, variable);
    declareUsage(variable.identifier, variable);
  }

  function withScope<R>(f: () => R, newScope: boolean = true): R {
    const outerScope = scope;
    scope = newScope ? new Scope(scope) : outerScope;
    try {
      return f();
    } finally {
      scope = outerScope;
    }
  }
  function solveTypeParameters(asts: ast.TypeParameter[]): Variable<ir.TypeVariableTypeType>[] {
    return asts.map(tp => {
      const bound = tp.bound ? solveType(tp.bound) : null;
      return new ir.TypeVariableInstanceType(tp.identifier, bound).asVariable;
    });
  }
  function functionASTToType(func: ast.Function): ir.FunctionType {
    const typeParameters = solveTypeParameters(func.typeParameters);
    return withScope(() => {
      for (const tp of typeParameters) {
        declareVariable(tp);
      }
      const parameters = func.parameters.map(p => new ir.Parameter(
          p.identifier,
          solveType(p.typeExpression),
          p.defaultValue ? requireConstExpr(p.defaultValue) : undefined));
      const returnType = solveType(func.returnType);
      return new ir.FunctionType(
        typeParameters, parameters, returnType, func.documentation?.value || null);
    });
  }
  function functionASTToVar(func: ast.Function): Variable<ir.FunctionType> {
    const type = functionASTToType(func);
    const variable = new Variable(
      true, func.identifier, type, func.documentation?.value || null);
    declareUsage(func.identifier, variable);
    return variable;
  }
  function solveBlockBody(stmts: ast.Statement[], newScope: boolean) {
    withScope(() => {
      const instanceTypeMap = new Map<ast.Class | ast.Trait, ir.InstanceType>();
      const functionTypeMap = new Map<ast.Function, ir.FunctionType>();
      function solveFunctionBody(funcAST: ast.Function) {
        const funcType = functionTypeMap.get(funcAST);
        if (!funcType) {
          throw new Error('Assertion Error');
        }
        withScope(() => {
          for (const typeParameter of funcType.typeParameters) {
            declareVariable(typeParameter);
          }
          for (const parameter of funcType.parameters) {
            declareVariable(new Variable(
              false, parameter.identifier, parameter.type, null));
          }
          solveBlock(funcAST.body);
        });
      }
      for (const stmt of stmts) {
        if (stmt instanceof ast.Class || stmt instanceof ast.Trait) {
          const typeParameters = solveTypeParameters(stmt.typeParameters);
          const bases = withScope(() => {
            const bb: ir.BaseInstanceType[] = [];
            for (const bexpr of stmt.bases) {
              const baseType = solveType(bexpr);
              if (baseType === ir.ANY_TYPE) {
                // everything already inherits from ANY... sort of
              } else if (baseType instanceof ir.InstanceType ||
                  baseType instanceof ir.BoundInstanceType) {
                bb.push(baseType);
              } else {
                err(bexpr.location, `${baseType} is not inheritable`);
              }
            }
            return bb;
          });
          const builtinType: ir.PrimitiveType | null = (() => {
            if (moduleName === '__builtin__') {
              switch (stmt.identifier.name) {
                case 'Nil':
                  return ir.NIL_TYPE;
                case 'Bool':
                  return ir.BOOL_TYPE;
                case 'Number':
                  return ir.NUMBER_TYPE;
                case 'String':
                  return ir.STRING_TYPE;
              }
            }
            return null;
          })();
          const instanceType = new ir.InstanceType(
            stmt instanceof ast.Trait,
            stmt.identifier,
            typeParameters,
            bases,
            stmt.documentation?.value || null,
            builtinType);
          instanceTypeMap.set(stmt, instanceType);
          declareVariable(instanceType.asVariable);
        }
      }
      for (const stmt of stmts) {
        if (stmt instanceof ast.Class || stmt instanceof ast.Trait) {
          const instanceType = instanceTypeMap.get(stmt);
          if (!instanceType) {
            throw new Error('Assertion Error');
          }
          const builtinType = instanceType.builtinType;
          if (stmt instanceof ast.Class) {
            for (const staticMethodAST of stmt.staticMethods) {
              // TODO: Check for duplicates
              const variable = functionASTToVar(staticMethodAST);
              instanceType.staticMethods.set(variable.identifier.name, variable);
              functionTypeMap.set(staticMethodAST, variable.type);
              if (builtinType) {
                builtinType.staticMethods.set(variable.identifier.name, variable);
              }
            }
          }
          withScope(() => {
            for (const tp of instanceType.typeParameters) {
              declareVariable(tp);
            }
            for (const base of instanceType.bases) {
              if (base instanceof ir.InstanceType) {
                for (const [k, v] of base.fields) {
                  instanceType.fields.set(k, v);
                }
                for (const [k, v] of base.methods) {
                  instanceType.methods.set(k, v);
                }
              } else if (base instanceof ir.BoundInstanceType) {
                for (const key of base.instanceType.fields.keys()) {
                  const field = base.getField(key);
                  if (field) {
                    instanceType.fields.set(key, field);
                  }
                }
                for (const key of base.instanceType.methods.keys()) {
                  const method = base.getMethod(key);
                  if (method) {
                    instanceType.methods.set(key, method);
                  }
                }
              }
            }
            for (const field of stmt.fields) {
              // TODO: Check for duplicates
              const type = solveType(field.typeExpression);
              const variable = new Variable(
                field.final,
                field.identifier,
                type,
                field.documentation?.value || null);
              instanceType.fields.set(variable.identifier.name, variable);
              declareUsage(variable.identifier, variable);
            }
            for (const methodAST of stmt.methods) {
              // TODO: Check for duplicates
              const variable = functionASTToVar(methodAST);
              instanceType.methods.set(variable.identifier.name, variable);
              functionTypeMap.set(methodAST, variable.type);
              if (builtinType) {
                builtinType.methods.set(variable.identifier.name, variable);
              }
            }
          });
        } else if (stmt instanceof ast.Function) {
          const funcVariable = functionASTToVar(stmt);
          declareVariable(funcVariable);
          functionTypeMap.set(stmt, funcVariable.type);
        }
      }
      for (const stmt of stmts) {
        if (stmt instanceof ast.Class) {
          const instanceType = instanceTypeMap.get(stmt);
          if (!instanceType) {
            throw new Error('Assertion Error');
          }
          stmt.staticMethods.forEach(m => solveFunctionBody(m));
          withScope(() => {
            for (const tp of instanceType.typeParameters) {
              declareVariable(tp);
            }
            const thisType = instanceType.typeParameters.length === 0 ?
              instanceType :
              new ir.BoundInstanceType(
                instanceType,
                instanceType.typeParameters.map(tp => tp.type.instanceType));
            const thisVariable =
              new Variable(true, stmt.identifier, thisType, null);
            declareVariable(thisVariable, 'this');
            if (instanceType.bases.length > 0) {
              const superType = instanceType.bases[0];
              const superVariable =
                new Variable(true, stmt.identifier, superType, null);
              declareVariable(superVariable, 'super');
            }
            stmt.methods.forEach(m => solveFunctionBody(m));
          });
        } else if (stmt instanceof ast.Function) {
          solveFunctionBody(stmt);
        } else {
          solveStmt(stmt);
        }
      }
    }, newScope);
  }
  function solveBlock(block: ast.Block) {
    solveBlockBody(block.statements, true);
  }

  function solveType(te: ast.TypeExpression | null): ir.Type {
    if (te === null) {
      return ir.ANY_TYPE;
    }
    function resolveValueType(
        identifier: ast.Identifier,
        valueType: ir.Type,
        args: ir.Type[]): ir.Type {
      if (valueType instanceof ir.ClassType) {
        declareUsage(identifier, valueType.instanceType.asVariable);
        if (args.length > 0) {
          return new ir.BoundInstanceType(valueType.instanceType, typeArgs);
        }
        return valueType.instanceType;
      }
      if (valueType instanceof ir.TypeVariableTypeType) {
        declareUsage(identifier, valueType.instanceType.asVariable);
        return valueType.instanceType;
      }
      // TODO: Error - not a valid type
      return ir.ANY_TYPE;
    }

    const typeArgs = te.args.map(arg => solveType(arg));

    if (!te.parentIdentifier) {
      module.cpoints.push(new ScopeCompletionPoint(
        te.baseIdentifier.location, scope, 'typesAndModulesOnly'));
      const name = te.baseIdentifier.name;
      switch (name) {
        case 'nil':
          return ir.NIL_TYPE;
        case 'Bool':
          return ir.BOOL_TYPE;
        case 'Int':
        case 'Float':
        case 'Number':
          return ir.NUMBER_TYPE;
        case 'String':
          return ir.STRING_TYPE;
        case 'Any':
          return ir.ANY_TYPE;
        case 'Never':
          return ir.NEVER_TYPE;
        case 'Class':
          return ir.CLASS_TYPE;
        case 'Tuple':
          return new ir.TupleType(typeArgs);
        case 'List':
          switch (typeArgs.length) {
            case 0:
              return ir.UNTYPED_LIST;
            case 1:
              return typeArgs[0].getListType();
            default:
              err(te.location, `List expects 1 arg but got ${typeArgs.length}`);
          }
          return ir.ANY_TYPE;
        case 'FrozenList':
          switch (typeArgs.length) {
            case 0:
              return ir.ANY_TYPE.getFrozenListType();
            case 1:
              return typeArgs[0].getFrozenListType();
            default:
              err(te.location, `FrozenList expects 1 arg but got ${typeArgs.length}`);
          }
          return ir.ANY_TYPE;
        case 'Optional':
          switch (typeArgs.length) {
            case 0:
              return ir.ANY_TYPE.getOptionalType();
            case 1:
              return typeArgs[0].getOptionalType();
            default:
              err(te.location, `Optional expects 1 arg but got ${typeArgs.length}`);
          }
          return ir.ANY_TYPE;
        case 'Iteration':
          switch (typeArgs.length) {
            case 0:
              return ir.ANY_TYPE.getIterationType();
            case 1:
              return typeArgs[0].getIterationType();
            default:
              err(te.location, `Iteration expects 1 arg but got ${typeArgs.length}`);
          }
          return ir.ANY_TYPE;
        case 'Iterable':
          switch (typeArgs.length) {
            case 0:
              return ir.ANY_TYPE.getIterableType();
            case 1:
              return typeArgs[0].getIterableType();
            default:
              err(te.location, `Iterable expects 1 arg but got ${typeArgs.length}`);
          }
          return ir.ANY_TYPE;
        case 'Dict':
          switch (typeArgs.length) {
            case 0:
              return ir.DictType.of(ir.ANY_TYPE, ir.ANY_TYPE);
            case 2:
              return ir.DictType.of(typeArgs[0], typeArgs[1]);
            default:
              err(te.location, `Dict expects 2 args but got ${typeArgs.length}`);
          }
          return ir.ANY_TYPE;
        case 'FrozenDict':
          switch (typeArgs.length) {
            case 0:
              return ir.FrozenDictType.of(ir.ANY_TYPE, ir.ANY_TYPE);
            case 2:
              return ir.FrozenDictType.of(typeArgs[0], typeArgs[1]);
            default:
              err(te.location, `FrozenDict expects 2 args but got ${typeArgs.length}`);
          }
          return ir.ANY_TYPE;
        case 'Function': {
          if (typeArgs.length === 0) {
            err(te.location,
              `Function requires at least 1 arg for the return type ` +
              `but got ${typeArgs.length}`);
            return ir.ANY_TYPE;
          }
          return new ir.FunctionType(
            [],
            typeArgs.slice(0, typeArgs.length - 1).map((t, i) => {
              return new ir.Parameter(
                new ast.Identifier(te.args[i].location, `a${i}`), t, undefined);
            }),
            typeArgs[typeArgs.length - 1],
            null);
        }
      }
      const valueType = scope.get(name)?.type;
      if (valueType) {
        return resolveValueType(te.baseIdentifier, valueType, typeArgs);
      }
      err(te.location, `Type ${name} not found`);
      return ir.ANY_TYPE;
    }

    const parentName = te.parentIdentifier.name;
    const parentVariable = scope.get(parentName);

    if (parentVariable) {
      module.cpoints.push(new MemberCompletionPoint(
        te.baseIdentifier.location, parentVariable.type, 'typesOnly'));
      declareUsage(te.parentIdentifier, parentVariable);
      const parentType = parentVariable.type;
      const baseName = te.baseIdentifier.name;
      if (parentType instanceof ir.ModuleType) {
        const valueType = parentType.module.scope.get(baseName)?.type;
        if (valueType) {
          return resolveValueType(te.baseIdentifier, valueType, typeArgs);
        } else {
          err(te.location, `${baseName} not found in ${parentType}`);
        }
      }
    } else {
      err(te.parentIdentifier.location,
        `${te.parentIdentifier.name} not found`);
    }

    return ir.ANY_TYPE;
  }

  function requireConstExpr(e: ast.Expression): ir.ConstValue {
    const value = solveConstExpr(e);
    if (value === undefined) {
      err(e.location, `Expected constexpr`);
      return 0;
    }
    return value;
  }

  function solveConstExpr(e: ast.Expression): ir.ConstValue | undefined {
    if (e instanceof ast.NilLiteral) {
      return null;
    } else if (e instanceof ast.BoolLiteral) {
      return e.value;
    } else if (e instanceof ast.NumberLiteral) {
      return e.value;
    } else if (e instanceof ast.StringLiteral) {
      return e.value;
    } else if (e instanceof ast.GetVariable) {
      const variable = scope.get(e.identifier.name);
      const type = variable?.type;
      if (type instanceof ir.LiteralType) {
        return type.value;
      }
    } else if (e instanceof ast.GetField) {
      const owner = e.owner;
      if (owner instanceof ast.GetVariable) {
        const ownerVariable = scope.get(owner.identifier.name);
        const ownerType = ownerVariable?.type;
        if (ownerType instanceof ir.ModuleType) {
          const ownerModule = ownerType.module;
          const variable = ownerModule.scope.get(e.identifier.name);
          const type = variable?.type;
          if (type instanceof ir.LiteralType) {
            return type.value;
          }
        }
      }
    } else if (e instanceof ast.MethodCall) {
      const owner = solveConstExpr(e.owner);
      if (owner === undefined) {
        return undefined;
      }
      const args: ir.ConstValue[] = [];
      for (const argExpr of e.args) {
        const arg = solveConstExpr(argExpr);
        if (arg === undefined) {
          return undefined;
        }
        args.push(arg);
      }
      switch (typeof owner) {
        case 'number':
          switch (args.length) {
            case 0:
              switch (e.identifier.name) {
                case '__neg__': return -owner;
              }
              break;
            case 1: {
              const arg = args[0];
              if (typeof arg === 'number') {
                switch (e.identifier.name) {
                  case '__add__': return owner + arg;
                  case '__sub__': return owner - arg;
                  case '__mul__': return owner * arg;
                  case '__mod__': return fmod(owner, arg);
                  case '__div__': return owner / arg;
                  case '__floordiv__': return Math.floor(owner / arg);
                }
              }
              break;
            }
          }
          break;
        case 'string':
          switch (args.length) {
            case 1: {
              const arg = args[0];
              if (typeof arg === 'string') {
                switch (e.identifier.name) {
                  case '__add__': return owner + arg;
                }
              }
              break;
            }
          }
          break;
      }
    }
    return undefined;
  }

  function applyFunction(
      location: MLocation,
      usage: Usage | null,
      funcType: ir.FunctionType,
      hintReturnType: ir.Type | null,
      argexprs: ast.Expression[],
      argLocations: MLocation[] | null): ir.Type {
    const tparams = funcType.typeParameters;
    const params = funcType.parameters;
    const rtype = funcType.returnType;

    // signature helpers
    if (argLocations) {
      for (let i = 0; i < argLocations.length; i++) {
        module.sighelps.push(new SigHelp(
          argLocations[i],
          null,
          null,
          funcType.parameters.map(p => p.type),
          funcType.parameters.map(p => p.identifier.name),
          i,
          funcType.returnType,
        ));
      }
    }

    // argc check
    const optargc = params.filter(p => p.defaultValue !== undefined).length;
    if (argexprs.length < params.length - optargc) {
      const minargc = params.length - optargc;
      argexprs.forEach(e => solveExpr(e));
      err(location,
        `At least ${minargc} args are required but got ${argexprs.length}`);
      return tparams.length === 0 ? rtype : ir.ANY_TYPE;
    } else if (argexprs.length > params.length) {
      argexprs.forEach(e => solveExpr(e));
      err(location,
        `Up to ${params.length} args are allowed but got ${argexprs.length}`);
      return tparams.length === 0 ? rtype : ir.ANY_TYPE;
    }

    if (tparams.length === 0) {
      // No type parameters
      for (let i = 0; i < argexprs.length; i++) {
        const argType = solveExpr(argexprs[i], params[i].type);
        if (!argType.isAssignableTo(params[i].type)) {
          err(argexprs[i].location,
            `Expected argument to be ${params[i].type} but got ${argType}`);
        }
      }
      return rtype;
    }

    // Infer type arguments
    const typeMap = new Map<ir.TypeVariableInstanceType, ir.Type>();
    const bindSet = new Set(funcType.typeParameters.map(tp => tp.type.instanceType));
    const binder = ir.newTypeBinder(typeMap, bindSet);
    function infer(param: ir.Type, type: ir.Type): void {
      if (param.equals(type)) {
        return;
      }
      if (param instanceof ir.ListType) {
        if (type instanceof ir.ListType) {
          infer(param.itemType, type.itemType);
        }
      } else if (param instanceof ir.FrozenListType) {
        if (type instanceof ir.FrozenListType) {
          infer(param.itemType, type.itemType);
        }
      } else if (param instanceof ir.OptionalType) {
        if (type instanceof ir.OptionalType) {
          infer(param.itemType, type.itemType);
        }
      } else if (param instanceof ir.DictType) {
        if (type instanceof ir.DictType) {
          infer(param.keyType, type.keyType);
          infer(param.valueType, type.valueType);
        }
      } else if (param instanceof ir.FrozenDictType) {
        if (type instanceof ir.FrozenDictType) {
          infer(param.keyType, type.keyType);
          infer(param.valueType, type.valueType);
        }
      } else if (param instanceof ir.IterableType) {
        if (type instanceof ir.IterableType) {
          infer(param.itemType, type.itemType);
        }
      } else if (param instanceof ir.IterationType) {
        if (type instanceof ir.IterationType) {
          infer(param.itemType, type.itemType);
        }
      } else if (param instanceof ir.FunctionType) {
        if (type instanceof ir.FunctionType) {
          if (type.typeParameters.length > 0) {
            // If there are type parameters on the argument function,
            // don't many any inferences.
            return;
          }
          const argc = Math.min(
            param.parameters.length, type.parameters.length);
          for (let i = 0; i < argc; i++) {
            infer(param.parameters[i].type, type.parameters[i].type);
          }
          infer(param.returnType, type.returnType);
        }
      } else if (param instanceof ir.TypeVariableInstanceType) {
        if (bindSet.has(param) && !typeMap.has(param)) {
          typeMap.set(param, type.toNonLiteralType());
        }
      }
    }
    if (hintReturnType) {
      infer(funcType.returnType, hintReturnType);
    }
    const argTypes: ir.Type[] = [];
    const paramTypes: ir.Type[] = [];
    for (let i = 0; i < argexprs.length; i++) {
      const bestGuessParamType = binder.bind(funcType.parameters[i].type);
      const argType = solveExpr(argexprs[i], bestGuessParamType);
      infer(funcType.parameters[i].type, argType);
      argTypes.push(argType);
    }
    for (let i = 0; i < argexprs.length; i++) {
      const paramType = binder.bind(funcType.parameters[i].type);
      paramTypes.push(paramType);
      if (!argTypes[i].isAssignableTo(paramType)) {
        err(argexprs[i].location,
          `Expected argument to be ${paramType} but got ${argTypes[i]}`);
      }
    }
    const returnType = binder.bind(funcType.returnType);
    if (usage) {
      usage.boundType = new ir.FunctionType(
        [],
        paramTypes.map((pt, i) => {
          const param = funcType.parameters[i];
          return new ir.Parameter(param.identifier, pt, undefined);
        }),
        returnType,
        null);
    }
    return returnType;
  }

  function solveHomogeneousSeqs(
      exprs: ast.Expression[], hint: ir.Type | null = null): [ir.Type, ir.Type[]] {
    let i = 0;
    const itemTypes: ir.Type[] = [];
    const bound = (() => {
      if (hint) {
        return hint;
      }
      if (exprs.length === 0) {
        return ir.NEVER_TYPE;
      }
      const firstItemType = solveExpr(exprs[i++]);
      itemTypes.push(firstItemType);
      return firstItemType.toNonLiteralType();
    })();
    let allInBound = true;
    for (; i < exprs.length; i++) {
      const itemType = solveExpr(exprs[i], bound);
      if (!itemType.isAssignableTo(bound)) {
        allInBound = false;
      }
      itemTypes.push(itemType);
    }
    if (allInBound) {
      return [bound, itemTypes];
    }
    for (let i = 1; i < itemTypes.length; i++) {
      if (!itemTypes[i].isAssignableTo(itemTypes[0])) {
        return [ir.ANY_TYPE, itemTypes];
      }
    }
    return [itemTypes[0], itemTypes];
  }

  function solveHomogeneousSeq(
      exprs: ast.Expression[], hint: ir.Type | null = null): ir.Type {
    return solveHomogeneousSeqs(exprs, hint)[0];
  }

  const expressionVisitor: ast.ExpressionVisitor<ir.Type | null, ir.Type> = {
    visitErrorExpression: function (e: ast.ErrorExpression, hint: ir.Type | null): ir.Type {
      return ir.ANY_TYPE;
    },
    visitGetVariable: function (e: ast.GetVariable, hint: ir.Type | null): ir.Type {
      module.cpoints.push(new ScopeCompletionPoint(
        e.identifier.location, scope));
      const variable = scope.get(e.identifier.name);
      if (variable) {
        declareUsage(e.identifier, variable);
        return variable.type;
      }
      err(e.location, `Variable "${e.identifier.name}" not found`);
      return ir.ANY_TYPE;
    },
    visitSetVariable: function (e: ast.SetVariable, hint: ir.Type | null): ir.Type {
      const variable = scope.get(e.identifier.name);
      if (variable) {
        solveExpr(e.value, variable.type);
        declareUsage(e.identifier, variable);
        if (variable.final) {
          err(
            e.location,
            `Assign to final variable "${e.identifier.name}"`);
        }
        return variable.type;
      }
      err(e.location, `Variable "${e.identifier.name}" not found`);
      solveExpr(e.value);
      return ir.ANY_TYPE;
    },
    visitNilLiteral: function (e: ast.NilLiteral, hint: ir.Type | null): ir.Type {
      return new ir.LiteralType(ir.NIL_TYPE, null);
    },
    visitBoolLiteral: function (e: ast.BoolLiteral, hint: ir.Type | null): ir.Type {
      return new ir.LiteralType(ir.BOOL_TYPE, e.value);
    },
    visitNumberLiteral: function (e: ast.NumberLiteral, hint: ir.Type | null): ir.Type {
      return new ir.LiteralType(ir.NUMBER_TYPE, e.value);
    },
    visitStringLiteral: function (e: ast.StringLiteral, hint: ir.Type | null): ir.Type {
      return new ir.LiteralType(ir.STRING_TYPE, e.value);
    },
    visitTypeAssertion: function (e: ast.TypeAssertion, hint: ir.Type | null): ir.Type {
      const type = solveType(e.typeExpression);
      solveExpr(e.expression, type);
      return type;
    },
    visitListDisplay: function (e: ast.ListDisplay, hint: ir.Type | null): ir.Type {
      if (hint === ir.UNTYPED_LIST) {
        for (const itemExpr of e.items) {
          solveExpr(itemExpr);
        }
        return ir.UNTYPED_LIST;
      }
      return solveHomogeneousSeq(
        e.items,
        hint instanceof ir.ListType ? hint.itemType : null).getListType();
    },
    visitFrozenListDisplay: function (e: ast.FrozenListDisplay, hint: ir.Type | null): ir.Type {
      return solveHomogeneousSeq(
        e.items,
        hint instanceof ir.ListType ? hint.itemType : null).getFrozenListType();
    },
    visitTupleDisplay: function (e: ast.TupleDisplay, hint: ir.Type | null): ir.Type {
      if (hint instanceof ir.TupleType && hint.itemTypes.length === e.items.length) {
        const items = e.items.map((item, i) => solveExpr(item, hint.itemTypes[i]));
        return new ir.TupleType(items);
      }
      const items = e.items.map(item => solveExpr(item));
      return new ir.TupleType(items);
    },
    visitDictDisplay: function (e: ast.DictDisplay, hint: ir.Type | null): ir.Type {
      const keyType = solveHomogeneousSeq(
        e.pairs.map(([key, _]) => key),
        hint instanceof ir.DictType ? hint.keyType : null);
      const valueType = solveHomogeneousSeq(
        e.pairs.map(([_, value]) => value),
        hint instanceof ir.DictType ? hint.valueType : null);
      return ir.DictType.of(keyType, valueType);
    },
    visitFrozenDictDisplay: function (e: ast.FrozenDictDisplay, hint: ir.Type | null): ir.Type {
      const keyType = solveHomogeneousSeq(
        e.pairs.map(([key, _]) => key),
        hint instanceof ir.FrozenDictType ? hint.keyType : null);
      const [valueType, valueTypes] = solveHomogeneousSeqs(
        e.pairs.map(([_, value]) => value),
        hint instanceof ir.FrozenDictType ? hint.valueType : null);
      const dictType = ir.FrozenDictType.of(keyType, valueType);
      const variables: Variable[] = [];
      for (let i = 0; i < e.pairs.length && i < valueTypes.length; i++) {
        const [keyExpr, valExpr] = e.pairs[i];
        const keyVal = solveConstExpr(keyExpr);
        let valType = valueTypes[i];
        if (typeof keyVal === 'string') {
          if (valType instanceof ir.PrimitiveType) {
            const value = solveConstExpr(valExpr);
            if (value !== undefined) {
              valType = new ir.LiteralType(valType, value);
            }
          }
          const variable = new Variable(
            true, new ast.Identifier(keyExpr.location, keyVal), valType, null);
          if (keyExpr instanceof ast.StringLiteral) {
            declareUsage(
              new ast.Identifier(keyExpr.location, keyExpr.value), variable);
          }
          variables.push(variable);
        }
      }
      const dictLiteralType = new ir.FrozenDictLiteralType(dictType, variables);
      return dictLiteralType;
      // return new ir.FrozenDictLiteralType(dictType, variables);
    },
    visitLambda: function (e: ast.Lambda, hint: ir.Type | null): ir.Type {
      const funcHint = (() => {
        const fh = hint?.asFunctionType();
        return fh &&
          fh.typeParameters.length === 0 &&
          fh.parameters.length === e.parameters.length ? fh : null;
      })();
      const parameters: ir.Parameter[] = [];
      for (let i = 0; i < e.parameters.length; i++) {
        const paramType =
          e.parameters[i].typeExpression ?
            solveType(e.parameters[i].typeExpression) :
            funcHint ?
              funcHint.parameters[i].type :
              ir.ANY_TYPE;
        const defexpr = e.parameters[i].defaultValue;
        const defaultValue = defexpr ? solveConstExpr(defexpr) : undefined;
        parameters.push(new ir.Parameter(
          e.parameters[i].identifier, paramType, defaultValue));
      }
      const priorReturnType =
        e.returnType ?
          solveType(e.returnType) :
          funcHint ? funcHint.returnType : null;
      const body = withScope(() => {
        for (const parameter of parameters) {
          declareVariable(new Variable(
            true, parameter.identifier, parameter.type, null));
        }
        return solveExpr(e.body, priorReturnType);
      });
      const returnType = priorReturnType || body;
      return new ir.FunctionType([], parameters, returnType, null);
    },
    visitFunctionCall: function (e: ast.FunctionCall, hint: ir.Type | null): ir.Type {
      const callable = solveExpr(e.func);
      const func = callable.asFunctionType();
      if (func) {
        let usage: Usage | null = null;
        if (e.func instanceof ast.GetVariable) {
          const usageID = usageMap.get(e.func.identifier);
          if (usageID !== undefined) {
            usage = module.usages[usageID];
          }
        }
        return applyFunction(e.location, usage, func, hint, e.args, e.argLocations);
      }
      if (callable !== ir.ANY_TYPE) {
        err(e.location, `${callable} is not callable`);
      }
      e.args.map(arg => solveExpr(arg));
      return ir.ANY_TYPE;
    },
    visitMethodCall: function (e: ast.MethodCall, hint: ir.Type | null): ir.Type {
      const owner = solveExpr(e.owner);
      const method = owner.getMethod(e.identifier.name);
      if (method) {
        declareUsage(e.identifier, method);
        const methodType = method.type.asFunctionType();
        if (methodType) {
          const usageID = usageMap.get(e.identifier);
          let usage: Usage | null = null;
          if (usageID !== undefined) {
            usage = module.usages[usageID];
          }
          return applyFunction(
            e.location, usage, methodType, hint, e.args, e.argLocations);
        }
        if (method.type !== ir.ANY_TYPE) {
          err(e.location, `${method.type} is not callable`);
        }
      } else {
        if (owner instanceof ir.ModuleType) {
          err(e.identifier.location,
            `Function ${e.identifier.name} not found on ` +
            `module ${owner.module.name}`);
        } else {
          err(e.identifier.location,
            `Method ${e.identifier.name} not found on ` +
            `${owner}`);
        }
      }
      e.args.map(arg => solveExpr(arg));
      return ir.ANY_TYPE;
    },
    visitGetField: function (e: ast.GetField, hint: ir.Type | null): ir.Type {
      const owner = solveExpr(e.owner);
      const field = owner.getField(e.identifier.name);
      if (field) {
        module.cpoints.push(new MemberCompletionPoint(
          e.identifier.location, owner));
        declareUsage(e.identifier, field);
        return field.type;
      } else {
        err(e.identifier.location,
          `${e.identifier.name} not found in ${owner}`);
      }
      return ir.ANY_TYPE;
    },
    visitSetField: function (e: ast.SetField, hint: ir.Type | null): ir.Type {
      const owner = solveExpr(e.owner);
      const field = owner.getField(e.identifier.name);
      if (field) {
        if (field.final) {
          err(e.identifier.location, `Cannot assign to final value`);
        }
        declareUsage(e.identifier, field);
        solveExpr(e.value, field.type);
        return field.type;
      } else {
        err(e.identifier.location,
          `${e.identifier.name} not found in ${owner}`);
      }
      solveExpr(e.value);
      return ir.ANY_TYPE;
    },
    visitDot: function (e: ast.Dot, hint: ir.Type | null): ir.Type {
      const owner = solveExpr(e.owner);
      module.cpoints.push(new MemberCompletionPoint(e.dotLocation, owner));
      return ir.ANY_TYPE;
    },
    visitOperation: function (e: ast.Operation, hint: ir.Type | null): ir.Type {
      switch (e.op) {
        case 'is':
          solveExpr(e.args[0]);
          solveExpr(e.args[1]);
          return ir.BOOL_TYPE;
        case 'and':
        case 'or': {
          const lhs = solveExpr(e.args[0], hint);
          const rhs = solveExpr(e.args[1], hint);
          return e.op == 'or' ?
            lhs.assumeTrue().merge(rhs) :
            lhs.assumeFalse().merge(rhs);
        }
        case 'if': {
          solveExpr(e.args[0]);
          return solveExpr(e.args[1], hint).merge(
            solveExpr(e.args[2], hint));
        }
        case 'not': {
          solveExpr(e.args[0]);
          return ir.BOOL_TYPE;
        }
      }
      return ir.ANY_TYPE;
    },
    visitRaise: function (e: ast.Raise, hint: ir.Type | null): ir.Type {
      solveExpr(e.exception);
      return ir.NEVER_TYPE;
    }
  };
  function solveExpr(expr: ast.Expression, hint: ir.Type | null = null): ir.Type {
    return expr.accept(expressionVisitor, hint);
  }

  const statementVisitor: ast.StatementVisitor = {
    visitNop: function (s: ast.Nop): void {
    },
    visitFunction: function (s: ast.Function): void {
      // See solveBlockBody
      throw new Error('AssertionError(visitFunction)');
    },
    visitClass: function (s: ast.Class): void {
      // See solveBlockBody
      throw new Error('AssertionError(visitClass)');
    },
    visitTrait: function (s: ast.Trait): void {
    },
    visitVariable: function (s: ast.Variable): void {
      const implicitType = solveExpr(s.valueExpression);
      let type = s.typeExpression ?
        solveType(s.typeExpression) : implicitType;
      if (s.final && type instanceof ir.PrimitiveType) {
        const value = solveConstExpr(s.valueExpression);
        if (value !== undefined) {
          type = new ir.LiteralType(type, value);
        }
      }
      if (!s.final) {
        type = type.toNonLiteralType();
      }
      const variable = new Variable(s.final, s.identifier, type, s.documentation);
      declareVariable(variable);
    },
    visitWhile: function (s: ast.While): void {
      solveExpr(s.condition);
      solveBlock(s.body);
    },
    visitFor: function (s: ast.For): void {
      withScope(() => {
        const containerType = solveExpr(s.container);
        const itemType = containerType.getIterType() || (() => {
          // TODO: Error - looping over non-iterable type.
          return ir.ANY_TYPE;
        })();
        const itemVariable = new Variable(false, s.variable, itemType, null);
        declareVariable(itemVariable);
        solveBlock(s.body);
      });
    },
    visitIf: function (s: ast.If): void {
      for (const [cond, body] of s.pairs) {
        solveExpr(cond);
        solveBlock(body);
      }
      if (s.fallback) {
        solveBlock(s.fallback);
      }
    },
    visitBlock: function (s: ast.Block): void {
      solveBlock(s);
    },
    visitReturn: function (s: ast.Return): void {
      solveExpr(s.expression);
    },
    visitExpressionStatement: function (s: ast.ExpressionStatement): void {
      solveExpr(s.expression);
    }
  };
  function solveStmt(stmt: ast.Statement): void {
    return stmt.accept(statementVisitor);
  }

  for (const imp of file.imports) {
    const importModule = moduleMap.get(imp.module.toString());
    if (importModule) {
      const moduleType = new ir.ModuleType(importModule);
      const moduleVariable = new Variable(
        true, imp.module.identifier, moduleType, importModule.file.documentation);
      declareUsage(imp.module.identifier, moduleVariable);
      if (imp.member) {
        const memberVariable = importModule.scope.get(imp.member.name);
        if (memberVariable) {
          declareVariable(memberVariable, imp.alias.name);
          declareUsage(imp.member, memberVariable);
          declareUsage(imp.alias, memberVariable);
        } else {
          err(imp.location, `${imp.member.name} not found in ${importModule.name}`);
        }
      } else {
        declareVariable(moduleVariable, imp.alias.name);
        declareUsage(imp.alias, moduleVariable);
      }
    } else {
      err(imp.location, `Module ${imp.module} not found`);
    }
  }
  solveBlockBody(file.statements, false);

  return module;
}

function mergeBuiltinScope(
    moduleName: string, moduleScope: Scope, moduleMap: Map<string, ir.Module>) {
  const builtinScope = moduleMap.get('__builtin__')?.scope;
  if (builtinScope) {
    for (const [k, v] of builtinScope.map) {
      moduleScope.map.set(k, v);
    }
  } else if (moduleName !== '__builtin__' && moduleName !== '__main__') {
    warn(`Builtin module not found (processing ${moduleName})`);
  }
}

function fmod(a: number, b: number): number {
  const f = a % b;
  if (f === 0) {
    return 0;
  }
  if ((a < 0) === (b < 0)) {
    return f;
  }
  return f + b;
}
