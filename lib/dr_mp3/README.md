
dr_mp3 header downloaded from
https://github.com/mackron/dr_libs
from commit dd762b861ecadf5ddd5fb03e9ca1db6707b54fbb

Modified dr_mp3.h to prefix public API functions with `mtots_`
This is so the extern functions don't conflict with the ones
included in `SDL_mixer`.
