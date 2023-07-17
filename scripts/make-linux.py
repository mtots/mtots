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
args = aparser.parse_args()
verbose: bool = args.verbose
release: bool = args.release
clean: bool = args.clean
logger = Logger(verbose)
compiler = Compiler(
    cc="gcc",
    flags=
        # warning flags
        (
            "-Wall",
            # "-Werror",
            "-Wpedantic", "-Wextra",
            "-Wno-unused-but-set-variable",
        ) +
        # opt flags
        (('-O3', '-flto') if release else ('-O0', '-g')),
    oDir=oDir,
    logger=logger,
    release=release,
)


def build() -> None:
    if clean:
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
        flags=["-std=c99", "-Ilib/miniz/src"],
        objectFiles=objectFiles)

    # Build FreeType library
    compiler.buildLibrary(
        "freetype",
        sources=getFreeTypeSources(),
        flags=[
            "-std=c99",
            "-DFT2_BUILD_LIBRARY",
            "-Ilib/freetype/include",
            "-Wno-dangling-pointer",
            "-Wno-error",
        ],
        objectFiles=objectFiles)

    compiler.buildLibrary(
        "lodepng",
        sources=[os.path.join(mtotsDir, "lib", "lodepng", "src", "lodepng.c")],
        flags=["-std=c99", "-Ilib/lodepng/src",],
        objectFiles=objectFiles)

    compiler.buildMtots(
        exePath=exePath,
        flags=(
            "-DMTOTS_USE_PRINTFLIKE=1",

            # # Try to keep this on if possible, but it should be ok to
            # # remove if necessary.
            # # I try to keep individual warnings explicitly enabled
            # # below so that they stay enabled even if the everything
            # # flag is omitted
            # "-Weverything",
            "-std=gnu99",

            # These are useful for
            #  * catching functions that were meant to be static
            #  * zero-argument functions that do not have 'void'
            #    in the parameter list
            "-Wmissing-prototypes",
            "-Wstrict-prototypes",

            # # Forces rewrite of what might look like ambiguities in some cases
            # "-Wcomma",

            # https://stackoverflow.com/a/58993253
            # TL;DR: In C89, you can call functions that have not been declared,
            # this can kind of help check this, although it probably is not critical
            # since other warnings will probably catch these as well
            "-Wbad-function-cast",

            # Other explicitly enabled warnings.
            # These are nice to have even if "-Weverything" is currently on,
            # it may be disabled at some point in time - explicitly listing warnings
            # allows these warnings to survive even if "-Weverything" is removed.
            "-Wshadow",
            "-Wcast-align",
            "-Wcast-qual",
            "-Wuninitialized",
            "-Wmissing-noreturn", # tenative - requires GNUC hacks or giving up C89

            # TODO: Re-enable these warnings when I get a chance
            # to address all the places in which they are violated.
            "-Wno-sign-compare",
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
            "-Wno-unused-parameter",
            "-Wno-missing-field-initializers",

            "-Wno-maybe-uninitialized",

            # Enabling this might actually mask some logic errors -
            # '-Wuninitialized' may be able to catch many situations in which
            # a logic error happens, but the forced assignment may hide it.
            "-Wno-conditional-uninitialized",

            # Sometimes it's easier to write some code that is unreachable than
            # to use macros. It should be mostly ok - most compilers should be
            # able to optimize them out.
            "-Wno-unreachable-code",
            "-Wno-unreachable-code-return",

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

            # Required for FreeType
            "-Wno-long-long",

            # SDL
            "-Ilib/sdl2/include",
        ),
        objectFiles=objectFiles,
        finalFlags=(
            "-lSDL2",
            "-lm",
        )
    )
    if release:
        buildArchive(
            versionString=versionString,
            logger=logger,
            outDir=outDir,
            platform='linux',
        )


if __name__ == "__main__":
    build()
