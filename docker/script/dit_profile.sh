#!/bin/bash


#
# define alias of tool-specific commands
#

alias \
    config='dit config' \
    conf='dit config' \
    cfg='dit config' \
    convert='dit convert' \
    conv='dit convert' \
    ditcp='dit cp' \
    erase='dit erase' \
    healthcheck='dit healthcheck' \
    hc='dit healthcheck' \
    dithelp='dit help' \
    ignore='dit ignore' \
    ig='dit ignore' \
    inspect='dit inspect' \
    insp='dit inspect' \
    label='dit label' \
    onbuild='dit onbuild' \
    onb='dit onbuild' \
    optimize='dit optimize' \
    opt='optimize' \
    setcmd='dit setcmd' \
    setc='dit setcmd'



#
# set PS1, to be referred every time any command is executed
#

PROMPT_REFLECT(){
    echo "$?" > /dit/tmp/last-exit-status
    : > /dit/tmp/last-command-line
    : > /dit/tmp/last-parse-result
    : > /dit/tmp/last-convert-result

    if ( ( history 1 | awk -f /dit/script/parse_history.awk ) && dit convert -s ); then
        awk -f /dit/script/reflect_result.awk < /dit/tmp/last-convert-result
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

    echo '0 0' > /dit/tmp/change-report.prov
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

if [ ! -s /dit/tmp/current-status.dev ]; then
    . /dit/share/.cmd_history > /dev/null
    echo 'under development' > /dit/tmp/current-status.dev
fi