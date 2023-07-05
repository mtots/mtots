import { warn } from "../../debug";
import type * as ir from "../ir";
import { MLocation } from "../location";
import { Scope } from "./scope";



/** Completion Point */
export abstract class CompletionPoint {
  readonly location: MLocation;
  constructor(location: MLocation) {
    this.location = location;
  }
  abstract getEntries(): [string, string | null][];
}

export type MemberCompletionPointType = 'default' | 'typesOnly';

export class MemberCompletionPoint extends CompletionPoint {
  readonly owner: ir.Type;
  readonly type: MemberCompletionPointType;
  constructor(
      location: MLocation,
      owner: ir.Type,
      type: MemberCompletionPointType = 'default') {
    super(location);
    this.owner = owner;
    this.type = type;
  }
  getEntries(): [string, string | null][] {
    const owner = this.owner;
    const entries: [string, string | null][] = [];
    const seen = new Set<string>();
    for (const list of [owner.getFieldNames(), owner.getMethodNames()]) {
      for (const name of list) {
        if (!seen.has(name)) {
          const variable = owner.getField(name) || owner.getMethod(name);
          if (this.type === 'typesOnly' && !variable?.type.isClassType()) {
            continue;
          }
          entries.push([
            name,
            variable?.type.toString() || null
          ]);
          seen.add(name);
        }
      }
    }
    return entries;
  }
}

export type ScopeCompletionPointType = 'default' | 'typesAndModulesOnly';

export class ScopeCompletionPoint extends CompletionPoint {
  readonly scope: Scope;
  readonly type: ScopeCompletionPointType;
  constructor(
      location: MLocation,
      scope: Scope,
      type: ScopeCompletionPointType = 'default') {
    super(location);
    this.scope = scope;
    this.type = type;
  }

  getEntries(): [string, string | null][] {
    const entries: [string, string | null][] = [['nil', null]];
    switch (this.type) {
      case 'default':
        entries.push(['true', null]);
        entries.push(['false', null]);
        break;
      case 'typesAndModulesOnly':
        entries.push(['nil', null]);
        entries.push(['Bool', null]);
        entries.push(['Int', null]);
        entries.push(['Float', null]);
        entries.push(['Number', null]);
        entries.push(['String', null]);
        entries.push(['Any', null]);
        entries.push(['Never', null]);
        entries.push(['Class', null]);
        entries.push(['List', null]);
        entries.push(['FrozenList', null]);
        entries.push(['Optional', null]);
        entries.push(['Iteration', null]);
        entries.push(['Iterable', null]);
        entries.push(['Dict', null]);
        entries.push(['FrozenDict', null]);
        entries.push(['Function', null]);
    }
    const seen = new Set<string>();
    for (let scope: Scope|null = this.scope; scope; scope = scope.parent) {
      for (const key of scope.map.keys()) {
        if (!seen.has(key)) {
          const variable = scope.map.get(key);
          if (this.type === 'typesAndModulesOnly') {
            if (!variable?.type.isClassType() && !variable?.type.isModuleType()) {
              continue;
            }
          }
          entries.push([key, variable?.type.toString() || null]);
          seen.add(key);
        }
      }
    }
    return entries;
  }
}
