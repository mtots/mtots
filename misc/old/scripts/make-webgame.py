"""
Takes a directory containing an mtots app and creates a zipped up file suitable for publishing on itch.io

The file will be named `out/webgame.zip`
"""
import shutil
import os
import sys
import subprocess
import argparse

mtotsDir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
scriptsDir = os.path.join(mtotsDir, 'scripts')
aparser = argparse.ArgumentParser()
aparser.add_argument('--verbose', '-v', default=False, action='store_true')
aparser.add_argument('appdir')
args = aparser.parse_args()
appdir: str = args.appdir
verbose: bool = args.verbose


def runScript(path: str, *args: str) -> None:
    subprocess.run([
        sys.executable,
        path,
        *args,
    ], check=True)


def getMtarPath() -> str:
    basename = os.listdir(os.path.join(mtotsDir, 'out', 'mtar'))[0]
    return os.path.join(mtotsDir, 'out', 'mtar', basename)


def main():
    shutil.rmtree(os.path.join(mtotsDir, 'out', 'webgame.zip'), ignore_errors=True)
    runScript(os.path.join(scriptsDir, 'make-mtar.py'), '-a', appdir)
    runScript(
        os.path.join(scriptsDir, 'make-web.py'),
        *(['-v'] if verbose else []),
        '--release',
        '-p', getMtarPath())
    if not os.path.isfile(os.path.join(mtotsDir, 'out', 'webgame.zip')):
        raise Exception("ERROR, FILE NOT GENERATED")
    print("webgame written to out/webgame.zip")


if __name__ == '__main__':
    main()
