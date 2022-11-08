#!/bin/sh


#
# Utilitys
#

my_usage(){
    cat <<EOF
Usages:
  sh exec.sh [OPTION]... [TARGET]
Build the image for 'dit', and run a container based on the created image.

Options:
  -n BASE_NAME       overwrite the base-image name
  -v BASE_VERSION    overwrite the base-image version
  -t IMAGE_TAG       overwrite the tag for the image to be generated
  -d SHARED_DIR      overwrite the directory to be bound
  -r                 just run 'docker-compose run'
  -h                 display this help, and exit normally

Targets:
  builder            peek into the stage where the dit command was generated
  product            start interactive development of your Dockerfile (default)
  test               make sure the dit command works properly

Remarks:
  - When options with arguments are given, set environment variables used in 'docker-compose.yml'.
  - If no TARGET is specified, it operates as if 'product' is specified.
  - Depending on the specified TARGET, '.dockerignore' is dynamically generated and deleted on exit.

If no options are given, 'docker-compose build' will use the default environment variables.
See '.env' and 'docker-compose.yml' for details.
EOF
    exit
}


my_abort(){
    echo "Try 'sh exec.sh -h' for more information." 1>&2
    exit 1
}



#
# Option Parse
#

if [ "${OPTIND}" -eq 1 ]; then
    while getopts n:v:t:d:rh OPT
    do
        case "${OPT}" in
            n)
                export BASE_NAME="${OPTARG}"
                ;;
            v)
                export BASE_VERSION="${OPTARG}"
                ;;
            t)
                export IMAGE_TAG="${OPTARG}"
                ;;
            d)
                export SHARED_DIR="${OPTARG}"

                if [ ! -d "${SHARED_DIR}" ]; then
                    echo "exec.sh: No such directory: '${SHARED_DIR}'" 1>&2
                    my_abort
                fi
                ;;
            r)
                NO_BUILD='true'
                ;;
            h)
                my_usage
                ;;
            *)
                my_abort
                ;;
        esac
    done
fi

shift "$(( OPTIND - 1 ))"



#
# Build & Run
#

case "$#" in
    0)
        BUILD_TARGET='product'
        ;;
    1)
        export BUILD_TARGET="$1"
        ;;
    *)
        echo 'exec.sh: No more than two arguments allowed' 1>&2
        my_abort
        ;;
esac


trap 'rm -f docker/.dockerignore' 0 1 2 3 15

case "${BUILD_TARGET}" in
    builder)
        {
            echo 'etc'
            echo '!etc/dit_install.sh'
            echo 'test'
        } > docker/.dockerignore
        ;;
    product)
        {
            echo 'test'
        } > docker/.dockerignore
        ;;
    test)
        if [ -z "${IMAGE_TAG}" ]; then
            export IMAGE_TAG='test'
        fi
        ;;
    *)
        echo "exec.sh: No such build target: '${BUILD_TARGET}'" 1>&2
        my_abort
        ;;
    esac


( [ -n "${NO_BUILD}" ] || docker-compose build ) && docker-compose run --rm dit