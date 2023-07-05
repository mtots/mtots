import * as vscode from 'vscode';
import { Registry } from "./registry";
import { convertPosition } from './converter';


export function newCompletionProvider(registry: Registry): vscode.CompletionItemProvider {
  return {
    async provideCompletionItems(document, position, token, context) {
      const module = await registry.refresh(document);
      const cpoint = module.findCompletionPoint(convertPosition(position));
      if (!cpoint) {
        return null;
      }
      return cpoint.getEntries().map(([name, detail]) => {
        const item = new vscode.CompletionItem(name);
        if (detail) {
          item.detail = detail;
        }
        if (name.startsWith('_')) {
          // private should have low-pri
          item.sortText = '~' + name;
        } else {
          // default priority
          item.sortText = '5' + name;
        }
        return item;
      });
    },
  }
}
