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

if [ ! -e /dit/share/Dockerfile.draft ]; then
    cat <<EOF > /dit/share/Dockerfile.draft
FROM ${BASE_IMAGE}:${BASE_VERSION}
SHELL [ "${SHELL:-/bin/sh}", "-c" ]
WORKDIR $(pwd)
EOF
fi

unset BASE_IMAGE
unset BASE_VERSION


if [ ! -e /dit/share/.dockerignore ]; then
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
find /dit/conf -type f -exec chmod a=rw {} +


if [ ! -e /dit/tmp ]; then
    mkdir /dit/tmp
fi

DEFAULT_UMASK_VALUE="$( umask )"
umask 0000

touch \
    /dit/tmp/last-history-number \
    /dit/tmp/last-exit-status \
    /dit/tmp/last-command-line \
    /dit/tmp/last-parse-result \
    /dit/tmp/last-convert-result \
    /dit/tmp/change-list.dock \
    /dit/tmp/change-list.hist \
    /dit/tmp/change-report

umask "${DEFAULT_UMASK_VALUE}"



#
# after reproducing the environment as necessary, start a login shell
#

if [ -s /dit/share/.cmd_history ]; then
    . /dit/share/.cmd_history > /dev/null
fi

cp -f /dit/script/dit_profile.sh /etc/profile.d/

exec /bin/bash --login