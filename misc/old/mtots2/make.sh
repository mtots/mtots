

gcc -Wall -Werror -Wpedantic -Weverything \
    -Wno-poison-system-directories -Wno-undef -Wno-unused-parameter \
    -Wno-padded -Wno-cast-align -Wno-float-equal -Wno-missing-field-initializers \
    -Wno-switch-enum \
    -std=c89 -lm -g -fsanitize=address \
    -DMTOTS_DEBUG_MEMORY_LEAK=1 \
    src/*.c -o mtots
