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

The build a minimal version of `mtots`, you should be able to simply compile all
C files in `src` with any C89 compliant C compiler. For example, with `gcc`, you can
simply run `gcc src/*.c` and `a.out` should be the interpreter.

-------

However to run `mtots` with all optimizations and features enabled,
run the following command from the root of this repository:

```
python3 make.py --verbose --release --test
```

If the command finishes successfully, it should have produced the `mtots` binary
at the root of this repository.

To install the binary you just built and tested, you just need to add this repository
to your `PATH`.

To do so,

* on **MacOS**, run
  ```
  echo "export PATH=\"\$PATH:$PWD\"" >> ~/.zshrc
  ```
* on **Linux**, run
  ```
  echo "export PATH=\"\$PATH:$PWD\"" >> ~/.bashrc
  ```
* on **Windows**, if you are not sure how to add a directory to the `PATH`, see
https://learn.microsoft.com/en-us/previous-versions/office/developer/sharepoint-2010/ee537574(v=office.14)
