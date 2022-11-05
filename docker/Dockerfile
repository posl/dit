ARG BASE_NAME=debian
ARG BASE_VERSION=latest

FROM ${BASE_NAME}:${BASE_VERSION} as builder
WORKDIR /dit/src

# install essential packages
COPY ./etc/dit_install.sh /dit/etc/
RUN sh /dit/etc/dit_install.sh \
        ca-certificates \
        curl \
        gcc \
        make && \
    grep -Fq 'yum' /dit/etc/package_manager || \
    sh /dit/etc/dit_install.sh \
        libc-dev

# build the command to switch users
ARG COMMIT_ID='212b75144bbc06722fbd7661f651390dc47a43d1'
RUN curl -L "https://github.com/ncopa/su-exec/archive/${COMMIT_ID}.tar.gz" | tar -xvz && \
    cd "su-exec-${COMMIT_ID}" && \
    make CC='gcc' && \
    mv su-exec /usr/local/bin/ && \
    cd .. && \
    rm -rf "su-exec-${COMMIT_ID}"

# generate the dit command
COPY ./cmd ./
RUN gcc -O2 -march=native -o /usr/local/bin/dit *.c



ARG BASE_NAME=debian
ARG BASE_VERSION=latest

FROM ${BASE_NAME}:${BASE_VERSION} as product

# load several commands and scripts
COPY --from=builder /usr/local/bin/ /usr/local/bin/
COPY ./etc/ /dit/etc/

# if there is no bash command, install it
RUN /bin/bash --version > /dev/null 2>&1 || \
    sh /dit/etc/dit_install.sh \
        bash && \
    rm -f /dit/etc/package_manager

ARG BASE_NAME=debian
ARG BASE_VERSION=latest

ENV BASE_NAME=${BASE_NAME}
ENV BASE_VERSION=${BASE_VERSION}

USER root
ENTRYPOINT [ "/bin/sh", "/dit/etc/dit_entry.sh" ]