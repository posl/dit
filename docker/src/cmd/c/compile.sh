#!/bin/sh

# pwd must be in the same directory as c-files
for i in ./*.c; do
    if [ -r $i ]; then
        gcc $i -o exe/${i%.c} -march=native -O2
    fi
done