#!/bin/sh


# =============================================================================
# initialize files in shared directory
# =============================================================================

if ! pushd /dit; then
    echo "Please check if you granted run-time option like '--mount type=bind,src=<YourDir>,dst=/dit'"
    exit
fi


if [ ! -e Dockerfile ]; then
    touch Dockerfile
fi

if [ ! -e Dockerfile.draft ]; then
    echo "FROM ${BASE}:${VERSION}" > Dockerfile.draft
fi
unset BASE
unset VERSION

if [ ! -e .dockerignore ]; then
    cat << EOF > .dockerignore
.cmd_log
.cmd_ignore
.cmd_disregard
EOF
fi


if [ ! -e .cmd_log ]; then
    touch .cmd_log
fi

if [ ! -e .cmd_ignore ]; then
    cat << EOF > .cmd_ignore
commit
optimize
inspect
setuser
label
healthcheck
ignore
disregard
EOF
fi

if [ ! -e .cmd_disregard ]; then
    cat << EOF > .cmd_disregard
ls
less
more
EOF
fi

popd



# =============================================================================
# if log-file isn't empty, reproduce the environment under construction
# =============================================================================


if [ -s .cmd_log ]; then
    . .cmd_log > /dev/null
fi



# =============================================================================
# set PS1, to realize interactional development in docker container
# =============================================================================

export PS1='\u@\h:\w \$ '
ls -alhSF