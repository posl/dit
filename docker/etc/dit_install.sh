#!/bin/sh


#
# record an available package manager and use it to install the specified packages
#

if type apk > /dev/null 2>&1; then
    readonly PACKAGE_MANAGER='apk'

    apk update
    apk add --no-cache "$@"

elif type apt-get > /dev/null 2>&1; then
    readonly PACKAGE_MANAGER='apt-get'

    apt-get update
    apt-get install -y --no-install-recommends "$@"
    apt-get clean
    rm -rf /var/lib/apt/lists/*

elif type yum > /dev/null 2>&1; then
    readonly PACKAGE_MANAGER='yum'

    yum update -y
    yum install -y "$@"
    yum clean all
    rm -rf /var/cache/yum

else
    echo "dit: no package manager is available" 1>&2
    exit 1

fi

echo "${PACKAGE_MANAGER}" > /dit/etc/package_manager