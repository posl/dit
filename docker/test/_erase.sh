#!/bin/sh -eux


#
# Unit Tests
#

dit erase --unit-tests



#
# Preprocessing
#

set +x

end_processing(){
    if [ -n "${DIR}" ]; then
        mv -f "${DIR}"/.dit_history "${DIR}"/Dockerfile.draft /dit/mnt
        mv -f "${DIR}"/erase-result.* "${DIR}"/reflect-report.* /dit/srv
        mv -f "${DIR}"/erase.log.* /dit/var
    fi

    rm -fR _erase[1-4].tmp
    echo
}

trap 'end_processing' EXIT
trap 'exit 1' HUP INT QUIT TERM


TMP1=_erase1.tmp
TMP2=_erase2.tmp
TMP3=_erase3.tmp

TARGET=_erase4.tmp


mkdir "${TARGET}"

cp -fp \
    /dit/mnt/.dit_history \
    /dit/mnt/Dockerfile.draft \
    /dit/srv/erase-result.* \
    /dit/srv/reflect-report.* \
    /dit/var/erase.log.* \
    "${TARGET}"

DIR="${TARGET}"

set -x



#
# Test Functions
#

#
# Usages:
#   init_target_file <target_c>
# Initialize the target file and the temporary file with the same content for testing.
#
# Variables:
#   <target_c>    character representing the files to be edited ('d' or 'h')
#
init_target_file(){
    set +x

    if [ "$1" = 'd' ]; then
        TARGET=/dit/mnt/Dockerfile.draft
    else
        TARGET=/dit/mnt/.dit_history
    fi

    cat <<EOF | tee "${TARGET}" > "${TMP1}"
#include "main.h"



#define LOG_FILE_D "/dit/var/erase.log.dock"
#define LOG_FILE_H "/dit/var/erase.log.hist"

#define ERASE_OPTID_NUMBERS 1
#define ERASE_OPTID_UNDOES 2
#define ERASE_OPTID_MAX_COUNT 4


#define if_necessary_assign_exit_status(tmp, exit_status)
    if ((tmp) && (exit_status != (UNEXPECTED_ERROR + ERROR_EXIT)))
        exit_status = ((tmp) + exit_status) ? (tmp) : (UNEXPECTED_ERROR + ERROR_EXIT)


#define getsize_check_list(i)  ((((i) - 1) >> 5) + 1)
#define getidx_check_list(i)  ((i) >> 5)
#define getmask_check_list(i)  (1 << ((i) & 0b11111))

#define setbit_check_list(list, i)  (list[getidx_check_list(i)] |= getmask_check_list(i))
#define getbit_check_list(list, i)  (list[getidx_check_list(i)] & getmask_check_list(i))
#define invbit_check_list(list, i)  (list[getidx_check_list(i)] ^= getmask_check_list(i))


#define ERASE_CONFIRMATION_MAX 8
EOF

    : > /dit/srv/reflect-report.prov
    dit erase "-$1r" 2>&1 | grep -F '' && exit 1

    if [ "$1" != 'd' ]; then
        {
            echo
            echo '# start'
            echo '# dit erase processing'
        } | tee -a "${TMP1}" | dit reflect -hp -

        : > /dit/srv/reflect-report.real
        dit reflect

        {
            echo '# error handling'
            echo '# finish'
        } | tee -a "${TMP1}" | dit reflect -hp -
    fi

    set -x
}


#
# Usages:
#   do_test <sed_addr> <target_c>
# Check the correctness of 'dit erase' by using the sed command.
#
# Variables:
#   <sed_addr>    address to be embedded in the sed command script
#   <target_c>    character representing the files to be edited ('d' or 'h')
#
# Remarks:
#   - Before calling this function, you must call 'init_target_file' and 'dit erase' appropriately.
#   - If you call this function multiple times while specifying <target_c>, you must specify 'd', then 'h'.
#
do_test(){
    sed -E "$1d" "${TMP1}" > "${TMP2}"
    diff -u "${TARGET}" "${TMP2}"

    if [ "$#" -gt 1 ]; then
        dit erase "-$2" --assume=Quit 2>&1 | grep -F '' && exit 1
        diff -u "${TARGET}" "${TMP2}"

        if [ "$2" = 'd' ]; then
            echo ' < dockerfile >' > "${TMP3}"
        else
            echo ' < history-file >' >> "${TMP3}"
        fi

        sed -En "$1p" "${TMP1}" | tee "${TMP2}" >> "${TMP3}"
        dit erase "-$2v" > "${TMP1}"
        diff -u "${TMP1}" "${TMP2}"

        if [ "$2" = 'd' ]; then
            echo >> "${TMP3}"
        else
            dit erase -dhv > "${TMP2}"
            diff -u "${TMP2}" "${TMP3}"
            cat "${TMP2}"
        fi
    fi
}


#
# Usages:
#   check_log_file <target_ext> <logs_array>
# Check the correctness of the contents of the log-file.
#
# Variables:
#   <target_ext>    extension of the target log-file
#   <logs_array>    sequence of unsigned char type integers representing the array of the log-data
#
check_log_file(){
    : > /dit/srv/reflect-report.real
    dit reflect

    OUTPUT="$( od -An -tuC /dit/var/erase.log."$1" | awk "{ for (i = $# - 1; i--;) print \$(NF - i) }" )"
    shift

    for i in ${OUTPUT}
    do
        if [ "${i}" = "$1" ]; then
            shift
        else
            break
        fi
    done

    if [ "$#" -ne 0 ]; then
        { echo "error: the contents of the log-file may be incorrect:"; echo "${OUTPUT}"; } 1>&2
        exit 1
    fi
}



do_test_regexp(){
    set +x

    init_target_file d
    dit erase -dy -E 'check_list'
    do_test '/check_list/' d
    check_log_file dock 21

    init_target_file h
    dit erase -hiy -E 'er'
    do_test '/[eE][rR]/' h
    check_log_file hist 19 2 1

    echo
    set -x
}


do_test_line_numbers(){
    set +x

    init_target_file d
    dit erase -dy -N '18-20,23'
    do_test '/#define get/' d
    check_log_file dock 23

    init_target_file h
    dit erase -hy -m5 -N '-20'
    do_test '/\(\(/' h
    check_log_file hist 22 3 2

    echo
    set -x
}


do_test_undoes(){
    set +x

    init_target_file d
    dit erase -dy -Z4
    do_test '/./' d
    check_log_file dock 11

    init_target_file h
    dit erase -hy
    do_test '29,30' h
    check_log_file hist 27 1 2

    echo
    set -x
}


do_test_other_options(){
    set +x

    for target_c in "$@"
    do
        init_target_file "${target_c}"
        dit erase "-${target_c}" -m0
        do_test '/0^/'

        dit erase "-${target_c}" -m0 --blank=truncate
        do_test '/^$/'

        if [ "${target_c}" = 'd' ]; then
            check_log_file dock 16
        else
            check_log_file hist 16 2 2
        fi

        echo "Done!"
    done

    echo
    set -x
}



#
# Integration Tests
#

do_test_regexp
do_test_line_numbers
do_test_undoes
do_test_other_options d h

dit erase --help | head -n2 | grep -F '  dit erase [OPTION]...'
echo



#
# Visual Tests
#

# successful cases

: 'check if empty lines in target files are squeezed without confirmation.'
init_target_file d
dit erase -dsv
cat "${TARGET}"
read -r REPLY

: 'check if the confirmation before deletion is performed consistently.'
init_target_file h
dit erase -hv -E '=' -N '18-'
read -r REPLY


init_target_file d
init_target_file h
echo

: 'check if the confirmation before deletion skipped, and you can select the lines to delete.'
dit erase -dhv --as N -Z2 -E 'it'
read -r REPLY


# failure cases

init_target_file d
init_target_file h
echo

: 'warning: the internal log-files should not be deleted. (no error message)'
rm -f /dit/var/erase.log.*
dit erase -dh
read -r REPLY

: 'warning: the target files should not be updated without using the dit command. (with error message)'
echo 'ARG abc=123' > /dit/mnt/Dockerfile.draft
echo 'abc=123' > /dit/mnt/.dit_history
dit erase -dh
read -r REPLY


: 'error: an invalid regular expression pattern must not be specified.'
dit erase -dh -E '*.txt' && exit 1
read -r REPLY

: 'error: multiple specifications of line numbers must be separated by commas.'
dit erase -dh -N '1 4 5' && exit 1
read -r REPLY

: 'error: a negative number cannot be specified for the undo count.'
dit erase -dh -Z-1 && exit 1
read -r REPLY

: 'error: a negative number cannot be specified for the maximum number of lines that can be deleted.'
dit erase -dh -m -5 && exit 1
read -r REPLY

: 'error: the target files must be specified explicitly.'
dit erase && exit 1
read -r REPLY


: 'error: the answer to the confirmation before deletion must be set from the following 3 ones.'
dit erase -dh --assume EXIT && exit 1
read -r REPLY

: 'error: how to handle empty lines must be selected from the following 3 types.'
dit erase -dh --blank=ignore && exit 1
read -r REPLY

: 'error: the target files must be specified by lower case characters, as follow.'
dit erase -dh --targ Both && exit 1
read -r REPLY


: 'error: this command takes no non-optional arguments.'
dit erase -dh regexp.txt && exit 1
read -r REPLY

: 'error: must not execute this command if target files have been deleted.'
rm -f /dit/mnt/Dockerfile.draft /dit/mnt/.dit_history
dit erase -dh && exit 1
read -r REPLY