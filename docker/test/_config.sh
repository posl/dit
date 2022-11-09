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
    output="$( dit config )"

    for line in ${output}
    do
        if [ "${line}" = "$1" ]; then
            shift
        else
            exit 1
        fi
    done

    output="$( od -A n -t uC /dit/var/config.stat | sed 's/^[ \t]*//' )"

    if [ "${output}" != "$1" ]; then
        exit 1
    fi

    echo
    set -x
}



#
# Integration Tests
#

# successful cases

do_test 'd=normal' 'h=normal' 12

dit config _4 || exit 1
do_test 'd=normal' 'h=no-ignore' 14

dit config 3,h=no-ref || exit 1
do_test 'd=simple' 'h=no-reflect' 15

dit config b=str,d=no-ignore || exit 1
do_test 'd=no-ignore' 'h=strict' 21

dit config --reset d=0 || exit 1
do_test 'd=no-reflect' 'h=normal' 2


# failure cases

dit config 5 && exit 1
read -r REPLY

dit config a=3 && exit 1
read -r REPLY

dit config -r d=no && exit 1
read -r REPLY

dit config d=no-ig h=1 && exit 1
read -r REPLY

: > /dit/var/config.stat
dit config
read -r REPLY



#
# Visual Tests
#

dit config --help | head -n2
read -r REPLY