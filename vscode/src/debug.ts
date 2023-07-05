import * as vscode from 'vscode';

const channel = vscode.window.createOutputChannel('mtots');

export function debug(message: string) {
  if (false) {
    channel.appendLine(message);
    console.log(`debug ${message}`);
  }
}

export function info(message: string) {
  channel.appendLine(message);
}

export function warn(message: string) {
  channel.appendLine(message);
  console.log(`warn ${message}`);
}

export function swrap<R>(tag: string, item: any, f: () => R): R {
  debug(`BEGIN ${tag} ${item}`);
  try {
    const result = f();
    debug(`END ${tag} ${item}`);
    return result;
  } catch (e) {
    debug(`THROW ${tag} ${item}`);
    throw e;
  }
}

export async function awrap<R>(tag: string, item: any, f: () => Promise<R>): Promise<R> {
  debug(`BEGIN ${tag} ${item}`);
  try {
    const result = await f();
    debug(`END ${tag} ${item}`);
    return result;
  } catch (e) {
    debug(`THROW ${tag} ${item}`);
    throw e;
  }
}
