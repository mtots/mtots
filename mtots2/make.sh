

gcc -Wall -Werror -Wpedantic -Weverything \
    -Wno-poison-system-directories -Wno-undef -Wno-unused-parameter \
    -Wno-padded -Wno-cast-align -Wno-float-equal \
    -std=c89 -lm -g -fsanitize=address \
    src/*.c -o mtots
