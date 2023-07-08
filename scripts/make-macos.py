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
    buildArchive,
    getFreeTypeSources,
)


mtotsDir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
outDir = os.path.join(mtotsDir, 'out')
oDir = os.path.join(mtotsDir, 'out', 'desktop-o')
exePath = os.path.join(outDir, 'desktop', 'mtots')
version = (0, 0, 1)
versionString = '.'.join(map(str, version))

aparser = argparse.ArgumentParser()
aparser.add_argument('--verbose', '-v', default=False, action='store_true')
aparser.add_argument('--release', '-r', default=False, action='store_true')
aparser.add_argument('--clean', '-c', default=False, action='store_true')
aparser.add_argument('--ditto', dest='ditto', action='store_true')
aparser.add_argument('--no-ditto', dest='ditto', action='store_false')
args = aparser.parse_args()
verbose: bool = args.verbose
release: bool = args.release
clean: bool = args.clean
useDitto: bool = args.ditto
logger = Logger(verbose)
compiler = Compiler(
    cc="clang",
    flags=
        # warning flags
        ("-Wall", "-Werror", "-Wpedantic", "-Wextra") +
        # opt flags
        (('-O3', '-flto') if release else ('-O0', '-g')),
    oDir=oDir,
    logger=logger,
)


def build() -> None:
    if clean or release:
        shutil.rmtree(outDir, ignore_errors=True)
    os.makedirs(outDir, exist_ok=True)

    logger.debug("Copying `root`")
    os.makedirs(os.path.join(outDir, 'desktop'), exist_ok=True)
    shutil.rmtree(os.path.join(outDir, 'desktop', 'root'), ignore_errors=True)
    shutil.copytree(os.path.join(mtotsDir, 'root'), os.path.join(outDir, 'desktop', 'root'))

    objectFiles: List[str] = []
    os.makedirs(oDir, exist_ok=True)

    # Build stb_image library
    compiler.buildLibrary(
        "stbimage",
        sources=[os.path.join(mtotsDir, "lib", "stbimage", "src", "mtotsa_stbimage.c")],
        flags=[
            "-std=c89",
            "-Ilib/stbimage/include",
            "-Wno-comment",
            "-Wno-unused-function",
        ],
        objectFiles=objectFiles)

    # Build miniz library
    compiler.buildLibrary(
        "miniz",
        sources=[os.path.join(mtotsDir, "lib", "miniz", "src", "miniz.c")],
        flags=["-std=c89", "-Ilib/miniz/src"],
        objectFiles=objectFiles)

    # Build FreeType library
    compiler.buildLibrary(
        "freetype",
        sources=getFreeTypeSources(),
        flags=["-std=c99", "-DFT2_BUILD_LIBRARY", "-Ilib/freetype/include"],
        objectFiles=objectFiles)

    compiler.buildLibrary(
        "lodepng",
        sources=[os.path.join(mtotsDir, "lib", "lodepng", "src", "lodepng.c")],
        flags=["-std=c89", "-Ilib/lodepng/src",],
        objectFiles=objectFiles)

    compiler.buildMtots(
        exePath=exePath,
        flags=(
            "-DMTOTS_USE_PRINTFLIKE=1",

            # "-Weverything",

            # This is useful for
            #  * catching functions that were meant to be static
            #  * zero-argument functions that do not have 'void'
            #    in the parameter list
            "-Wmissing-prototypes",

            # Forces rewrite of what might look like ambiguities in some cases
            "-Wcomma",

            # TODO: Re-enable these warnings when I get a chance
            # to address all the places where they are used.
            "-Wno-sign-conversion",
            "-Wno-shorten-64-to-32",
            "-Wno-implicit-float-conversion",
            "-Wno-implicit-int-conversion",
            "-Wno-float-conversion",
            "-Wno-double-promotion",

            # These seem ok to turn off
            "-Wno-poison-system-directories",
            "-Wno-padded",
            "-Wno-undef",
            "-Wno-float-equal",
            "-Wno-switch-enum",

            # Covered switch defaults allow me to handle potentially bad cases
            # where an enum variable contains an invalid value
            "-Wno-covered-switch-default",

            # Sometimes some macros are 'expected' to fit a pattern, but might
            # not be used (yet). E.g. 'IS_WINDOW' in 'mtots_m_gg.c'.
            "-Wno-unused-macros",

            # Documentation Errors from freetype
            # These seem ok to turn off, but I only need to turn them
            # off because I needed to include freetype headers.
            # TODO: add an adapter to the freetype source so that we do not
            # have to add accomodations here for freetype headers.
            "-Wno-documentation-unknown-command",
            "-Wno-documentation",

            # SDL
            "-framework", "AudioToolbox",
            "-framework", "AudioToolbox",
            "-framework", "Carbon",
            "-framework", "Cocoa",
            "-framework", "CoreAudio",
            "-framework", "CoreFoundation",
            "-framework", "CoreVideo",
            "-framework", "ForceFeedback",
            "-framework", "GameController",
            "-framework", "IOKit",
            "-framework", "CoreHaptics",
            "-framework", "Metal",
            "-Ilib/sdl2/include",
            f"{mtotsDir}/lib/sdl2/targets/macos/libSDL2.a",
        ),
        objectFiles=objectFiles,
    )
    if release:
        buildArchive(
            versionString=versionString,
            logger=logger,
            outDir=outDir,
            platform='macos',
            useDitto=useDitto,
        )


if __name__ == "__main__":
    build()
