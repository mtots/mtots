import * as vscode from "vscode"
import * as converter from "./converter"
import { Registry } from "./registry";


export function newSignatureHelpProvider(registry: Registry): vscode.SignatureHelpProvider {
  return {
    async provideSignatureHelp(document, position, token, context) {
      const module = await registry.refresh(document);
      const sh = module?.findSignatureHelper(converter.convertPosition(position));
      if (!sh) {
        return null;
      }
      const parameterLabels = sh.parameterNames ?
        sh.parameterNames.map((n, i) => `${n} ${sh.parameterTypes[i]}`) :
        sh.parameterTypes.map((t, i) => `arg${i} ${t}`);

      const help = new vscode.SignatureHelp();
      help.activeParameter = sh.parameterIndex;
      help.activeSignature = 0;
      const signatureInformation = new vscode.SignatureInformation(
        (sh.functionName || '') +
        `(${parameterLabels.join(', ')})` +
        (sh.returnType ? ` ${sh.returnType}` : ''));
      if (sh.functionDocumentation) {
        signatureInformation.documentation = sh.functionDocumentation;
      }
      signatureInformation.activeParameter = sh.parameterIndex;
      signatureInformation.parameters.push(...sh.parameterTypes.map((pt, i) => {
        return new vscode.ParameterInformation(parameterLabels[i]);
      }));
      help.signatures = [signatureInformation];
      return help;
    },
  }
}
