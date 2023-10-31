# mtots VSCode Extension

VSCode support for the mtots programming language

As of version `0.0.17`, the following features are implemented:

* goto-definition
* hover - show type or function signature and comment,
* tab-complete for variable, function, field and method names
* signature help provider

The type solver also supports:

* Generic functions

Future work:

* Generic classes

## Release Notes

### 0.0.52

More correct Union - Union assignability checks
Bugfix with trait assignability checks

### 0.0.51

nil-coalesce operator

types are now checked when values are assigned to variables or fields.

### 0.0.50

nil-check syntax

### 0.0.49

Allow assigning to final fields inside `__init__` methods.

### 0.0.48

Support for `Union` types.
Misc type inference improvements.

### 0.0.47

Support for keyword arguments

### 0.0.45

"Mtots: Run Mtots File" command to run an open file

F5 shortcut to run active Mtots file

### 0.0.44

Fix member completion when in middle of using a decorator

### 0.0.43

Hovering function calls will now show the actual value of default parameters
instead of just `*`.

### 0.0.42

Misc bugfixes

### 0.0.40

Root path is determined relative to the mtots executable if one is found.

Fix minor race condition bug

### 0.0.39

Better goto-definition for module variables

### 0.0.38

Bugfix when resolving usage of qualified parameter types

Updated logic for finding modules (in particular, `__init__` modules are now supported).

### 0.0.37

Hover to get information about a field over the definition of the field
(methods already could do this, but fields have been overlooked up to this
point)

`isAssignableTo` now takes into account inheritance

### 0.0.35

Untyped lists are shown as `List` instead of `List[Any]`

`LiteralType.isAssignableTo()` bugfix

Fixes hanging and out of memory issue due to an infinite loop in the parser.

Drop support for running this extension in the browser.
Might be brought back at some point in the future, but it's quite a bit
less convenient to debug, and I don't really have a use for this at the
moment.

### 0.0.34

Support for decorator syntax

constexpressions can include constexpr variables from imported modules.

Another patch for the OOM issues with files that do not end in a newline.
It seems this issue is still in the rewrite.

### 0.0.33

Large rewrite of the extension!

The parser and lexer are resued, but everything else, including the way the
extension comes together, and the type solver have all been rewritten.

### 0.0.32

Short term fix for the hanging and crashing missing newline bug.

... But the issue still seems to persist.... Address this later at some point.

### 0.0.31

Web compatible - i.e. this extension should now be installable from vscode.dev

Parser errors are no longer fatal

NOTE: There is a performance bug that happens whenever any `mtots` file does not
end in a newline and causes a crash. Right now I have no idea why this is the
case, and I don't quite have the bandwidth to explore this right now.

### 0.0.29

Names that start with `_` will appear after names that do not start with `_`.

`List.__add__()`, `List.extend()`, `List.reverse()` methods

`List()`, `Dict()`, `Dict.fromPairs()`

`Tuple`

Hover will now distinguish between whether the symbol usage appears in
a value context or a type context.

`FrozenList`.

New style `Tuple`s.

`String.upper()` and `String.lower()` methods.

### 0.0.31

`String.startsWith` and `String.endsWith` methods.

`String.__iter__()`

### 0.0.30

Lambda expression syntax

### 0.0.29

...


### 0.0.28

Support for trait fields

Fix autocomplete bug

Support for the ternary conditional operator (i.e. `if-then` expressions).

trait interface methods and fields bugfix

### 0.0.27

Fix placement of docs for variable (comes after type, rather than before)

`List.clear()` method

`**`/`__pow__` operator support

`__slice__` syntax support

Syntax highlighting for binary number literals

Variable documentation being placed above the declaration is no longer supported.
Variable documentation must now come after the name (or type annotation if present) and
before the `=` symbol.

`Number.toU32`

### 0.0.26

Fix a bug in the type solver related to the `Iteration` type

### 0.0.25

Bitwise operators for numbers

Iterable based on whether `__iter__` method is implemented for custom types

Better hover comment for constructor calls

### 0.0.24

Support for adding documentation to fields.

Support for adding documentation to variables after the name.

### 0.0.23

`List.flatten`

`StringBuilder`

### 0.0.22

Support for `from .. import` syntax.

Better goto-definition for imported modules and module members.

### 0.0.21

`~/git/mtots-apps` is now the default for the root
if `MTOTS_ROOT` is not explicitly provided

### 0.0.20

Minor bugfix where autocomplete wouldn't work when nested inside a function
call with mismatched argument count

### 0.0.19

`~/git/mtots/apps` is now the default for the lib root
if `MTOTS_LIB_ROOT` is not explicitly provided.

### 0.0.18

XXX A whole lot of things since 0.0.6 XXX

### 0.0.6

raise syntax and documentation for variables

### 0.0.5

Bugfix for when modules cannot be found.

### 0.0.4

Actual smart language features!
Diagnostics, goto-definition, auto-completes based on variables in scope and
members available for a given value; parameter hints.

Likely still lots of bugs that need to be squashed. Will address them
as I use this extension and find them.

### 0.0.3

`try` and `raise` are valid keywords

### 0.0.2

Properly color line comments

### 0.0.1

An extension is reborn~
