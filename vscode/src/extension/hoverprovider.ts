import * as vscode from 'vscode';
import * as ir from '../lang/ir';
import { Registry } from "./registry";
import { convertPosition } from './converter';
import { Variable } from '../lang/ir/scope';


function formatDocstr(docstr: string): string {
  const lines = docstr.split('\n');
  let depth = Math.max(...lines.map(line => line.length));
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].trim() === '') {
      lines[i] = '';
    } else {
      depth = Math.min(depth, lines[i].length - lines[i].trimStart().length);
    }
  }
  for (let i = 0; i < lines.length; i++) {
    lines[i] = lines[i].substring(depth);
  }
  return lines.join('\n');
}

export function newHoverProvider(registry: Registry): vscode.HoverProvider {
  return {
    async provideHover(document, position, token) {
      const module = await registry.refresh(document);
      const markedStrings: vscode.MarkdownString[] = [];
      const usage = module.findUsage(convertPosition(position));
      if (usage) {
        const typeInfo = new vscode.MarkdownString();
        markedStrings.push(typeInfo);
        typeInfo.appendCodeblock(ir.formatVariable(usage.variable));

        const funcType = usage.variable.type.asFunctionType();
        if (funcType && funcType !== usage.variable.type) {
          const callInfo = new vscode.MarkdownString();
          markedStrings.push(callInfo);
          callInfo.appendCodeblock(ir.formatVariable(
            new Variable(true, usage.variable.identifier, funcType, null)));
        }

        if (usage.boundType) {
          const bindInfo = new vscode.MarkdownString();
          markedStrings.push(bindInfo);
          bindInfo.appendCodeblock(ir.formatVariable(
            new Variable(true, usage.variable.identifier, usage.boundType, null)));
        }

        if (usage.variable.documentation) {
          const documentation = new vscode.MarkdownString();
          markedStrings.push(documentation);
          documentation.appendMarkdown(
            formatDocstr(usage.variable.documentation));
        }
      }
      return new vscode.Hover(markedStrings);
    },
  }
}
