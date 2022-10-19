ARG BASE_NAME=debian
ARG BASE_VERSION=latest
FROM ${BASE_NAME}:${BASE_VERSION} as builder

# install the gnu c-compiler
WORKDIR /dit/etc
COPY ./script/dit_install.sh ./
RUN sh dit_install.sh gcc libc-dev

# generate the dit command
WORKDIR /dit/src
COPY ./cmd ./
RUN gcc *.c -o /usr/local/bin/dit -mtune=native -march=native -O2



ARG BASE_NAME=debian
ARG BASE_VERSION=latest
FROM ${BASE_NAME}:${BASE_VERSION} as product

ARG BASE_NAME=debian
ARG BASE_VERSION=latest
ENV BASE_NAME=${BASE_NAME}
ENV BASE_VERSION=${BASE_VERSION}

# load the dit command and several script files
COPY --from=builder /usr/local/bin/dit /usr/local/bin/
COPY ./script/ /dit/etc/

# if there is no bash command, install it
RUN /bin/bash --version > /dev/null 2>&1 || sh /dit/etc/dit_install.sh bash

USER root
ENTRYPOINT [ "/bin/sh", "/dit/etc/dit_entry.sh" ]