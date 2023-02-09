#!/bin/sh -ex


#
# Unit Tests
#

dit config --unit-tests



#
# Test Functions
#

#
# Usages:
#   do_test d=<mode_repr> h=<mode_repr> <stat>
# Check the correctness of what 'dit config' displays and what the config-file contains.
#
# Variables:
#   <mode_repr>    string representing each mode
#   <stat>         unsigned integer between 0 and 24, stored in the config-file
#
do_test(){
    set +x

    if OUTPUT="$( dit config )"; then
        echo
    else
        read -r REPLY
    fi

    for line in ${OUTPUT}
    do
        if [ "${line}" = "$1" ]; then
            shift
        else
            break
        fi
    done

    if [ "$#" -ne 1 ]; then
        { echo 'error: the display contents may be incorrect:'; echo "${OUTPUT}"; } 1>&2
        exit 1
    fi

    OUTPUT="$( od -An -tuC /dit/var/config.stat | awk '{ print $1 }' )"

    if [ "${OUTPUT}" != "$1" ]; then
        echo "error: the contents of the config-file may be incorrect: ${OUTPUT}" 1>&2
        exit 1
    fi

    set -x
}



#
# Integration Tests
#

do_test 'd=normal' 'h=normal' 12

dit config _4
do_test 'd=normal' 'h=no-ignore' 14

dit config 3,h=no-ref
do_test 'd=simple' 'h=no-reflect' 15

dit config b=str,d=no-ignore
do_test 'd=no-ignore' 'h=strict' 21

dit config --reset d=0
do_test 'd=no-reflect' 'h=normal' 2

dit config --help | head -n2 | grep -F '  dit config [OPTION]...'
echo



#
# Visual Tests
#

# failure cases

: 'error: mode integer must be between 0 and 4.'
dit config 5 && exit 1
read -r REPLY

: 'error: target file must be represented by one of "bdh".'
dit config a=3 && exit 1
read -r REPLY

: 'error: mode string must be uniquely interpretable.'
dit config -r d=no && exit 1
read -r REPLY

: 'error: multiple mode specifications must be separated by commas.'
dit config d=no-ig h=1 && exit 1
read -r REPLY

: 'error: if the internal config-file is empty, it should be reset.' > /dit/var/config.stat
do_test 'd=normal' 'h=normal' 12