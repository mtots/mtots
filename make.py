"""
Build mtots
"""
import argparse
import os
import platform
import subprocess
import sys
import typing

REPO_DIR = os.path.dirname(os.path.realpath(__file__))

if REPO_DIR != os.getcwd():
    raise Exception("This make script is intended to be run from the repo directory")

SYSTEM_WINDOWS = "Windows"
SYSTEM_MACOS = "Darwin"
SYSTEM_LINUX = "Linux"
SYSTEM = platform.system()

CL_BAT = os.path.join(REPO_DIR, "misc", "windows", "cl.bat")

WINDOWS_BASE_FLAGS = [
    "/std:c11",
]

WINDOWS_DEBUG_FLAGS = []

WINDOWS_RELEASE_FLAGS = [
    "/O2",
    "/GL",
    "/DMTOTS_RELEASE=1",
]

MACOS_BASE_FLAGS = [
    # WARNINGS
    "-Wall",
    "-Werror",
    "-Wpedantic",
    "-Weverything",
    "-Wno-poison-system-directories",
    "-Wno-undef",
    "-Wno-unused-parameter",
    "-Wno-padded",
    "-Wno-cast-align",
    "-Wno-float-equal",
    "-Wno-missing-field-initializers",
    "-Wno-switch-enum",
    # flags I want to eventually remove
    "-Wno-implicit-int-conversion",
    "-Wno-shorten-64-to-32",
    "-Wno-sign-conversion",
    "-Wno-float-conversion",
    "-Wno-implicit-float-conversion",
    "-Wno-double-promotion",
    "-Wno-sign-compare",
    "-Wno-conditional-uninitialized",
    "-Wno-unused-macros",
    "-Wno-documentation-unknown-command",
    # math library (not needed on clang, but gcc on Linux often needs it)
    "-lm",
    # language standard
    "-std=c89",
]

MACOS_DEBUG_FLAGS = [
    "-g",
    "-fsanitize=address",
    "-O0",
]

MACOS_RELEASE_FLAGS = [
    "-O3",
    "-flto",
    "-DMTOTS_RELEASE=1",
]

MACOS_SDL_FLAGS = [
    "-F/Library/Frameworks",
    "-framework",
    "SDL2",
    "-DMTOTS_ENABLE_SDL=1",
    "-framework",
    "SDL2_ttf",
    "-DMTOTS_ENABLE_SDL_TTF=1",
    "-framework",
    "SDL2_mixer",
    "-DMTOTS_ENABLE_SDL_MIXER=1",
    "-framework",
    "SDL2_image",
    "-DMTOTS_ENABLE_SDL_IMAGE=1",
    "-Wno-documentation",
]

aparser = argparse.ArgumentParser()
aparser.add_argument("--verbose", "-v", default=False, action="store_true")
aparser.add_argument("--release", "-r", default=False, action="store_true")
aparser.add_argument("--no-compile", dest="compile", default=True, action="store_false")
aparser.add_argument("--test", "-t", default=False, action="store_true")
aparser.add_argument(
    "--enable-sdl",
    default=False,
    action="store_true",
    help="links SDL. Requires SDL framework to be installed",
)
args = aparser.parse_args()

VERBOSE: bool = args.verbose
RELEASE: bool = args.release
COMPILE: bool = args.compile
TEST: bool = args.test
ENABLE_SDL: bool = args.enable_sdl


def c_sources() -> typing.List[str]:
    srcs: typing.List[str] = []
    for filename in sorted(os.listdir("src")):
        if filename.endswith(".c"):
            srcs.append(os.path.join("src", filename))
    return srcs


def compile(release: bool):
    """
    Compile Mtots for the current platform and produces the binary at `mtots`
    """
    if VERBOSE:
        print("COMPILING mtots")
        print(f"  release = {release}")
        print(f"  ENABLE_SDL = {ENABLE_SDL}")
    args: typing.List[str] = []
    if SYSTEM == SYSTEM_WINDOWS:
        args.extend(
            [
                CL_BAT,
                *WINDOWS_BASE_FLAGS,
                *(WINDOWS_RELEASE_FLAGS if release else WINDOWS_DEBUG_FLAGS),
                *c_sources(),
                "/Fe:mtots",
            ]
        )
    elif SYSTEM == SYSTEM_MACOS:
        args.extend(
            [
                # using 'gcc' to access clang adds additional
                # compatibility checks
                "gcc",
                *MACOS_BASE_FLAGS,
                *(MACOS_RELEASE_FLAGS if release else MACOS_DEBUG_FLAGS),
                *(MACOS_SDL_FLAGS if ENABLE_SDL else []),
                *c_sources(),
                "-omtots",
            ]
        )
    elif SYSTEM == SYSTEM_LINUX:
        raise Exception("TODO: compile for Linux")
    else:
        # Assume other UNIX
        args.extend(["cc", *c_sources(), "-omtots"])
    if VERBOSE:
        print(f"  COMPILING-ARGS: ")
        print(f"    {args[0]}")
        for arg in args[1:]:
            print(f"      {arg}")
    subprocess.run(args, check=True)


def test():
    """
    Test the `mtots` binary
    """
    plat = "win" if os.name == "nt" else ""
    ansiRed = "\033[31m"
    ansiGreen = "\033[32m"
    ansiReset = "\033[0m"

    testDir = os.path.join(REPO_DIR, "test")
    mtotsPath = os.path.join(REPO_DIR, "mtots")

    dirnames = sorted(os.listdir(testDir))

    testSetCount = len(dirnames)
    testCount = passCount = 0

    print("TESTING mtots")
    print(f"  Found {testSetCount} test set(s)")

    for testSet in dirnames:
        testSetDir = os.path.join(testDir, testSet)
        filenames = sorted(os.listdir(testSetDir))
        scriptFilenames = [fn for fn in filenames if fn.endswith(".mtots")]

        print(f"    {testSet}")

        for sfn in scriptFilenames:
            base = sfn[: -len(".mtots")]

            scriptPath = os.path.join(testSetDir, sfn)

            expectNonzeroExit = False
            expectExit = 0
            expectExitFn = os.path.join(testSetDir, f"{base}.exit.txt")
            if os.path.exists(expectExitFn):
                with open(expectExitFn) as f:
                    expectExitStr = f.read().strip()
                    if expectExitStr.lower() == "any":
                        expectExit = None
                    elif expectExitStr.lower() == "nonzero":
                        expectExit = None
                        expectNonzeroExit = True
                    else:
                        expectExit = int(expectExitStr)

            expectOut = ""
            expectOutPlatFn = os.path.join(testSetDir, f"{base}.{plat}.out.txt")
            expectOutFn = (
                expectOutPlatFn
                if plat and os.path.exists(expectOutPlatFn)
                else os.path.join(testSetDir, f"{base}.out.txt")
            )
            if os.path.exists(expectOutFn):
                with open(expectOutFn) as f:
                    expectOut = f.read()

            expectErr = ""
            expectErrPlatFn = os.path.join(testSetDir, f"{base}.{plat}.err.txt")
            expectErrFn = (
                expectErrPlatFn
                if plat and os.path.exists(expectErrPlatFn)
                else os.path.join(testSetDir, f"{base}.err.txt")
            )
            if os.path.exists(expectErrFn):
                with open(expectErrFn) as f:
                    expectErr = f.read()

            sys.stdout.write(f"      testing {base}... ")

            proc = subprocess.run(
                [mtotsPath, os.path.relpath(scriptPath, REPO_DIR)],
                capture_output=True,
                text=True,
                cwd=REPO_DIR,
            )

            if expectExit is not None and proc.returncode != expectExit:
                print(ansiRed)
                print("FAILED (exit code)")
                print("##### Expected #####")
                print(expectExit)
                print("##### TO EQUAL #####")
                print(proc.returncode)
                print(f"##### STDOUT: #####")
                print(proc.stdout)
                print(f"##### STDERR: #####")
                print(proc.stderr)
                print(ansiReset)
            elif expectNonzeroExit and proc.returncode == 0:
                print(ansiRed)
                print("FAILED (exit code)")
                print("##### Expected #####")
                print("(nonzero)")
                print("##### TO EQUAL #####")
                print(proc.returncode)
                print(f"##### STDOUT: #####")
                print(proc.stdout)
                print(f"##### STDERR: #####")
                print(proc.stderr)
                print(ansiReset)
            elif expectOut != proc.stdout:
                print(ansiRed)
                print("FAILED (stdout)")
                print("##### Expected #####")
                print(proc.stdout)
                print("##### TO EQUAL #####")
                print(expectOut)
                print(ansiReset)
            elif expectErr != proc.stderr:
                print(ansiRed)
                print("FAILED (stderr)")
                print("##### Expected #####")
                print(proc.stderr)
                print("##### TO EQUAL #####")
                print(expectErr)
                print(ansiReset)
            else:
                passCount += 1
                print(f"{ansiGreen}OK{ansiReset}")
            testCount += 1

    if passCount == testCount:
        print("  ALL TESTS PASS")
    else:
        print(f"  Some tests {ansiRed}failed{ansiReset}:")
        print(f"    {passCount} / {testCount}")
        print(f"    {testCount - passCount} test(s) failed")

    exit(0 if passCount == testCount else 1)


if __name__ == "__main__":
    if COMPILE:
        compile(RELEASE)
    if TEST:
        test()
