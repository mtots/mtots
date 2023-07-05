import * as vscode from 'vscode';
import { getSelectionOrAllText, writeToNewEditor } from './utils';
import { parse } from '../lang/parser';

export async function parseCommand() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    return;
  }
  const text = getSelectionOrAllText(editor);
  await writeToNewEditor(async emit => {
    const module = parse(editor.document.uri, text);
    emit(JSON.stringify(module, null, 2));
  }, 'json');
}
