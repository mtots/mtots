import * as vscode from 'vscode';
import * as ast from '../lang/ast';
import * as ir from '../lang/ir';
import * as converter from './converter';
import { ModuleFinder } from './modulefinder';
import { parse } from '../lang/parser';
import { solve } from '../lang/solver';
import { awrap, debug, warn } from '../debug';

const BUILTIN_MODULE_NAME = '__builtin__';
const MAIN_MODULE_NAME = '__main__';

export class Registry {
  readonly diagnostics = vscode.languages.createDiagnosticCollection('mtots');
  readonly moduleFinder = new ModuleFinder();

  /** Maps URI strings to their RegistryEntry */
  readonly entryMap = new Map<string, RegistryEntry>();

  delete(uri: vscode.Uri) {
    this.entryMap.delete(uri.toString());
  }

  getModule(uri: vscode.Uri): ir.Module | null {
    const uriString = uri.toString();
    const entry = this.entryMap.get(uriString);
    if (!entry) {
      return null;
    }
    return entry.moduleMap.get(MAIN_MODULE_NAME) || null;
  }

  async refresh(document: vscode.TextDocument): Promise<ir.Module> {
    return await awrap('refresh', document.uri, () => this._refresh(document));
  }

  async _refresh(document: vscode.TextDocument): Promise<ir.Module> {
    try {
      const entry = (() => {
        const uriString = document.uri.toString();
        const foundEntry = this.entryMap.get(uriString);
        if (foundEntry) {
          return foundEntry;
        }
        const e = new RegistryEntry(document.uri);
        this.entryMap.set(uriString, e);
        return e;
      })();
      await this.refreshEntry(document, entry);

      const module = entry.moduleMap.get(MAIN_MODULE_NAME);
      if (!module) {
        // TODO: more structured assertion error
        throw new Error(`Assertion error when loading ${document.uri}`);
      }

      this.diagnostics.set(document.uri, module.errors.map(e => {
        return {
          message: e.message,
          range: converter.convertMRange(e.location.range),
          severity: vscode.DiagnosticSeverity.Warning,
        }
      }));

      return module;
    } catch (e) {
      if (e instanceof Error) {
        warn(`EXCEPTION ${e.message}, ${e.stack}`);
      }
      throw e;
    }
  }

  private async refreshEntry(
      document: vscode.TextDocument,
      entry: RegistryEntry): Promise<void> {
    return await awrap(
      'refreshEntry',
      document.uri,
      () => entry._lock(() => this._refreshEntry(document, entry)));
  }

  private async _refreshEntry(
      document: vscode.TextDocument,
      entry: RegistryEntry): Promise<void> {

    // =====================================
    // PART 1: AST and IR caching mechanism
    // =====================================
    const previousFileCache = entry.fileCache;
    const newFileCache = new Map<string, ast.File>();
    entry.fileCache = newFileCache;
    const previousModuleMap = entry.moduleMap;
    const newModuleMap = new Map<string, ir.Module>();
    entry.moduleMap = newModuleMap;
    function parseDocument(document: vscode.TextDocument): ast.File {
      const key = `${document.uri.toString()}[${document.version}]`;
      const cached = previousFileCache.get(key);
      if (cached) {
        debug(`  PARSE (cached) ${document.uri}`);
        newFileCache.set(key, cached);
        return cached;
      }
      debug(`  PARSE ${document.uri}`);

      // TODO: There's a weird OOM bug related to files that do not
      // end in newlines. This is currently a little workaround
      // hack for that issue. Figure out what's at the root of this
      // problem and fix the bug.
      let text = document.getText();

      const file = parse(document.uri, text);

      debug(`  PARSE FINISHED ${document.uri}`);
      newFileCache.set(key, file);
      return file;
    }
    function getCachedModule(
        moduleName: string, file: ast.File): ir.Module | null {
      // We look at the cached module and check that:
      //   * the AST is exactly the same as before (*.file === file), and
      //   * IR of all dependencies are exactly the same as before.
      const previousModule = previousModuleMap.get(moduleName);
      if (previousModule && previousModule.file === file) {
        for (const imp of file.imports) {
          const dep = imp.module.toString();
          if (previousModuleMap.get(dep) !== newModuleMap.get(dep)) {
            return null;
          }
        }
        return previousModule;
      }
      return null;
    }
    function solveFile(moduleName: string, file: ast.File): ir.Module {
      const cachedModule = getCachedModule(moduleName, file);
      if (cachedModule) {
        debug(`  SOLVE (cached) ${moduleName}`);
        newModuleMap.set(moduleName, cachedModule);
        return cachedModule
      }
      debug(`  SOLVE ${moduleName}`);
      const module = solve(moduleName, file, newModuleMap);
      newModuleMap.set(moduleName, module);
      return module;
    }

    // =====================================================
    // Part 2: Load the ASTs from all the relevant files
    // =====================================================
    const astMap = new Map<string, ast.File>();
    const dependencyMap = new Map<string, Set<string>>();
    const seen = new Set([MAIN_MODULE_NAME]);
    const mainModuleAST = parseDocument(document);
    const stack: [string, ast.File][] = [[MAIN_MODULE_NAME, mainModuleAST]];
    const builtinModuleDocument = await this.moduleFinder.find(BUILTIN_MODULE_NAME);
    if (builtinModuleDocument) {
      stack.push([BUILTIN_MODULE_NAME, parseDocument(builtinModuleDocument)]);
    }
    await awrap('refreshPart2', document.uri, async () => {
      for (let pair; pair = stack.pop();) {
        const [moduleName, file] = pair;
        astMap.set(moduleName, file);
        const dependencies = new Set<string>();
        dependencyMap.set(moduleName, dependencies);
        for (const imp of file.imports) {
          const importModuleName = imp.module.toString();
          dependencies.add(importModuleName);
          if (!seen.has(importModuleName)) {
            seen.add(importModuleName);
            const importDocument =
              await this.moduleFinder.find(importModuleName);
            if (importDocument) {
              const importFile = parseDocument(importDocument);
              stack.push([importModuleName, importFile]);
            } else {
              // TODO: Error message about module not found
            }
          }
        }
      }
    });

    // =====================================================
    // Part 3: Create an ordered list of all the modules
    // based on their dependencies.
    // (i.e. run a topological sort on the modules)
    // =====================================================
    seen.clear();
    const orderedModuleList: string[] =
      builtinModuleDocument === document ? [] : [BUILTIN_MODULE_NAME];
    const trace = new Set<string>();
    function computeOrderedModuleList(moduleName: string) {
      if (seen.has(moduleName)) {
        return;
      }
      seen.add(moduleName);
      if (trace.has(moduleName)) {
        // TODO: Emit error message about recursive imports
        return;
      }
      trace.add(moduleName);
      for (const dep of dependencyMap.get(moduleName) || []) {
        computeOrderedModuleList(dep);
      }
      orderedModuleList.push(moduleName);
      trace.delete(moduleName);
    }
    computeOrderedModuleList(MAIN_MODULE_NAME);

    // =====================================================
    // Part 4: Solve all the modules
    // =====================================================
    for (const moduleName of orderedModuleList) {
      const file = astMap.get(moduleName);
      if (file) {
        solveFile(moduleName, file);
      } else {
        // TODO: assertion error
      }
    }
  }
}

class RegistryEntry {
  readonly mainDocumentUri: vscode.Uri;
  private lockPromise: Promise<void> | null = null;
  fileCache: Map<string, ast.File> = new Map();
  moduleMap: Map<string, ir.Module> = new Map();
  constructor(mainDocumentUri: vscode.Uri) {
    this.mainDocumentUri = mainDocumentUri;
  }

  // "locking" execution on this RegistryEntry.
  // This is meant to be used exclusively in Registry.refreshEntry
  //
  // If `refreshEntry` is already running, it's generally not useful to be
  // running another instance of it at the same time. as such, new requests
  // to refresh the entry will simply wait for the existing run to finish.
  //
  _lock(f: () => Promise<void>): Promise<void> {
    const prevPromise = this.lockPromise;
    if (prevPromise) {
      // Someone else is already in the block
      // Just wait for them to finish then return.
      return prevPromise;
    }
    // Otherwise, nobody is executing. Take ownership
    // NOTE: the promise executor is guaranteed to run synchronously.
    // https://stackoverflow.com/questions/31069453
    // We do this dance with resolver/rejecter because we want to make sure
    // that `this.lockPromise` is set before `f` starts.
    var resolver = (value: void | PromiseLike<void>) => {}
    var rejecter = (reason?: any) => {};
    const lockPromise = new Promise<void>((resolve, reject) => {
      resolver = resolve;
      rejecter = reject;
    });
    this.lockPromise = lockPromise;
    f().then(resolver, rejecter).finally(() => this.lockPromise = null);
    return lockPromise;
  }
}
