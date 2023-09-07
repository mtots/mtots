

g++ -Wall -Werror -Wpedantic -Weverything \
    -Wno-poison-system-directories -Wno-undef -Wno-unused-parameter \
    -Wno-padded -Wno-cast-align -Wno-float-equal -Wno-missing-field-initializers \
    -Wno-switch-enum -Wno-c++98-compat \
    -std=c++20 -lm -g -fsanitize=address \
    src/*.cc -o mtots
