#!/bin/sh


if ( apt-get --version > /dev/null 2>&1 ); then
    apt-get update
    apt-get install -y --no-install-recommends "$@"
    apt-get clean
    rm -rf /var/lib/apt/lists/*

elif ( yum --version > /dev/null 2>&1 ); then
    yum update -y
    yum install -y "$1"
    yum clean all
    rm -rf /var/cache/yum

elif ( apk --version > /dev/null 2>&1 ); then
    apk update
    apk add --no-cache "$@"

else
    echo "dit: no package manager is available" 1>&2
    exit 1

fi