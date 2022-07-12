#!/bin/sh


#
# initialize files related Docker in shared directory
#

if ! pushd /dit; then
    echo "Please check if you created or specified the directory to be bound."
    exit
fi


if [ ! -e Dockerfile ]; then
    touch Dockerfile
fi

if [ ! -e Dockerfile.draft ]; then
    echo "FROM ${BASE}:${VERSION}" > Dockerfile.draft
    echo "WORKDIR $(pwd)" >> Dockerfile.draft
fi
unset BASE
unset VERSION

if [ ! -e .dockerignore ]; then
    cat << EOF > .dockerignore
.cmd_history
.ignore_hist
.ignore_dock
user_entry.sh
EOF
fi



#
# if necessary, reproduce the environment under construction
#

if [ ! -e .cmd_history ]; then
    touch .cmd_history
fi

if [ -s .cmd_history ]; then
    source .cmd_history > /dev/null
fi

popd



#
# set PS1, to be referred every time any command is executed
#

PROMPT_OPT(){
    :
}

PROMPT_STR(){
    echo '\u@\h:\w \$ '
}

PS1='$( (reflect -n && PROMPT_OPT) > /dev/null )'"$( PROMPT_STR )"
export PS1
readonly PS1