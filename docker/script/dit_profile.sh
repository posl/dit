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
    opt='dit optimize' \
    reflect='dit reflect' \
    refl='dit reflect' \
    setcmd='dit setcmd' \
    setc='dit setcmd'



#
# set PS1, to be referred every time any command is executed
#

PROMPT_REFLECT(){
    echo "$?" > /dit/tmp/last-exit-status
    : > /dit/tmp/convert-result.dock
    : > /dit/tmp/convert-result.hist

    if ( ( history 1 | awk -f /dit/etc/dit_update.awk ) && dit convert ); then
        dit reflect -dh
    fi
}

PROMPT_OPTION(){
    :
}

PROMPT_REPORT(){
    if [ -s /dit/tmp/reflect-report.act ]; then
        cat /dit/tmp/reflect-report.act
    else
        echo '[dock:+0 hist:+0] '
    fi

    : > /dit/tmp/reflect-report.bak
}

export -f PROMPT_REFLECT PROMPT_OPTION PROMPT_REPORT 2> /dev/null || true


PROMPT_STRING='\u@\h:\w \$ '
export PROMPT_STRING


if ( unset PS1 ); then
    PS1='$( ( PROMPT_REFLECT && PROMPT_OPTION ) > /dev/null && PROMPT_REPORT )'"${PROMPT_STRING:-\\$ }"
    readonly PS1
fi

export PS1



#
# if necessary, reproducing the environment under construction
#

DIT_VERSION='1.0.0'

if [ ! -s /dit/etc/dit_version ]; then
    . /dit/mnt/.dit_history > /dev/null
    history -r /dit/mnt/.dit_history
    echo "${DIT_VERSION:-?.?.?}" > /dit/etc/dit_version
fi