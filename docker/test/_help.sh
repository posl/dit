#!/bin/sh


#
# Unit Tests
#

dit help --unit-tests || exit 1



#
# Variables
#

set +x

OUTPUT=''
CMDS_NUM=13

TMP1=_help1.tmp
TMP2=_help2.tmp

trap ': > /dit/etc/dit_version; rm -f _help[1-2].tmp; echo' EXIT

set -x



#
# Functions
#

do_test_cmd_list(){
    set +x

    OUTPUT="$( dit help -a )" || exit 1
    : > "${TMP1}"

    for cmd in ${OUTPUT}
    do
        CMDS_NUM="$(( CMDS_NUM - 1 ))"
        ( dit help || exit 1 ) | grep -En "^  ${cmd}    " >> "${TMP1}" || exit 1
    done

    [ "${CMDS_NUM}" -eq 0 ] || exit 1

    sort -nu "${TMP1}" > "${TMP2}"
    diff -u "${TMP1}" "${TMP2}" || exit 1

    echo
    set -x
}


do_test_manual(){
    set +x

    for cmd in ${OUTPUT}
    do
        ( dit help --man "${cmd}" || exit 1 ) | head -n2 | grep -F "  dit ${cmd} [OPTION]..." || exit 1
    done

    echo
    set -x
}


do_test_example(){
    set +x

    for cmd in ${OUTPUT}
    do
        ( dit help -e "${cmd}" || exit 1) | tee "${TMP1}" | grep -F " ${cmd} " > "${TMP2}" || exit 1
        diff -u "${TMP1}" "${TMP2}" || exit 1
    done

    echo
    set -x
}



#
# Integration Tests
#

do_test_cmd_list
do_test_manual
do_test_example

( dit help --help || exit 1 ) | head -n2 | grep -F '  dit help [OPTION]...' || exit 1
echo



#
# Visual Tests
#

# successful cases

: 'show usage examples of commands that should be learned.'
dit help -e || exit 1
read -r REPLY

: 'show a short description of the interface of all dit commands.'
dit help -d || exit 1
read -r REPLY


: 'when specifying one command'
dit help --d inspect || exit 1
read -r REPLY

: 'when specifying multiple abbreviated commands'
dit help --desc conv optim refl erase || exit 1
read -r REPLY

: 'when specifying multiple commands with aliases'
dit help --description cfg hc || exit 1
read -r REPLY


: 'display the contents of the version-file.'
echo 'dit version a.b.c' > /dit/etc/dit_version
dit help -V || exit 1
read -r REPLY


# failure cases

: 'when specifying an invalid command'
dit help -m commit && exit 1
read -r REPLY

: 'when specifying an ambiguous command'
dit help --exam he && exit 1
read -r REPLY

: 'intentionally make an unexpected error.'
rm -f /dit/etc/dit_version
dit help -V && exit 1
read -r REPLY