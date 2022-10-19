#!/bin/sh


#
# initialize the files in the shared directory
#

if [ ! -e /dit/mnt ]; then
    echo 'dit: the directory to be bound is not specified' 1>&2
    exit 1
fi


if [ ! -e /dit/mnt/.dit_history ]; then
    touch /dit/mnt/.dit_history
fi

if [ ! -s /dit/mnt/.dockerignore ]; then
    echo '.dit_history' > /dit/mnt/.dockerignore
fi


if [ ! -e /dit/mnt/Dockerfile ]; then
    touch /dit/mnt/Dockerfile
fi

if [ ! -s /dit/mnt/Dockerfile.draft ]; then
    cat <<EOF > /dit/mnt/Dockerfile.draft
FROM ${BASE_NAME}:${BASE_VERSION}
SHELL [ "${SHELL:-/bin/sh}", "-c" ]
WORKDIR $(pwd)
EOF
fi

unset BASE_NAME
unset BASE_VERSION



#
# set the permissions on several files specific to this tool
#

chmod a=x /usr/local/bin/dit

chmod a=rw \
    /dit/mnt/.dit_history \
    /dit/mnt/.dockerignore \
    /dit/mnt/Dockerfile \
    /dit/mnt/Dockerfile.draft

find /dit/etc -type f -exec chmod a=r {} +


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
# perform other necessary initializations, and enter a login shell
#

echo '0' > /dit/tmp/last-exit-status
echo '0' > /dit/tmp/last-history-number

dit config -r
dit ignore -dhr
dit optimize -r
dit reflect

cp -f /dit/etc/dit_profile.sh /etc/profile.d/

exec /bin/bash --login