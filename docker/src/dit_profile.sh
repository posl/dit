#!/bin/bash


#
# initialize files related Docker in shared directory
#

if [ ! -e /dit/share ]; then
    echo "Error: please check if you created or specified the directory to be bound." 1>&2
    exit 1
fi


if [ ! -e /dit/share/Dockerfile ]; then
    touch /dit/share/Dockerfile
fi

if [ ! -e /dit/share/Dockerfile.draft ]; then
    cat <<EOF > /dit/share/Dockerfile.draft
FROM ${BASE}:${VERSION}
SHELL [ "${SHELL:-/bin/sh}", "-c" ]
WORKDIR $(pwd)
EOF
    unset BASE
    unset VERSION
fi

if [ ! -e /dit/share/.dockerignore ]; then
    echo ".cmd_history" > /dit/share/.dockerignore
fi



#
# set environment variables, reserved by this tool
#

CHANGE_IN_HISTORY=+0
CHANGE_IN_DOCKERFILE=+0

MODE_OF_REFLECT=both
MODE_OF_IGNORE=simple

MAXLEN_OF_COMMAND=128
MAXLEN_OF_LINE=1024


export \
    LAST_EXIT_STATUS  LAST_SENTENCE \
    CHANGE_IN_HISTORY  CHANGE_IN_DOCKERFILE \
    MODE_OF_REFLECT  MODE_OF_IGNORE \
    MAXLEN_OF_COMMAND  MAXLEN_OF_LINE



#
# set PS1, to be referred every time any command is executed
#

PROMPT_REFLECT(){
    [ $? = 0 ] && LAST_EXIT_STATUS=0 || LAST_EXIT_STATUS=1
    LAST_SENTENCE="$( history 1 | awk 'NR == 1 { $1 = ""; sub(/[ \t]*/, "", $0) } { print $0 }' )"

    if [ "${LAST_EXIT_STATUS}" = 0 ]; then
        :
    fi
}

PROMPT_OPTION(){
    :
}

PROMPT_REPORT(){
    echo "[hist:${CHANGE_IN_HISTORY%:*} dock:${CHANGE_IN_DOCKERFILE%:*}] "
}

export -f PROMPT_REFLECT PROMPT_OPTION PROMPT_REPORT || true


PROMPT_STRING='\u@\h:\w \$ '
export PROMPT_STRING


PS1='$( (PROMPT_REFLECT && PROMPT_OPTION) > /dev/null && PROMPT_REPORT )'"${PROMPT_STRING:-\\$ }"
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