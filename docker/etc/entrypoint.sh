#!/bin/bash -eu


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
    /dit/var/optimize.json \
    \
    /tmp/dit_profile.sh

umask "${DEFAULT_UMASK_VALUE}"


chmod a=rx \
    /dit \
    /dit/etc \
    /dit/tmp \
    /dit/var



#
# define symbolic links for each dit command, and initialize the internal files
#

for cmd in $( dit help -a )
do
    ln -s dit /usr/local/bin/"${cmd}"
done


echo '0' > /dit/tmp/last-exit-status
echo '-1' > /dit/tmp/last-history-number

: > /tmp/dit_profile.sh

config -r
ignore -dhr
optimize -r
reflect



#
# set shell variables that is referred every time any command line is executed
#

PROMPT_REFLECT()
{
    local LAST_EXIT_STATUS="$?"
    local PROMPT_STRING=' [d:?? h:??] \u:\w \$ '

    if history 1 | awk -f /dit/etc/parse_history.awk; then
        echo "${LAST_EXIT_STATUS}" > /dit/tmp/last-exit-status

        if dit convert -qs; then
            dit reflect -dh
        fi

        : > /dit/tmp/reflect-report.real
        if dit reflect; then
            PROMPT_STRING="$( cat /dit/tmp/reflect-report.real )"
        fi
    fi

    if unset PS1 2> /dev/null; then
        PS1="${PROMPT_STRING}"
    fi
}

PROMPT_OPTION()
{
    :
}

readonly -f PROMPT_REFLECT
export -f PROMPT_REFLECT PROMPT_OPTION


PROMPT_COMMAND='{ PROMPT_REFLECT; PROMPT_OPTION; } > /dev/null'

readonly PROMPT_COMMAND
export PROMPT_COMMAND



#
# enter a shell as the default user, and reproduce the environment under construction if necessary
#

DEFAULT_USER="$( head -n1 /dit/etc/default_user )"
rm -f /dit/etc/default_user


cat <<EOF > /tmp/dit_profile.sh
set -a
shopt -u expand_aliases
enable -n help

if [ -s /dit/mnt/.dit_history ]; then
    echo 'Reproducing the environment under construction ...'

    set -ex
    . /dit/mnt/.dit_history > /dev/null
    set +ex

    echo 'Done!'
    history -r /dit/mnt/.dit_history
fi

unset ENV
rm -f /tmp/dit_profile.sh
EOF

chown "${DEFAULT_USER}" /tmp/dit_profile.sh

export ENV=/tmp/dit_profile.sh


exec su-exec "${DEFAULT_USER}" "$@"