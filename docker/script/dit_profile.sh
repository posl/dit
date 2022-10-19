#!/bin/bash


#
# define aliases of the tool-specific commands
#

alias \
    config='dit config' \
    conf='dit config' \
    cfg='dit config' \
    convert='dit convert' \
    conv='dit convert' \
    erase='dit erase' \
    healthcheck='dit healthcheck' \
    hc='dit healthcheck' \
    ignore='dit ignore' \
    ig='dit ignore' \
    inspect='dit inspect' \
    insp='dit inspect' \
    label='dit label' \
    onbuild='dit onbuild' \
    onb='dit onbuild' \
    optimize='dit optimize' \
    optim='dit optimize' \
    reflect='dit reflect' \
    refl='dit reflect' \
    setcmd='dit setcmd' \
    setc='dit setcmd'



#
# set shell variables that is referred every time any command is executed
#

PROMPT_REFLECT(){
    LAST_EXIT_STATUS="$?"

    if ( history 1 | awk -f /dit/etc/dit_update.awk ); then
        echo "${LAST_EXIT_STATUS:-0}" > /dit/tmp/last-exit-status

        if ( dit convert -qs ); then
            dit reflect -dh
        fi

        : > /dit/tmp/reflect-report.real
        dit reflect
    fi
}

PROMPT_OPTION(){
    :
}

PROMPT_REPORT(){
    if [ -s /dit/tmp/reflect-report.real ]; then
        cat /dit/tmp/reflect-report.real
    else
        echo 'd:?? h:??'
    fi
}

export -f PROMPT_REFLECT PROMPT_OPTION PROMPT_REPORT 2> /dev/null


if ( unset PROMPT_COMMAND 2> /dev/null ); then
    PROMPT_COMMAND='( PROMPT_REFLECT && PROMPT_OPTION ) > /dev/null'
    readonly PROMPT_COMMAND
fi

CB='\[\e[1;32m\]'
CE='\[\e[m\]'
PS1="${CB}"' ['"${CE}"'$( PROMPT_REPORT )'"${CB}"'] '"${CE}"'\u:\w '"${CB}"'\$ '"${CE}"

export PROMPT_COMMAND
export PS1



#
# if necessary, reproducing the environment under construction
#

DIT_VERSION='1.0.0'

if [ ! -s /dit/etc/dit_version ]; then
    . /dit/mnt/.dit_history > /dev/null
    history -r /dit/mnt/.dit_history
    echo "dit version ${DIT_VERSION:-?.?.?}" > /dit/etc/dit_version
fi