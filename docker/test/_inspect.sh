#!/bin/sh -ex


#
# Unit Tests
#

dit inspect --unit-tests



#
# Preprocessing
#

set +x

trap 'rm -fr _inspect[1-3].tmp; echo' EXIT
trap 'exit 1' HUP INT QUIT TERM


TMP1=_inspect1.tmp
TMP2=_inspect2.tmp

DIR=_inspect3.tmp


mkdir -m g+s "${DIR}"
chown root:sys "${DIR}"

cd "${DIR}"

dd if=/dev/random of=large.rand bs=1M count=1
chmod a=w,u+r,u+s large.rand

mkdir -m a=rx,u+w,o+t empty
mknod -m a=rw zero.dev c 1 5
mknod -m a= fc1.unused b 1 2
mkfifo -m a=,g=rw .pipe
ln -s /usr/local/bin/dit dit.link

cd ..


echo
set -x



#
# Test Functions
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

    dit inspect "$1" "${DIR}" > "${TMP1}"
    awk '/^.* \|-- / { print $1, $2, $3, $NF }' "${TMP1}" | tee "${TMP2}"

    ls -Al "$1" "${DIR}" | awk 'NR != 1 { print $1, $3, $4, $NF }' > "${TMP1}"
    diff -u "${TMP1}" "${TMP2}"

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

dit inspect --help | head -n2 | grep -F '  dit inspect [OPTION]...'
echo



#
# Visual Tests
#

# successful cases

: 'check the correctness of file information and file name coloring.'
ls -Al --color "${DIR}"
dit inspect -C
read -r REPLY

: 'check if the correct file information is displayed.'
dit inspect -C /dit /dev
read -r REPLY


# failure cases

: 'warning: a non-existing file should not be specified.'
rm -f "${TMP1}"
dit inspect "${TMP1}"
read -r REPLY

: 'warning: an invalid symbolic link should not be specified.'
ln -s "${TMP1}" "${TMP1}"
dit inspect "${TMP1}"
read -r REPLY


: 'error: sorting style must be selected from the following 3 types.'
dit inspect --sort none && exit 1
read -r REPLY