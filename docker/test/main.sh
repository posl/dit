#!/bin/bash -ex


#
# Test Functions
#

do_test_startup_error(){
    : 'error: a subcommand to be invoked must be specified.'
    dit && exit 1
    read -r REPLY

    : 'error: an invalid command must not be specified.'
    dit push && exit 1
    read -r REPLY
}



#
# run test codes for each dit command
#

if dit test; then
    # a few visual tests
    do_test_startup_error

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