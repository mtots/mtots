import * as vscode from 'vscode';
import { lex } from '../lang/lexer';
import { getSelectionOrAllText, writeToNewEditor } from './utils';

export async function tokenizeCommand() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    return;
  }
  const text = getSelectionOrAllText(editor);
  const tokens = lex(editor.document.uri, text);

  await writeToNewEditor(emit => {
    for (const token of tokens) {
      emit(
        `${token.location.range.start.line + 1}:` +
        `${token.location.range.start.column + 1} - ` +
        `${token.location.range.end.line + 1}:` +
        `${token.location.range.end.column + 1} - ` +
        `${token.type} ` +
        `${token.value === null ? '' : JSON.stringify(token.value)}\n`);
    }
  });
}
