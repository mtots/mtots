import * as vscode from 'vscode';
import { MError } from '../lang/error';
import { warn } from '../debug';

export function getSelectionOrAllText(editor: vscode.TextEditor) {
  const selection =
    editor.selection.isEmpty ?
      new vscode.Range(
        editor.document.lineAt(0).range.start,
        editor.document.lineAt(editor.document.lineCount - 1).range.end) :
      new vscode.Range(
        editor.selection.start,
        editor.selection.end);
  return editor.document.getText(selection);
}

export async function writeToNewEditor(
    f: (emit: (m: string) => void) => void,
    language: string = 'plaintext') {
  const document = await vscode.workspace.openTextDocument({
    content: '',
    language: language,
  });
  let insertText = '';
  function emit(m: string) {
    insertText += m;
  }
  try {
    f(emit);
  } catch (e) {
    if (e instanceof MError) {
      insertText += `ERROR: ${e.message}`;
      insertText += `  ${e.location.range.start.line + 1}:`;
      insertText += `${e.location.range.start.column + 1}`;
    } else {
      warn('' + e);
      throw e;
    }
  }
  const edit = new vscode.WorkspaceEdit();
  edit.insert(document.uri, new vscode.Position(0, 0), insertText);
  if (await vscode.workspace.applyEdit(edit)) {
    vscode.window.showTextDocument(document);
  }
}
