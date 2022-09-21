#!/bin/bash


#
# define alias of the tool-specific commands
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
    : > /dit/tmp/last-command-line
    : > /dit/tmp/convert-result.dock
    : > /dit/tmp/convert-result.hist

    if ( ( history 1 | awk -f /dit/src/parse_history.awk ) && dit convert ); then
        dit reflect -d /dit/tmp/convert-result.dock
        dit reflect -h /dit/tmp/convert-result.hist
    fi
}

PROMPT_OPTION(){
    :
}

PROMPT_REPORT(){
    if [ -s /dit/tmp/change-report.act ]; then
        cat /dit/tmp/change-report.act
    else
        echo '[dock:+0 hist:+0] '
    fi

    : > /dit/tmp/change-report.prov
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

if [ ! -s /dit/etc/current-status.dit ]; then
    . /dit/usr/.cmd_history > /dev/null
    history -r /dit/usr/.cmd_history
    echo 'under development' > /dit/etc/current-status.dit
fi