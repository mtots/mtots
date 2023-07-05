stb_image library together with an adapter for Mtots.

I didn't want to expose all of `stb_image.h` to the rest of Mtots.

So I also include an adapter for Mtots.

`stb_image.h` was taken from commit `5736b15f7ea0ffb08dd38af21067c314d6a3aae9`
of https://github.com/nothings/stb

`mtotsa_stbimage.h` and `mtotsa_stbimage.c` are files that use `stb_image.h`
to export symbols for use in Mtots.
(mtotsa is short for "Mtots Adapter")
