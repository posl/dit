#!/bin/sh


#
# record an available package manager and use it to install the specified packages
#

if type apk > /dev/null 2>&1; then
    readonly PACKAGE_MANAGER='apk'

    apk add --no-cache "$@"

elif type apt-get > /dev/null 2>&1; then
    readonly PACKAGE_MANAGER='apt-get'

    apt-get update
    apt-get install -y --no-install-recommends "$@"
    rm -rf /var/lib/apt/lists/*

elif type yum > /dev/null 2>&1; then
    readonly PACKAGE_MANAGER='yum'

    yum install -y "$@"
    rm -rf /var/cache/yum

else
    echo "dit: no package manager is available" 1>&2
    exit 1

fi

echo "${PACKAGE_MANAGER}" > /dit/etc/package_manager