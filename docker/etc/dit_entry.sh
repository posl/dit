#!/bin/sh


#
# check the directory shared with the host environment
#

if [ ! -e /dit/mnt ]; then
    echo 'dit: the directory to be bound is not specified' 1>&2
    exit 1
fi


if [ -s /dit/mnt/Dockerfile.draft ]; then
    PREV_FROM="$( head -n1 /dit/mnt/Dockerfile.draft )"
    CURR_FROM="$( head -n1 /dit/etc/Dockerfile.base )"

    if [ "${PREV_FROM}" != "${CURR_FROM}" ]; then
        echo "dit: base-image inconsistency with the contents of 'Dockerfile.draft'" 1>&2
        exit 1
    fi
fi

touch \
    /dit/mnt/.dit_history \
    /dit/mnt/.dockerignore \
    /dit/mnt/Dockerfile \
    /dit/mnt/Dockerfile.draft



#
# set the permissions on several files specific to this tool
#

chmod a=x /usr/local/bin/dit

find /dit/etc -type f -exec chmod a=r {} +

chmod a=rw \
    /dit/mnt/.dit_history \
    /dit/mnt/.dockerignore \
    /dit/mnt/Dockerfile \
    /dit/mnt/Dockerfile.draft


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
    /dit/var/cmd.log \
    /dit/var/config.stat \
    /dit/var/erase.log.dock \
    /dit/var/erase.log.hist \
    /dit/var/ignore.json.dock \
    /dit/var/ignore.json.hist \
    /dit/var/optimize.json

umask "${DEFAULT_UMASK_VALUE}"


chmod a=rx \
    /dit \
    /dit/etc \
    /dit/tmp \
    /dit/var



#
# initialize the internal files, and enter a shell as the default user
#

echo '0' > /dit/tmp/last-exit-status
echo '-1' > /dit/tmp/last-history-number

dit config -r
dit ignore -dhr
dit optimize -r
dit reflect


DEFAULT_USER="$( head -n1 /dit/etc/default_user )"
rm -f /dit/etc/default_user

export ENV=/dit/etc/dit_profile.sh


if [ "${DEFAULT_USER}" != 'root' ]; then
    exec su-exec "${DEFAULT_USER}" "$@"
else
    exec "$@"
fi