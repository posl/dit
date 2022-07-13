
#
# initialize files related Docker in shared directory
#

if [ ! -e /dit ]; then
    echo "Please check if you created or specified the directory to be bound."
    exit
fi


if [ ! -e /dit/Dockerfile ]; then
    touch /dit/Dockerfile
fi

if [ ! -e /dit/Dockerfile.draft ]; then
    (echo "FROM ${BASE}:${VERSION}" && echo "WORKDIR $(pwd)") > /dit/Dockerfile.draft
fi
unset BASE
unset VERSION

if [ ! -e /dit/.dockerignore ]; then
    cat << EOF > /dit/.dockerignore
.cmd_history
.ignore_hist
.ignore_dock
user_entry.sh
EOF
fi



#
# if necessary, reproduce the environment under construction
#

if [ ! -e /dit/.cmd_history ]; then
    touch /dit/.cmd_history
fi

if [ -s /dit/.cmd_history ]; then
    . /dit/.cmd_history > /dev/null
fi



#
# set PS1, to be referred every time any command is executed
#

CHANGE_HIST=+0
CHANGE_DOCK=+0
export CHANGE_HIST CHANGE_DOCK


PROMPT_DIT(){
    if [ $? ]; then
        convert "$( history -1 | awk '{$1 = ""; sub(/[ \t]+/, "", $0); print $0}' )" \
        | tee >( grep -A"${CHANGE_HIST}" '^ [To History]' | tail -"${CHANGE_HIST}" >> /dit/.cmd_history ) \
        | grep -A"${CHANGE_DOCK}" '^ [To Dockerfile]' | tail -"${CHANGE_DOCK}" >> /dit/Dockerfile.draft
    fi
}

PROMPT_OPT(){
    :
}

PROMPT_STR(){
    echo "[hist:${CHANGE_HIST} dock:${CHANGE_DOCK}] \u@\h:\w \$ "
}


PS1='$( (PROMPT_DIT && PROMPT_OPT) > /dev/null && PROMPT_STR )'
export PS1
readonly PS1