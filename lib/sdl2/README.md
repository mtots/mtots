SDL2 library

Modified to build static libraries that my
binary can link to.

SDL2-2.26.5.zip was downloaded from https://github.com/libsdl-org/SDL/releases/download/release-2.26.5/SDL2-2.26.5.zip
Referenced by: https://github.com/libsdl-org/SDL/releases/tag/release-2.26.5

The extracted sources were modified a bit to get the static build as desired. See the `patchfiles` directory
for files that were potentially changed.

# MacOS

`libSDL2.a` and `default.metallib` were created by building the Xcode project after editing the scheme
to produce a release build.

# Windows

I am just going to ignore 32-bit windows platforms.

As of windows 11, Windows ARM64 can run x86/x64 binaries
without modification.

So I build just a single win64 lib for Windows.

My steps to produce `SDL2.lib` are:
    * extract SDL2-2.26.5.zip
    * Open SDL2-2.26.5/VisualC/SDL.sln with Visual Studio 2022
    * Set build config from "Debug" to "Release" and also "x64"
    * Right click "SDL" in solution explorer and click "Properties"
    * Go to "Configuration Properties -> General" and change
        "Configuration Type" from "Dynamic Library (.dll)" to
        "Static library (.lib)".
    * Build the library. If you just press the play button,
        Visual Studio might complain that the product is not
        runnable.
        You should see the static library at x64/Release/SDL2.lib.

These setting changes listed above should be reflected in the files I've commited in `patchfiles/../VisualC`.

To actually link this:
    * In the `mtots` project at `mtots/projects/windows/mtots`.
        In Project Properties > Linker > Input, in additional dependencies,
        include:
        ```
        SDL2.lib
        imm32.lib
        winmm.lib
        setupapi.lib
        version.lib
        ```
    These additional dependencies (ofc, SDL2.lib is needed, but
    I mean the other ones) are required when linking to the SDL2 library
    statically - these are the dependencies that SDL2.lib itself
    depends on.

    * Also make sure to include the directory containing
        `SDL2.lib` to "Project Properties > Linker > General > Additional Library Directories".


Reference:
https://stackoverflow.com/a/66523216

Unlike in the post, I did not need CMake.
