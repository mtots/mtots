
Bits of the `freetype2` source for use with mtots.

`freetype-2.13.0.tar.gz` was downloaded from

`https://download.savannah.gnu.org/releases/freetype/freetype-2.13.0.tar.gz`

From this website:

`https://download.savannah.gnu.org/releases/freetype/`

Linked by this website:

`https://freetype.org/download.html`

========

Steps I took:

* I downloaded the sources from the link above,
* I copied over the `include` and `src` directories,
* I modified `ftmodule.h` to exclude modules I didn't want
* I compile each source file listed in `INSTALL.ANY.md` (See `make.py`)
    with `FT2_BUILD_LIBRARY` defined.
* I include the individually compiled object files when I compile `mtots`.

Fortunately I didn't have to modify the sources (aside a bit from `ftmodule.h`)

I had some trouble figuring out how to get it to build at first,
but somehow somewhere I picked up that setting `FT2_BUILD_LIBRARY`
might help things. And it seems to have worked.
