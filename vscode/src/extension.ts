import * as vscode from 'vscode';
import { tokenizeCommand } from './commands/tokenize';
import { parseCommand } from './commands/parse';
import { Registry } from './extension/registry';
import { newDefinitionProvider } from './extension/definitionprovider';
import { newHoverProvider } from './extension/hoverprovider';
import { newSignatureHelpProvider } from './extension/shprovider';
import { newCompletionProvider } from './extension/completionprovider';



export function activate(context: vscode.ExtensionContext) {
  const sub = (item: vscode.Disposable) => context.subscriptions.push(item);

  const registry = new Registry();

  sub(vscode.commands.registerCommand(
    'mtots.tokenize',
    tokenizeCommand));
  sub(vscode.commands.registerCommand(
    'mtots.parse',
    parseCommand));

  if (vscode.window.activeTextEditor &&
      vscode.window.activeTextEditor.document.languageId === 'mtots') {
    const document = vscode.window.activeTextEditor.document;
    (async () => {
      await registry.refresh(document);
    })();
  }

  sub(vscode.workspace.onDidSaveTextDocument(async document => {
    if (document.languageId === 'mtots') {
      await registry.refresh(document);
    }
  }));

  sub(vscode.window.onDidChangeActiveTextEditor(async editor => {
    if (editor?.document.languageId === 'mtots') {
      await registry.refresh(editor.document);
    }
  }));

  sub(vscode.workspace.onDidCloseTextDocument(async document => {
    // NOTE that there is some asymmetry here.
    // We do not want to refresh the registry for a document
    // that is merely opened. We intentionally try to only run it
    // if a user has opened the document and made it visible.
    //
    // On the other hand, we might not necessarily want to
    // delete data for a tab that the user just closed. They might
    // want to open it again.
    registry.delete(document.uri);
  }));

  sub(vscode.languages.registerDefinitionProvider(
    { language: 'mtots' },
    newDefinitionProvider(registry)));
  sub(vscode.languages.registerHoverProvider(
    { language: 'mtots' },
    newHoverProvider(registry)));
  sub(vscode.languages.registerSignatureHelpProvider(
    { language: 'mtots' },
    newSignatureHelpProvider(registry), '(', ','));
  sub(vscode.languages.registerCompletionItemProvider(
    { language: 'mtots' },
    newCompletionProvider(registry), '.'));
}
