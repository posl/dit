#!/bin/bash


#
# initialize files in shared directory
#

if [ ! -e /dit/share ]; then
    echo "dit: please check if you created or specified the directory to be bound." 1>&2
    exit 1
fi


if [ ! -e /dit/share/Dockerfile ]; then
    touch /dit/share/Dockerfile
fi

if [ ! -s /dit/share/Dockerfile.draft ]; then
    cat <<EOF > /dit/share/Dockerfile.draft
FROM ${BASE_IMAGE}:${BASE_VERSION}
SHELL [ "${SHELL:-/bin/sh}", "-c" ]
WORKDIR $(pwd)
EOF
fi

unset BASE_IMAGE
unset BASE_VERSION


if [ ! -s /dit/share/.dockerignore ]; then
    echo ".cmd_history" > /dit/share/.dockerignore
fi

if [ ! -e /dit/share/.cmd_history ]; then
    touch /dit/share/.cmd_history
fi



#
# set the permissions on several files specific to this tool
#

chmod a=x /usr/local/bin/dit

chmod a=rw \
    /dit/share/Dockerfile \
    /dit/share/Dockerfile.draft \
    /dit/share/.dockerignore \
    /dit/share/.cmd_history

find /dit/script -type f -exec chmod a=r {} +


DEFAULT_UMASK_VALUE="$( umask )"
umask 0000

if [ ! -e /dit/conf ]; then
    mkdir /dit/conf
fi

if [ ! -e /dit/tmp ]; then
    mkdir /dit/tmp
fi

touch \
    /dit/conf/cmd-ignore.dock \
    /dit/conf/cmd-ignore.hist \
    /dit/conf/current-spec.conv \
    /dit/conf/current-spec.opt \
    \
    /dit/tmp/change-log.dock \
    /dit/tmp/change-log.hist \
    /dit/tmp/change-log.onb \
    /dit/tmp/change-log.setc \
    /dit/tmp/change-report.act \
    /dit/tmp/change-report.prov \
    \
    /dit/tmp/current-status.conv \
    /dit/tmp/current-status.dit \
    /dit/tmp/current-status.opt \
    \
    /dit/tmp/last-history-number \
    /dit/tmp/last-exit-status \
    /dit/tmp/last-command-line \
    /dit/tmp/last-parse-result \
    /dit/tmp/last-convert-result

umask "${DEFAULT_UMASK_VALUE}"


chmod a=rx \
    /dit \
    /dit/script \
    /dit/conf \
    /dit/tmp



#
# perform other necessary initializations, and launch a login shell
#

dit config -r
dit ignore -r
dit erase -r both

cp -f /dit/script/dit_profile.sh /etc/profile.d/

exec /bin/bash --login