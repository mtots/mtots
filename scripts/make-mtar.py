"""
Create an `mtar` file from an app directory

An `mtar` file is a packaged collection of Mtots code and resources.
Basically the Mtots equivalent of Java's `jar` files.
"""
import shutil
import os
import json
import argparse
from typing import List, Set, Dict, Optional, Any

mtotsDir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
outDir = os.path.join(mtotsDir, 'out')
mtarDir = os.path.join(outDir, 'mtar')

aparser = argparse.ArgumentParser()
aparser.add_argument('--app-dir', '-a', type=str, required=True)
args = aparser.parse_args()
appDir: str = args.app_dir
appName: str = os.path.basename(appDir)
mtarPath = os.path.join(mtarDir, appName + '.mtar')
mtarTmpDir = os.path.join(outDir, 'tmp-mtar')

searchDirs: List[str] = [os.path.dirname(appDir)]

errStack: List[str] = []

def getAppDepNames(appDir: str) -> Set[str]:
    appJSONPath = os.path.join(appDir, 'app.json')
    if os.path.isfile(appJSONPath):
        with open(appJSONPath) as f:
            jsonData = json.load(f)
            if isinstance(jsonData, dict) and isinstance(jsonData['deps'], list):
                deps: List[Any] = jsonData['deps']
                ret: Set[str] = set([appDir])
                for dep in deps:
                    if isinstance(dep, str):
                        ret.add(dep)
                    else:
                        raise TypeError(f"Dependency is not a str {dep!r}")
                return ret
    return set()


def findAppDir(appName: str) -> Optional[str]:
    for searchDir in searchDirs:
        depDir = os.path.join(searchDir, appName)
        if os.path.isdir(depDir):
            return depDir
    return None


def formatStackMessage(stack: List[str]) -> str:
    out: List[str] = []
    for a, b in zip(stack[::2], stack[1::2]):
        out.append(f'\n  {a} depends on {b}')
    return ''.join(out)


def getAppDepClosure(
        appDir: str,
        cache: Dict[str, Set[str]],
        stack: List[str]) -> Set[str]:
    cached = cache.get(appDir, None)
    if cached is not None:
        return cached

    if appDir in stack:
        raise Exception(f"Circular dependency{formatStackMessage(stack + [appDir])}")

    stack.append(appDir)
    try:
        deps = {appDir}
        for depName in getAppDepNames(appDir):
            dep = findAppDir(depName)
            if dep is None:
                raise Exception(
                    f"Could not find appdir for {depName!r}"
                    f"{formatStackMessage(stack)}")
            deps |= getAppDepClosure(dep, cache, stack)

        cache[appDir] = deps
        return deps
    finally:
        stack.pop()


def main() -> None:
    shutil.rmtree(mtarDir, ignore_errors=True)
    shutil.rmtree(mtarTmpDir, ignore_errors=True)
    os.makedirs(mtarDir, exist_ok=True)
    os.makedirs(mtarTmpDir, exist_ok=True)
    appDirs = getAppDepClosure(appDir, {}, [])
    for adir in appDirs:
        shutil.copytree(
            adir, os.path.join(mtarTmpDir, os.path.basename(adir)))
    shutil.make_archive(
        base_name=os.path.join(mtarDir, appName),
        format='zip',
        root_dir=os.path.join(mtarTmpDir))
    os.rename(os.path.join(mtarDir, appName + '.zip'), mtarPath)


if __name__ == '__main__':
    main()
