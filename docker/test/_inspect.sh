#!/bin/sh


#
# Unit Tests
#

dit inspect --unit-tests || exit 1



#
# Variables
#

set +x

trap 'rm -fR _inspect[1-3].tmp; echo' EXIT

TMP1=_inspect1.tmp
TMP2=_inspect2.tmp


DIR=_inspect3.tmp

mkdir -m g+s "${DIR}"
chown root:sys "${DIR}"

cd "${DIR}" || exit 1

dd if=/dev/random of=large.rand bs=1M count=1
chmod a=w,u+r,u+s large.rand

mkdir -m a=rx,u+w,o+t empty
mknod -m a=rw zero.dev c 1 5
mkfifo -m a=,g=rw .pipe
ln -s /usr/local/bin/dit dit.link

cd ..


echo
set -x



#
# Functions
#

#
# Usages:
#   do_test <target_option>
# Check consistency between 'dit inspect' and 'ls -Al' output.
#
# Variables:
#   <target_option>    the option you want to test
#
do_test(){
    set +x

    if [ "$#" -eq 1 ]; then
        ( dit inspect "$1" "${DIR}" || exit 1 ) | awk '/^.* \|-- / { print $1, $2, $3, $NF }' > "${TMP1}"
        ( ls -Al "$1" "${DIR}" || exit 1 ) | awk 'NR != 1 { print $1, $3, $4, $NF }' | tee "${TMP2}"

        diff -u "${TMP1}" "${TMP2}" || exit 1
    fi

    echo
    set -x
}



#
# Integration Tests
#

do_test -F
do_test -n
do_test -S
do_test -X

( dit inspect --help || exit 1 ) | head -n2 | grep -F '  dit inspect [OPTION]...' || exit 1
echo



#
# Visual Tests
#

# successful cases

: 'check the correctness of file information and file name coloring.'
ls -Al --color "${DIR}"
dit inspect -C || exit 1
read -r REPLY

: 'make sure the correct file information is displayed.'
dit inspect -C /dit /dev || exit 1
read -r REPLY


# failure cases

: 'warning: a non-existing file should not be specified.'
rm -f "${TMP1}"
dit inspect "${TMP1}" || exit 1
read -r REPLY

: 'warning: an invalid symbolic link should not be specified.'
ln -s "${TMP1}" "${TMP1}"
dit inspect "${TMP1}" || exit 1
read -r REPLY


: 'error: sorting style must be selected from the following 3 types.'
dit inspect --sort none && exit 1
read -r REPLY