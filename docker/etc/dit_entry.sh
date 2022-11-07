#!/bin/sh


#
# check the shared directory, and initialize the base-file for Dockerfile
#

if [ ! -e /dit/mnt ]; then
    echo 'dit: the directory to be bound is not specified' 1>&2
    exit 1
fi

touch \
    /dit/mnt/.dit_history \
    /dit/mnt/.dockerignore \
    /dit/mnt/Dockerfile \
    /dit/mnt/Dockerfile.draft


cat <<EOF > /dit/etc/Dockerfile.base
FROM ${BASE_NAME}:${BASE_VERSION}
SHELL [ "${SHELL:-/bin/sh}", "-c" ]
WORKDIR $( pwd )
EOF

unset BASE_NAME BASE_VERSION



#
# set the permissions on several files specific to this tool
#

chmod a=x /usr/local/bin/dit

find /dit/etc -type f -exec chmod a=r {} +
find /dit/mnt -type f -exec chmod a=rw {} +


DEFAULT_UMASK_VALUE="$( umask )"
umask 0000

if [ ! -e /dit/tmp ]; then
    mkdir /dit/tmp
fi

if [ ! -e /dit/var ]; then
    mkdir /dit/var
fi

touch \
    /dit/etc/dit_version \
    \
    /dit/tmp/convert-result.dock \
    /dit/tmp/convert-result.hist \
    /dit/tmp/erase-result.dock \
    /dit/tmp/erase-result.hist \
    /dit/tmp/last-command-line \
    /dit/tmp/last-exit-status \
    /dit/tmp/last-history-number \
    /dit/tmp/reflect-report.prov \
    /dit/tmp/reflect-report.real \
    \
    /dit/var/config.stat \
    /dit/var/erase.log.dock \
    /dit/var/erase.log.hist \
    /dit/var/ignore.list.dock \
    /dit/var/ignore.list.hist \
    /dit/var/optimize.stat \
    /dit/var/setcmd.log

umask "${DEFAULT_UMASK_VALUE}"


chmod a=rx \
    /dit \
    /dit/etc \
    /dit/tmp \
    /dit/var



#
# initialize or delete the internal files depending on usage, and enter a login shell
#

echo '0' > /dit/tmp/last-exit-status
echo '-1' > /dit/tmp/last-history-number

dit config -r
dit ignore -dhr
dit optimize -r
dit reflect


DEFAULT_USER="$( head -n1 /dit/etc/default_user )"

cp -f /dit/etc/dit_profile.sh /etc/profile.d/

rm -f \
    /dit/etc/default_user \
    /dit/etc/dit_*.sh \


if [ "${DEFAULT_USER}" != 'root' ]; then
    su-exec "${DEFAULT_USER}" "$@"
else
    exec "$@"
fi