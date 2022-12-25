#!/bin/sh -eux


#
# Unit Tests
#

dit help --unit-tests



#
# Preprocessings
#

set +x

end_processing(){
    if [ -n "${TMP3}" ]; then
        mv -f "${TMP3}" /dit/etc/dit_version
    fi

    rm -f _help[1-3].tmp
    echo
}

trap 'end_processing' EXIT
trap 'exit 1' HUP INT QUIT TERM


CMDS_NUM=14

TMP1=_help1.tmp
TMP2=_help2.tmp

TARGET=_help3.tmp


cp -fp /dit/etc/dit_version "${TARGET}"
TMP3="${TARGET}"

set -x



#
# Test Functions
#

do_test_cmd_list(){
    set +x

    TARGET="$( dit help -a )"
    : > "${TMP1}"

    for cmd in ${TARGET}
    do
        CMDS_NUM="$(( CMDS_NUM - 1 ))"

        if ! ( dit help | grep -En "^  ${cmd}    " >> "${TMP1}" ); then
            echo "error: the display contents of 'dit help' may be incorrect: ${cmd}" 1>&2
            exit 1
        fi
    done

    if [ "${CMDS_NUM}" -ne 0 ]; then
        { echo "error: the display contents of 'dit help -a' may be incorrect:"; echo "${TARGET}"; } 1>&2
        exit 1
    fi

    sort -nu "${TMP1}" > "${TMP2}"
    diff -u "${TMP1}" "${TMP2}"

    echo
    set -x
}


do_test_manual(){
    set +x

    for cmd in ${TARGET}
    do
        dit help --man "${cmd}" | head -n2 | grep -F "  dit ${cmd} [OPTION]..."
    done

    echo
    set -x
}


do_test_example(){
    set +x

    for cmd in ${TARGET}
    do
        dit help -e "${cmd}" | tee "${TMP1}" | grep -F " ${cmd} " | tee "${TMP2}"
        diff -u "${TMP1}" "${TMP2}"
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

dit help --help | head -n2 | grep -F '  dit help [OPTION]...'
echo



#
# Visual Tests
#

# successful cases

: 'check if usage examples of 4 commands that should be learned are displayed.'
dit help -e
read -r REPLY

: 'check if a short description of the interface of all dit commands is displayed.'
dit help -d
read -r REPLY


: 'check if a short description of the specified dit command is displayed.'
dit help --d inspect
read -r REPLY

: 'check if short descriptions for some commands specified in abbreviations are displayed.'
dit help --desc conv optim refl erase
read -r REPLY


: 'check the correctness of version information'
echo 'dit version a.b.c' > /dit/etc/dit_version
dit help -V
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