import * as vscode from 'vscode';
import * as path from 'path';
import { which } from '../sys/which';

const WINDOWS = process.platform === 'win32';
const SEPARATOR = WINDOWS ? ';' : ':';
const MTOTSPATH = process.env.MTOTSPATH || '';
// const HOME = process.env.HOME || process.env.USERPROFILE || '';
// const HOME_URI = HOME.length > 0 ? vscode.Uri.file(HOME) : null;
// const MTOTS_HOME = HOME_URI ? vscode.Uri.joinPath(HOME_URI, 'git', 'mtots') : null;
const MTOTSPATH_URIS =
  MTOTSPATH.split(SEPARATOR, 99999).
    filter(p => p.length > 0).
    map(p => vscode.Uri.file(p));
const GLOBAL_ROOTS = [
  ...MTOTSPATH_URIS,
  // ...(MTOTS_HOME ? [
  //   vscode.Uri.joinPath(MTOTS_HOME, 'root'),
  //   vscode.Uri.joinPath(MTOTS_HOME, 'apps'),
  // ] : []),
];
let resolvedExeRoot = false;
let exeRoot: vscode.Uri | null = null;

async function getExeRoot(): Promise<vscode.Uri | null> {
  if (!resolvedExeRoot) {
    resolvedExeRoot = true;
    const mtotsExePath = await which('mtots');
    if (!mtotsExePath) {
      return null;
    }
    return exeRoot = vscode.Uri.joinPath(vscode.Uri.file(path.dirname(mtotsExePath)), 'root');
  }
  return exeRoot;
}

async function listRoots(workspaceFolderUris: vscode.Uri[]): Promise<vscode.Uri[]> {
  const roots = [
    ...workspaceFolderUris.map(workspaceFolderUri => [
      workspaceFolderUri,
      vscode.Uri.joinPath(workspaceFolderUri, 'src'),
      vscode.Uri.joinPath(workspaceFolderUri, 'apps'),
      vscode.Uri.joinPath(workspaceFolderUri, 'root'),
    ], 1).flat(),
    ...GLOBAL_ROOTS,
  ];
  const eRoot = await getExeRoot();
  if (eRoot) {
    roots.push(eRoot);
  }
  return roots;
}

function listUriCandidates(root: vscode.Uri, relativeFilePath: string): vscode.Uri[] {
  return [
    vscode.Uri.joinPath(root, relativeFilePath, '__init__.types.mtots'),
    vscode.Uri.joinPath(root, relativeFilePath, '__init__.mtots'),
    vscode.Uri.joinPath(root, relativeFilePath + '.types.mtots'),
    vscode.Uri.joinPath(root, relativeFilePath + '.mtots'),
  ];
}

export class ModuleFinder {
  async find(moduleName: string): Promise<vscode.TextDocument | null> {
    const relativeFilePath = moduleName.split('.').join('/');
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (workspaceFolders) {
      for (const root of await listRoots(workspaceFolders.map(wf => wf.uri))) {
        for (const uri of listUriCandidates(root, relativeFilePath)) {
          try {
            return await vscode.workspace.openTextDocument(uri);
          } catch (e) {
            // File could not be opened... assume it doesn't exist.
          }
        }
      }
    }
    return null;
  }
}
