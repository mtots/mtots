"""
Helpers for Mtots related Python scripts
"""
import sys
import subprocess
import contextlib
import os
import shutil
from typing import List, Iterable

mtotsDir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))


class Logger:
    def __init__(self, verbose: bool) -> None:
        self.verbose = verbose
        self._depth = 0

    @contextlib.contextmanager
    def indent(self):
        self._depth += 1
        yield
        self._depth -= 1

    def debug(self, message: str):
        if self.verbose:
            sys.stderr.write(f"DEBUG {'  ' * self._depth}{message}\n")


def run(args: List[str]) -> None:
    try:
        subprocess.run(args, check=True)
    except subprocess.CalledProcessError as e:
        sys.stderr.write(f'Failed process:\n')
        sys.stderr.write(f'  {args[0]}\n')
        for arg in args[1:]:
            sys.stderr.write(f'    {arg}\n')
        sys.stderr.write(f'Return code: {e.returncode}\n')
        sys.exit(1)


def getSources() -> List[str]:
    srcs: List[str] = []
    srcDir = os.path.join(mtotsDir, 'src')
    for filename in os.listdir(srcDir):
        if filename.endswith('.c'):
            srcs.append(os.path.join(srcDir, filename))
    return srcs


def getExtensionSources(extensionName: str) -> List[str]:
    srcs: List[str] = []
    srcDir = os.path.join(mtotsDir, 'src', 'extensions', extensionName)
    for filename in os.listdir(srcDir):
        if filename.endswith('.c'):
            srcs.append(os.path.join(srcDir, filename))
    return srcs


def getFreeTypeSources() -> List[str]:
    # These are the files listed in freetype/INSTALL.ANY
    srcs: List[str] = [
        "src/base/ftsystem.c",
        "src/base/ftinit.c",
        "src/base/ftdebug.c",
        "src/base/ftbase.c",
        "src/base/ftbbox.c",
        "src/base/ftglyph.c",
        "src/base/ftbdf.c",
        "src/base/ftbitmap.c",
        "src/base/ftcid.c",
        "src/base/ftfstype.c",
        "src/base/ftgasp.c",
        "src/base/ftgxval.c",
        "src/base/ftmm.c",
        "src/base/ftotval.c",
        "src/base/ftpatent.c",
        "src/base/ftpfr.c",
        "src/base/ftstroke.c",
        "src/base/ftsynth.c",
        "src/base/fttype1.c",
        "src/base/ftwinfnt.c",
        "src/base/ftmac.c",
        "src/bdf/bdf.c",
        "src/cff/cff.c",
        "src/cid/type1cid.c",
        "src/pcf/pcf.c",
        "src/pfr/pfr.c",
        "src/sfnt/sfnt.c",
        "src/truetype/truetype.c",
        "src/type1/type1.c",
        "src/type42/type42.c",
        "src/winfonts/winfnt.c",
        "src/raster/raster.c",
        "src/sdf/sdf.c",
        "src/smooth/smooth.c",
        "src/autofit/autofit.c",
        "src/cache/ftcache.c",
        "src/gzip/ftgzip.c",
        "src/lzw/ftlzw.c",
        "src/bzip2/ftbzip2.c",
        "src/gxvalid/gxvalid.c",
        "src/otvalid/otvalid.c",
        "src/psaux/psaux.c",
        "src/pshinter/pshinter.c",
        "src/psnames/psnames.c",
    ]
    srcs = [os.path.join(*src.split('/')) for src in srcs]
    return [os.path.join(mtotsDir, 'lib', 'freetype', src) for src in srcs]


def buildArchive(
        *,
        versionString: str,
        logger: Logger,
        outDir: str,
        platform: str,
        useDitto: bool = False):
    archiveRootName = 'mtots-' + versionString
    archiveBaseName = f"{archiveRootName}-{platform}"
    archiveName = f"{archiveBaseName}.zip"
    logger.debug(f"Building archive {archiveName}")
    shutil.rmtree(
        os.path.join(outDir, archiveRootName), ignore_errors=True)
    shutil.copytree(
        os.path.join(outDir, 'desktop'),
        os.path.join(outDir, archiveRootName))
    if useDitto:
        run([
            'ditto',
            '-c', '-k', '--keepParent',
            os.path.join(outDir, archiveRootName),
            os.path.join(outDir, archiveName)])
    else:
        shutil.make_archive(
            base_name=os.path.join(outDir, archiveBaseName),
            format='zip',
            root_dir=os.path.join(outDir),
            base_dir=archiveRootName)
    shutil.rmtree(
        os.path.join(outDir, archiveRootName),
        ignore_errors=True)


class Compiler:
    cc: str
    """
    C compiler. Usually, `gcc` or `clang
    """

    def __init__(
                self,
                cc: str,
                *,
                flags: Iterable[str],
                oDir: str,
                logger: Logger) -> None:
        self.cc = cc
        self.flags = tuple(flags)
        self.oDir = oDir
        self.logger = logger

    def buildLibrary(
            self,
            libname: str,
            *,
            sources: Iterable[str],
            flags: Iterable[str],
            objectFiles: List[str]):
        self.logger.debug(f"Building {libname}")
        os.makedirs(os.path.join(self.oDir, libname), exist_ok=True)
        with self.logger.indent():
            for path in sources:
                basename = os.path.basename(path).removesuffix(".c")
                opath = os.path.join(self.oDir, libname, f"{basename}.o")
                if os.path.exists(opath):
                    self.logger.debug(f"compiled {basename} (cached)")
                else:
                    self.logger.debug(f"compiling {basename}")
                    run([
                        self.cc,
                        *self.flags,
                        *flags,
                        "-c",
                        path,
                        "-o", opath,
                    ])
                objectFiles.append(opath)

    def buildMtots(
            self,
            *,
            exePath: str,
            flags: Iterable[str],
            objectFiles: List[str],
            finalFlags: Iterable[str]=()):
        self.logger.debug(f"compiling mtots")
        run([
            self.cc,

            "-std=c89",
            *self.flags,
            *flags,

            "-Isrc",
            "-fsanitize=address",

            "-o", exePath,

            # Include the stb_image library
            "-Ilib/stbimage/include",

            # Include the miniz library
            "-Ilib/miniz/src",
            "-Isrc/extensions/zip",
            "-DMTOTS_ENABLE_ZIP=1",
            *getExtensionSources('zip'),

            # Include the FreeType library
            "-Ilib/freetype/include",

            # Font module
            #   - depends on freetype
            "-DMTOTS_ENABLE_FONT=1",
            "-Isrc/extensions/font",
            *getExtensionSources('font'),

            # font.roboto.mono module
            #   - depends on font module
            "-DMTOTS_ENABLE_FONT_ROBOTO_MONO=1",
            "-Isrc/extensions/fontrm",
            *getExtensionSources('fontrm'),

            # SDL
            "-Isrc/extensions/sdl",
            *getExtensionSources('sdl'),

            # Audio module
            "-DMTOTS_ENABLE_AUDIO=1",
            "-Isrc/extensions/audio",
            *getExtensionSources('audio'),

            # Image module
            #   - depends on lodepng
            "-DMTOTS_ENABLE_IMAGE=1",
            "-Ilib/lodepng/src",
            "-Isrc/extensions/image",
            *getExtensionSources('image'),

            # Canvas module
            #   - depends on image module
            "-DMTOTS_ENABLE_CANVAS=1",
            "-Isrc/extensions/canvas",
            *getExtensionSources('canvas'),

            # gg module
            #   - depends on SDL
            #   - depends on image module
            #   - depends on font.roboto.mono module
            "-DMTOTS_ENABLE_GG=1",
            "-Isrc/extensions/gg",
            *getExtensionSources('gg'),

            # paco
            #   - depends on SDL
            "-DMTOTS_ENABLE_PACO=1",
            "-Isrc/extensions/paco",
            *getExtensionSources('paco'),

            # mtots sources
            *getSources(),

            # object files from libraries
            *objectFiles,

            *finalFlags,
        ])
