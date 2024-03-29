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

if [ ! -s /dit/mnt/.dockerignore ]; then
    {
        echo .dit_history;
        echo Dockerfile.draft;
    } > /dit/mnt/.dockerignore
fi


touch \
    /dit/mnt/.dit_history \
    /dit/mnt/.dockerignore \
    /dit/mnt/Dockerfile \
    /dit/mnt/Dockerfile.draft



#
# set the permissions on several files specific to this tool
#

chown root /usr/local/bin/dit
chmod a=x,u+s /usr/local/bin/dit

find /dit/bin -type f -exec chmod a=x {} +
find /dit/etc -type f -exec chmod a=r {} +

chmod a=rw \
    /dit/mnt/.dit_history \
    /dit/mnt/.dockerignore \
    /dit/mnt/Dockerfile \
    /dit/mnt/Dockerfile.draft


DEFAULT_UMASK_VALUE="$( umask )"
umask 0000

if [ ! -e /dit/srv ]; then
    mkdir /dit/srv
fi

if [ ! -e /dit/var ]; then
    mkdir /dit/var
fi

touch \
    /dit/srv/convert-result.dock \
    /dit/srv/convert-result.hist \
    /dit/srv/erase-result.dock \
    /dit/srv/erase-result.hist \
    /dit/srv/last-command-line \
    /dit/srv/last-exit-status \
    /dit/srv/last-history-number \
    /dit/srv/reflect-report.prov \
    /dit/srv/reflect-report.real \
    \
    /dit/var/cmd.log \
    /dit/var/config.stat \
    /dit/var/erase.log.dock \
    /dit/var/erase.log.hist \
    /dit/var/ignore.json.dock \
    /dit/var/ignore.json.hist \
    /dit/var/ignore.list.args \
    /dit/var/optimize.json

umask "${DEFAULT_UMASK_VALUE}"


chmod a=rx \
    /dit \
    /dit/bin \
    /dit/etc \
    /dit/srv \
    /dit/var



#
# define symbolic links for each dit command, and initialize the internal files
#

for cmd in $( dit help -a )
do
    ln -s dit /usr/local/bin/"${cmd}"
done


echo '0' > /dit/srv/last-exit-status
echo '-1' > /dit/srv/last-history-number

config -r
ignore -r
ignore -dhr
optimize -r
reflect



#
# set shell variables that is referred every time any command line is executed
#

PROMPT_REFLECT()
{
    local LAST_EXIT_STATUS="$?"

    if history 1 | awk -f /dit/etc/parse_history.awk; then
        echo "${LAST_EXIT_STATUS}" > /dit/srv/last-exit-status

        if dit convert -qs; then
            dit reflect -dh
        fi

        : > /dit/srv/reflect-report.real
        dit reflect

        if unset PS1 2> /dev/null && [ -f /dit/srv/reflect-report.real ]; then
            PS1="$( < /dit/srv/reflect-report.real )"
        fi
    fi
}

PROMPT_OPTION()
{
    :
}

export -f PROMPT_REFLECT PROMPT_OPTION


PS1=' [d:?? h:??] \u:\w \$ '
PROMPT_COMMAND='{ PROMPT_REFLECT; PROMPT_OPTION; } > /dev/null'

export PS1 PROMPT_COMMAND



#
# enter a shell as the default user, and reproduce the environment under construction if necessary
#

DEFAULT_USER="$( head -n1 /dit/tmp/default_user )"
rm -f /dit/tmp/default_user


cat <<EOF > /dit/tmp/.profile
set -a
shopt -u expand_aliases
enable -n help

readonly PROMPT_COMMAND
readonly -f PROMPT_REFLECT

if [ -s /dit/mnt/.dit_history ]; then
    echo 'Reproducing the environment under construction ...'

    set -ex
    . /dit/mnt/.dit_history > /dev/null
    set +ex

    echo 'Done!'
    history -r /dit/mnt/.dit_history
fi

unset ENV
rm -f /dit/tmp/.profile
EOF

chown "${DEFAULT_USER}" /dit/tmp/.profile

export ENV=/dit/tmp/.profile


exec /dit/bin/su-exec "${DEFAULT_USER}" "$@"