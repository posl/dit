#!/bin/sh


#
# run test codes for each dit command
#

echo
echo '+ dit test'

if dit test; then
    for script in _*.sh
    do
        if [ "${script}" != '_*.sh' ]; then
            echo
            sh -x "${script}" || exit 1
        fi
    done
else
    exit 1
fi