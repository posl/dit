#!/bin/bash


#
# define aliases of the tool-specific commands
#

alias \
    cmd='dit cmd' \
    cfg='dit config' \
    conf=cfg \
    config=cfg \
    conv='dit convert' \
    convert=conv \
    copy='dit copy' \
    erase='dit erase' \
    hc='dit healthcheck' \
    healthcheck=hc \
    ig='dit ignore' \
    ignore=ig \
    insp='dit inspect' \
    inspect=insp \
    label='dit label' \
    onb='dit onbuild' \
    onbuild=onb \
    optim='dit optimize' \
    optimize=optim \
    refl='dit reflect' \
    reflect=refl



#
# set shell variables that is referred every time any command line is executed
#

PROMPT_REFLECT(){
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

PROMPT_OPTION(){
    :
}

export -f PROMPT_REFLECT PROMPT_OPTION


if unset PROMPT_COMMAND 2> /dev/null; then
    PROMPT_COMMAND='{ PROMPT_REFLECT; PROMPT_OPTION; } > /dev/null'
    readonly PROMPT_COMMAND
fi

export PROMPT_COMMAND



#
# if necessary, reproducing the environment under construction
#

DIT_VERSION='1.0.0'

if [ ! -s /dit/etc/dit_version ]; then
    # . /dit/mnt/.dit_history > /dev/null
    history -r /dit/mnt/.dit_history
    echo "dit version ${DIT_VERSION:-?.?.?}" > /dit/etc/dit_version
fi