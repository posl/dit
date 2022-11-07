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
  product            start interactive development of your Dockerfile
  test               make sure the dit command works properly

Remarks:
  - If no TARGET is specified, it operates as if 'product' is specified.
  - There is no need to specify TARGET for normal use.
  - '. ./exec.sh' should not be used as it may leave unnecessary environment variables.

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

if [ "$#" -eq 1 ]; then
    export BUILD_TARGET="$1"

    if [ "${BUILD_TARGET}" = 'test' ] && [ -z "${IMAGE_TAG}" ]; then
        export IMAGE_TAG='test'
    fi

elif [ "$#" -gt 1 ]; then
    echo 'exec.sh: No more than two arguments allowed' 1>&2
    my_abort
fi


( [ -n "${NO_BUILD}" ] || docker-compose build ) && exec docker-compose run --rm dit