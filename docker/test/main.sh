#!/bin/bash -ex


#
# run test codes for each dit command
#

if dit test; then
    set +x
    shopt -s nullglob

    for script in _*.sh
    do
        echo
        sh -ex "${script}"
    done
else
    exit 1
fi