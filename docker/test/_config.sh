#!/bin/sh


#
# Unit Tests
#

dit config --unit-tests || exit 1



#
# Functions
#

#
# Usages:
#   do_test d=<mode_repr> h=<mode_repr> <stat>
# Check the correctness of what 'dit config' displays and what config-file contains.
#
# Variables:
#   <mode_repr>    string representing each mode
#   <stat>         unsigned integer between 0 and 24, stored in config-file
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
            exit 1
        fi
    done

    OUTPUT="$( od -An -tuC /dit/var/config.stat | awk '{ print $1 }' )"

    if [ "${OUTPUT}" != "$1" ]; then
        exit 1
    fi

    set -x
}



#
# Integration Tests
#

do_test 'd=normal' 'h=normal' 12

dit config _4 || exit 1
do_test 'd=normal' 'h=no-ignore' 14

dit config 3,h=no-ref || exit 1
do_test 'd=simple' 'h=no-reflect' 15

dit config b=str,d=no-ignore || exit 1
do_test 'd=no-ignore' 'h=strict' 21

dit config --reset d=0 || exit 1
do_test 'd=no-reflect' 'h=normal' 2

( dit config --help || exit 1 ) | head -n2 | grep -F '  dit config [OPTION]...' || exit 1
echo



#
# Visual Tests
#

# failure cases

: 'mode integer must be between 0 and 4.'
dit config 5 && exit 1
read -r REPLY

: '>  target file must be represented by one of "bdh".'
dit config a=3 && exit 1
read -r REPLY

: 'mode string must be uniquely interpretable.'
dit config -r d=no && exit 1
read -r REPLY

: 'multiple mode specifications must be separated by commas.'
dit config d=no-ig h=1 && exit 1
read -r REPLY

: 'intentionally make an unexpected error.' > /dit/var/config.stat
do_test 'd=normal' 'h=normal' 12