"""
Build Mtots with emscripten
"""
import argparse
import os
import shutil
from libmtots import (
    Logger,
    buildArchive,
    run,
)


mtotsDir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
outDir = os.path.join(mtotsDir, 'out')
exePath = os.path.join(outDir, 'desktop', 'mtots.exe')
version = (0, 0, 1)
versionString = '.'.join(map(str, version))
msbuildPath = os.path.join(mtotsDir, 'scripts', 'msbuild.bat')
slnPath = os.path.join(mtotsDir, 'projects', 'windows', 'mtots' ,'mtots.sln')

aparser = argparse.ArgumentParser()
aparser.add_argument('--verbose', '-v', default=False, action='store_true')
aparser.add_argument('--release', '-r', default=False, action='store_true')
aparser.add_argument('--clean', '-c', default=False, action='store_true')
args = aparser.parse_args()


verbose: bool = args.verbose
release: bool = args.release
clean: bool = args.clean
logger = Logger(verbose)
buildExePath = os.path.join(
    mtotsDir, 'projects', 'windows', 'mtots', 'x64',
    'Release' if release else 'Debug', 'mtots.exe')


def build() -> None:
    if clean or release:
        shutil.rmtree(outDir, ignore_errors=True)
    os.makedirs(outDir, exist_ok=True)

    logger.debug("Copying `root`")
    os.makedirs(os.path.join(outDir, 'desktop'), exist_ok=True)
    shutil.rmtree(os.path.join(outDir, 'desktop', 'root'), ignore_errors=True)
    shutil.copytree(os.path.join(mtotsDir, 'root'), os.path.join(outDir, 'desktop', 'root'))

    run([
        msbuildPath,
        slnPath,
    ] + (["/p:Configuration=Release"] if release else []))

    shutil.copy(buildExePath, exePath)

    buildArchive(
        versionString=versionString,
        logger=logger,
        outDir=outDir,
        platform='windows',
    )


if __name__ == "__main__":
    build()
