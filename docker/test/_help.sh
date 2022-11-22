#!/bin/sh


#
# Unit Tests
#

dit help --unit-tests || exit 1



#
# Preprocessings
#

set +x

trap ': > /dit/etc/dit_version; rm -f _help[1-2].tmp; echo' EXIT
trap 'exit 1' HUP INT QUIT TERM


OUTPUT=''
CMDS_NUM=13

TMP1=_help1.tmp
TMP2=_help2.tmp

set -x



#
# Test Functions
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

: 'check if usage examples of 4 commands that should be learned are displayed.'
dit help -e || exit 1
read -r REPLY

: 'check if a short description of the interface of all dit commands is displayed.'
dit help -d || exit 1
read -r REPLY


: 'check if a short description of the specified dit command is displayed.'
dit help --d inspect || exit 1
read -r REPLY

: 'check if short descriptions for some commands specified in abbreviations are displayed.'
dit help --desc conv optim refl erase || exit 1
read -r REPLY

: 'check if short descriptions for some aliased commands are displayed.'
dit help --description cfg hc || exit 1
read -r REPLY


: 'check the correctness of version information'
echo 'dit version a.b.c' > /dit/etc/dit_version
dit help -V || exit 1
read -r REPLY


# failure cases

: 'error: an invalid command must not be specified.'
dit help -m commit && exit 1
read -r REPLY

: 'error: the command must be uniquely interpretable.'
dit help --exam he && exit 1
read -r REPLY

: 'error: the internal version-file must not be handled carelessly.'
rm -f /dit/etc/dit_version
dit help -V && exit 1
read -r REPLY