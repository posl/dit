#!/bin/sh -eux


#
# run test codes for each dit command
#

dit inspect /dit > /tmp/main1.tmp


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

dit inspect /dit > /tmp/main2.tmp
diff -u /tmp/main[1-2].tmp