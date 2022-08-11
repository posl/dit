#!/bin/bash


#
# initialize files related Docker in shared directory
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

if [ ! -e /dit/tmp ]; then
    mkdir /dit/tmp
fi


PROMPT_REFLECT(){
    echo "$?" > /dit/tmp/last-exit-status

    if ( ( history 1 | awk -f /dit/script/parse_history.awk ) && dit convert -s ); then
        awk -f /dit/script/reflect_result.awk < /dit/tmp/last-convert-result
    fi
}

PROMPT_OPTION(){
    :
}

PROMPT_REPORT(){
    if ( cat /dit/tmp/change-report 2> /dev/null ); then
        rm -f /dit/tmp/change-report
    else
        echo "[dock:+0 hist:+0] "
    fi
}

export -f PROMPT_REFLECT PROMPT_OPTION PROMPT_REPORT || true


PROMPT_STRING='\u@\h:\w \$ '
export PROMPT_STRING



PS1='$( ( PROMPT_REFLECT && PROMPT_OPTION ) > /dev/null && PROMPT_REPORT )'"${PROMPT_STRING:-\\$ }"
export PS1
readonly PS1



#
# if necessary, reproduce the environment under construction
#

if [ -e /dit/share/.cmd_history ]; then
    if [ -s /dit/share/.cmd_history ]; then
        . /dit/share/.cmd_history > /dev/null
    fi
else
    touch /dit/share/.cmd_history
fi