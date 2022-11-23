#!/bin/sh -eux


#
# run test codes for each dit command
#

if dit test; then
    set +x

    for script in _*.sh
    do
        if [ "${script}" != '_*.sh' ]; then
            echo
            sh -eux "${script}"
        fi
    done
else
    exit 1
fi