#!/bin/bash


#
# initialize the files in shared directory
#

if [ ! -e /dit/usr ]; then
    echo "dit: please check if you created or specified the directory to be bound." 1>&2
    exit 1
fi


if [ ! -e /dit/usr/.cmd_history ]; then
    touch /dit/usr/.cmd_history
fi

if [ ! -s /dit/usr/.dockerignore ]; then
    echo ".cmd_history" > /dit/usr/.dockerignore
fi


if [ ! -e /dit/usr/Dockerfile ]; then
    touch /dit/usr/Dockerfile
fi

if [ ! -s /dit/usr/Dockerfile.draft ]; then
    cat <<EOF > /dit/usr/Dockerfile.draft
FROM ${BASE_IMAGE}:${BASE_VERSION}
SHELL [ "${SHELL:-/bin/sh}", "-c" ]
WORKDIR $(pwd)
EOF
fi

unset BASE_IMAGE
unset BASE_VERSION



#
# set the permissions on several files specific to this tool
#

chmod a=x /usr/local/sbin/dit

chmod a=rw \
    /dit/usr/.cmd_history \
    /dit/usr/.dockerignore \
    /dit/usr/Dockerfile \
    /dit/usr/Dockerfile.draft

find /dit/src -type f -exec chmod a=r {} +


DEFAULT_UMASK_VALUE="$( umask )"
umask 0000

if [ ! -e /dit/etc ]; then
    mkdir /dit/etc
fi

if [ ! -e /dit/tmp ]; then
    mkdir /dit/tmp
fi

touch \
    /dit/etc/change-log.dock \
    /dit/etc/change-log.hist \
    /dit/etc/cmd-ignore.dock \
    /dit/etc/cmd-ignore.hist \
    \
    /dit/etc/current-spec.conv \
    /dit/etc/current-spec.opt \
    /dit/etc/current-status.dit \
    /dit/etc/current-status.opt \
    /dit/etc/current-status.refl \
    \
    /dit/tmp/change-report.act \
    /dit/tmp/change-report.prov \
    /dit/tmp/convert-result.dock \
    /dit/tmp/convert-result.hist \
    /dit/tmp/last-command-line \
    /dit/tmp/last-exit-status \
    /dit/tmp/last-history-number

umask "${DEFAULT_UMASK_VALUE}"


chmod a=rx \
    /dit \
    /dit/etc \
    /dit/src \
    /dit/tmp



#
# perform other necessary initializations, and enter a login shell
#

dit config -r
dit ignore -r
dit erase -r both

cp -f /dit/src/dit_profile.sh /etc/profile.d/

exec /bin/bash --login