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

TODO

See `scripts/make-web.py`, `scripts/make-macos.py`. As mentioned above, for a ANSI C89 verison without any extensions,
you can just compile all the `h` and `c` files in `src/`.

# IDE

IDE support is provided primarily through vscode.

The extension is available [here](https://marketplace.visualstudio.com/items?itemName=mtots.mtots)

The source for the extension is in this repository [here](vscode/).
