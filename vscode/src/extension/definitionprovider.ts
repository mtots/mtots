import * as vscode from 'vscode';
import { Registry } from "./registry";
import { convertMLocation, convertPosition } from './converter';
import { BUILTIN_LOCATION } from '../lang/location';
import * as ir from '../lang/ir';


export function newDefinitionProvider(registry: Registry): vscode.DefinitionProvider {
  return {
    async provideDefinition(document, position, token) {
      const module = registry.getModule(document.uri);
      const usage = module?.findUsage(convertPosition(position));
      if (usage) {
        const variableType = usage.variable.type;
        if (variableType instanceof ir.ModuleType) {
          return convertMLocation(variableType.module.file.location);
        }
        if (usage.variable.identifier.location !== BUILTIN_LOCATION) {
          return convertMLocation(usage.variable.identifier.location);
        }
      }
      return null;
    },
  }
}
