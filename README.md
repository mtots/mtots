# Mtots

The Mtots programming langauge

Currently, there is still quite a bit of work to do before Mtots is ready for prime time.

# What is Mtots?

I just wanted a language that that was:

1. **Fun** like **Python**\
  The language should be beginner friendly, but also easy for more seasoned programmers to have have fun and make prototypes with.
2. **Typed** like **Typescript**\
  There should be good IDE support and everything should be typed by default. Even if programmers can write untyped when needed and the runtime may ignore type annotations.
3. **Portable** like **Lua**\
  `gcc src/*.c` or `clang src/*.c` on this repository should just work. In fact, any ANSI C89 compiler shoudl be able to compile the core of the language. While the language should provide extensions that help make the language's environment feel more "batteries included", if you just want to take the core of the language and embed it somewhere, you should be able to very easily by taking some `c` and `h` files and including them in your C or C++ project.

# BUILDING

## MacOS

`scripts/make-macos.py`

On MacOS you need Xcode installed.

## Windows

`scripts/make-windows.py`

On Windows you need Visual Studio installed. Currently only tested with Visual Studio 2022

TODO: More details + add comment about the Visual Studio project at `projects/windows/mtots`

## Linux

`scripts/make-linux.py`

You need to have SDL2 development libraries pre-installed. A minimum of version `2.0.18` is required
and version `2.26.5` and above is recommended.

TODO: More details

## Web and WebAssembly

`scripts/make-web.py`

TODO: More details on how to use this to bundle mtots programs for web

## C89 and manual builds

Compiling all C files directly under `src` should produce a binary that runs
the interpreter without any extensions.

TODO: include some details on how one might go about adding the extensions
to the build, in particular, SDL

# IDE

IDE support is provided primarily through vscode.

The extension is available [here](https://marketplace.visualstudio.com/items?itemName=mtots.mtots)

The source for the extension is in this repository [here](vscode/).
