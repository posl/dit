ARG BASE_NAME
ARG BASE_VERSION

FROM "${BASE_NAME}:${BASE_VERSION}" AS builder
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
RUN make && \
    make clean



ARG BASE_NAME
ARG BASE_VERSION

FROM "${BASE_NAME}:${BASE_VERSION}" AS product

# load several commands and scripts
COPY --from=builder /usr/local/bin/ /usr/local/bin/
COPY ./etc/ /dit/etc/

# if there is no bash command, install it
RUN /bin/bash --version > /dev/null 2>&1 || \
    sh /dit/etc/dit_install.sh \
        bash && \
    rm -f /dit/etc/package_manager

ARG BASE_NAME
ARG BASE_VERSION

ENV BASE_NAME="${BASE_NAME}"
ENV BASE_VERSION="${BASE_VERSION}"

# record the default user because it will be switched to a privileged user for a while
RUN whoami > /dit/etc/default_user
USER root

ENTRYPOINT [ "/bin/sh", "/dit/etc/dit_entry.sh" ]
CMD [ "/bin/bash", "--login" ]



FROM product AS test

# load test script for each dit command
WORKDIR /dit/test
COPY ./test ./

# interactively verify that all dit commands work properly
CMD [ "/bin/sh", "main.sh" ]