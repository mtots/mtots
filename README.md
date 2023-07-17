# Mtots

The Mtots programming langauge

Currently, there is still quite a bit of work to do before Mtots is ready for prime time.

# What is Mtots?

I made Mtots because I wanted a language that was:

1. **Fun** like **Python**\
  The language should be beginner friendly, but also easy for more seasoned programmers to have have fun and make prototypes with.
2. **Typed** like **Typescript**\
  There should be good IDE support and everything should be typed by default. Even if programmers can write untyped when needed and the runtime may ignore type annotations.
3. **Portable** like **Lua**\
  `gcc src/*.c` or `clang src/*.c` on this repository should just work. In fact, any ANSI C89 compiler shoudl be able to compile the core of the language. While the language should provide extensions that help make the language's environment feel more "batteries included", if you just want to take the core of the language and embed it somewhere, you should be able to very easily by taking some `c` and `h` files and including them in your C or C++ project.

# IDE Support

The recommended IDE to use with Mtots is VSCode.

The extension is available [here](https://marketplace.visualstudio.com/items?itemName=mtots.mtots)

The source for the extension is in this repository [here](vscode/).

# INSTALLATION

Currently, the main way to install Mtots is by building from source.

## MacOS

### Requirements
* Xcode command line tools (specifically, `clang` should be available)
* Python3 (system Python might work, but builds are tested with Python3.11)

### Build and Installation Steps

To build `mtots`, run the following command from the root of this repository:

```
python3 scripts/make-macos.py --verbose --release
```

If the command finishes successfully, it should produce the `mtots` binary at
`out/desktop/mtots`.

To test the `mtots` command you just built, run the following command:

```
python3 scripts/run-tests.py
```

To install the binary you just built and tested, run:

```
echo "export PATH=\"\$PATH:$PWD/out/desktop\"" >> ~/.zshrc
```

or add the output directory to your `PATH` in some way.

Now you should be able to use the `mtots` command to run Mtots scripts from the command line.

## Windows

### Requirements
* Visual Studio 2022 (other versions of VS may work, but are not tested)
* Python 3

### Build and Installation Steps

To build `mtots`, run the following command from the root of this repository:

```
python scripts\make-windows.py --verbose --release
```

If the command finishes successfully, it should produce the `mtots` binary at
`out\desktop\mtots`.

To test the `mtots` command you just built, run the following command:

```
python scripts\run-tests.py
```

To install the binary you just built and tested, add the folder containing the `mtots`
executable you just created to the `PATH`.

If you are not sure how to add a directory to the `PATH`, see
https://learn.microsoft.com/en-us/previous-versions/office/developer/sharepoint-2010/ee537574(v=office.14)

or seek help on a search engine

## Linux

### Requirements
* `gcc` (On Debian/Ubuntu distros: `apt install gcc`)
* `python3` (On Debian/Ubutnu distros: should come preinstalled)
* SDL2-dev (On Debian/Ubuntu distros: `apt install libsdl2-dev`)
  * SDL2 version must be at least `2.0.18` - version `2.26.5` and above is recommended
    since the Windows and MacOS builds are fixed at version `2.26.5`.

### Build and Installation Steps

To build `mtots`, run the following command from the root of this repository:

```
python3 scripts/make-linux.py --verbose --release
```

If the command finishes successfully, it should produce the `mtots` binary at
`out/desktop/mtots`.

To test the `mtots` command you just built, run the following command:

```
python3 scripts/run-tests.py
```

To install the binary you just built and tested, add `$PWD/out/desktop` to the `PATH`
in some way.

If you are using `bash`, you can run the following command:

```
echo "export PATH=\"\$PATH:$PWD/out/desktop\"" >> ~/.bashrc
```

Now you should be able to use the `mtots` command to run Mtots scripts from the command line.

## Webassembly and itch.io

If you have made a game with Mtots and would like to share with your friends on itch.io,
you can run the the `make-webgame.py` script to easily create a zip file you can upload
that will let your friends play the game directly from the browser.

Your game should be a directory containing at least a `main.mtots` file containing
the entrypoint to your game.

Then to create your zip file, run:

```
python3 scripts/make-webgame.py path/to/your/game/directory
```

And if the command runs successfully, you should have a file

```
out/webgame.zip
```

that you can upload to itch.io.

For example, to package `ggdemo` for itch.io, you would run

```
python3 scripts/make-webgame.py apps/ggdemo
```

## C89 and manual builds

You may have a bespoke environment not listed in any of the platforms above.

Compiling all C files directly under `src` should produce a binary that runs
the interpreter without any extensions.

To include the extensions, you will want to look at each fo the directories
in `src/extensions` and add them one by one and enabling certain macro flags.
Many of them will also require that you build 3rd party C libraries available
in the `lib` directory.

All of the 3rd party libraries in `lib` should be very straightfward to
install except for SDL. SDL might be a bit hairy, but if you already have
a development installation of SDL (that is, you can build SDL2 programs with
C or C++), that should be enough.
