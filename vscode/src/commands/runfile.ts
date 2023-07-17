import * as vscode from 'vscode';



export async function runFileCommand() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    return;
  }
  const path = editor.document.uri.fsPath;
  const terminal =
    vscode.window.activeTerminal ||
    vscode.window.createTerminal(`RunMtotsFile`);
  terminal.sendText(`mtots ${path}`);
}
