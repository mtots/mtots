import * as vscode from 'vscode';

export async function runFileCommand() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    return;
  }
  await editor.document.save();
  const path = editor.document.uri.fsPath;
  const terminal =
    vscode.window.activeTerminal ||
    vscode.window.createTerminal();
  terminal.sendText(`mtots ${path}`);
}
