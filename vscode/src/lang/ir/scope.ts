import type * as ir from '../ir';
import * as ast from '../ast';


export class Variable<T extends ir.Type = ir.Type> {
  readonly __tagVariable = 0;
  readonly final: boolean;
  readonly identifier: ast.Identifier;
  readonly type: T;
  readonly documentation: string | null;
  constructor(
      final: boolean,
      identifier: ast.Identifier,
      type: T,
      documentation: string | null) {
    this.final = final;
    this.identifier = identifier;
    this.type = type;
    this.documentation = documentation;
  }
}

export class Scope {
  readonly __tagScope = 0;
  readonly parent: Scope | null;
  readonly map = new Map<string, Variable>();
  constructor(parent: Scope | null) {
    this.parent = parent;
  }
  get(name: string): Variable | null {
    for (let scope: Scope | null = this; scope; scope = scope.parent) {
      const variable = scope.map.get(name);
      if (variable) {
        return variable;
      }
    }
    return null;
  }
}
