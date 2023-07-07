Compiling even the debug build was starting to take a while,
so I tried to profile

I just made some temporary hacks in libmtots.py to get the json files
with `-ftime-trace`.

It looks like the total compile time adds up to about ~1sec.

This leads me to believe that the majority of the build time
is actually the linking. Unfortunately, `-ftime-trace` doesn't
seem to produce anything when I run clang without the `-c` flag.

Resources:

https://www.snsystems.com/technology/tech-blog/clang-time-trace-feature
chrome://tracing/
