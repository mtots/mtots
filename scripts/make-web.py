"""
Build Mtots with emscripten
"""
import argparse
import os
import shutil
from typing import List
from libmtots import (
    Compiler,
    Logger,
    getFreeTypeSources,
)


mtotsDir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
outDir = os.path.join(mtotsDir, 'out')
oDir = os.path.join(outDir, 'web-o')
exeDir = os.path.join(outDir, 'web')
exePath = os.path.join(exeDir, 'index.html')
version = (0, 0, 1)
versionString = '.'.join(map(str, version))

aparser = argparse.ArgumentParser()
aparser.add_argument('--package', '-p', type=str, required=True)
aparser.add_argument('--verbose', '-v', default=False, action='store_true')
aparser.add_argument('--release', '-r', default=False, action='store_true')
aparser.add_argument('--clean', '-c', default=False, action='store_true')
args = aparser.parse_args()
packagePath: str = args.package
verbose: bool = args.verbose
release: bool = args.release
clean: bool = args.clean

packageFileName = os.path.basename(packagePath)
packageBaseName = (
    packageFileName.removesuffix('.zip').removesuffix('.mtzip'))
logger = Logger(verbose)

CC = 'emcc'
WARNING_FLAGS = ("-Wall", "-Werror", "-Wpedantic")
OPT_FLAGS = ('-O3', '-flto') if release else ('-O0', '-g')
compiler = Compiler(
    CC,
    flags=WARNING_FLAGS+OPT_FLAGS,
    oDir=oDir,
    logger=logger,
    release=release,
)


def build() -> None:
    if clean or release:
        shutil.rmtree(outDir, ignore_errors=True)
    os.makedirs(outDir, exist_ok=True)

    objectFiles: List[str] = []
    os.makedirs(oDir, exist_ok=True)

    # Build stb_image library
    compiler.buildLibrary(
        "stbimage",
        sources=[os.path.join(mtotsDir, "lib", "stbimage", "src", "mtotsa_stbimage.c")],
        flags=[
            "-std=c99",
            "-Ilib/stbimage/include",
            "-Wno-comment",
            "-Wno-unused-function",
        ],
        objectFiles=objectFiles)

    # Build miniz library
    compiler.buildLibrary(
        "miniz",
        sources=[os.path.join(mtotsDir, "lib", "miniz", "src", "miniz.c")],
        flags=[
            "-std=c89",
            *WARNING_FLAGS,
            *OPT_FLAGS,
            "-Ilib/miniz/src",
        ],
        objectFiles=objectFiles)

    # Build FreeType library
    compiler.buildLibrary(
        "freetype",
        sources=getFreeTypeSources(),
        flags=[
            "-std=c99",
            *WARNING_FLAGS,
            *OPT_FLAGS,
            "-DFT2_BUILD_LIBRARY",
            "-Ilib/freetype/include",
        ],
        objectFiles=objectFiles)

    compiler.buildLibrary(
        "lodepng",
        sources=[os.path.join(mtotsDir, "lib", "lodepng", "src", "lodepng.c")],
        flags=[
            "-std=c89",
            *WARNING_FLAGS,
            *OPT_FLAGS,
            "-Ilib/lodepng/src",
        ],
        objectFiles=objectFiles)

    shutil.rmtree(exeDir, ignore_errors=True)
    os.makedirs(exeDir, exist_ok=True)
    compiler.buildMtots(
        exePath=exePath,
        flags=(
            # not available in emscripten
            "-DMTOTS_USE_PRINTFLIKE=0",

            # Required for FreeType
            "-Wno-long-long",

            # Start script
            f'-DMTOTS_WEB_START_SCRIPT=' +
            f'"/home/web_user/git/mtots/apps/{packageFileName}"',

            # Use a barebones shell file ideal for hosting on itch.io
            "--shell-file", f"{mtotsDir}/projects/web/minimal-modified.html",

            # Preloaded files
            '--preload-file',
                f'{packagePath}@/home/web_user/git/mtots/apps/{packageFileName}',
            '--preload-file',
                f'{mtotsDir}/root@/home/web_user/git/mtots/root',
            '-DMTOTSPATH_FALLBACK="/home/web_user/git/mtots/root"',

            # Allows the script to allocate memory as needed
            "-sALLOW_MEMORY_GROWTH",

            # SDL
            "-sUSE_SDL=2",
        ),
        objectFiles=objectFiles,
    )
    shutil.make_archive(
        base_name=os.path.join(outDir, 'webgame'),
        format='zip',
        root_dir=os.path.join(outDir, 'web'))



if __name__ == "__main__":
    build()
