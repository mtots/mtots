# Script to test that the C sources will compile with C++

rm -rf src2
cp -R src src2
for fn in src2/*.c; do mv $fn ${fn//.c/.cc}; done

g++ -Wall -Werror -Wpedantic -Weverything \
    -Wno-poison-system-directories -Wno-undef -Wno-unused-parameter \
    -Wno-padded -Wno-cast-align -Wno-float-equal -Wno-missing-field-initializers \
    -Wno-switch-enum \
    -std=c++98 -lm -g -fsanitize=address \
    -Wno-old-style-cast \
    src2/*.cc -o mtots

rm -rf src2
