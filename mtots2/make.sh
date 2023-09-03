

gcc -Wall -Werror -Wpedantic -Weverything \
    -Wno-poison-system-directories -Wno-undef -Wno-unused-parameter \
    -Wno-padded -Wno-cast-align -Wno-float-equal -Wno-missing-field-initializers \
    -std=c89 -lm -g -fsanitize=address \
    src/*.c -o mtots
