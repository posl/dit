ARG BASE_NAME
ARG BASE_VERSION

FROM "${BASE_NAME}:${BASE_VERSION}" AS builder
WORKDIR /dit/src


# install essential packages
COPY ./etc/package.sh /dit/etc/

RUN sh -eu /dit/etc/package.sh \
        ca-certificates \
        curl \
        gcc \
        make \
    && \
    grep -Fq 'yum' /dit/etc/package_manager || \
    sh -eu /dit/etc/package.sh \
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

RUN make && make clean




ARG BASE_NAME
ARG BASE_VERSION

FROM "${BASE_NAME}:${BASE_VERSION}" AS product


COPY --from=builder /usr/local/bin /usr/local/bin/
COPY --from=builder /dit/etc/package_manager /dit/etc/
COPY ./etc /dit/etc/


# record the default user, and generate the base Dockerfile
ARG BASE_NAME
ARG BASE_VERSION

RUN whoami > /dit/etc/default_user && \
    { \
        echo "FROM ${BASE_NAME}:${BASE_VERSION}" && \
        ( type /bin/bash > /dev/null 2>&1 || dit package -v bash ) && \
        echo 'SHELL [ "/bin/bash", "--posix", "-c" ]'; \
    } > /dit/etc/Dockerfile.base


# initialization process is performed by the privileged user
USER root

ENTRYPOINT [ "/bin/bash", "-eu", "/dit/etc/entrypoint.sh" ]
CMD [ "/bin/bash", "--posix" ]




FROM product AS test

# change not to switch users, and install the diff command as needed
RUN echo 'root' > /dit/etc/default_user && \
    type diff > /dev/null 2>&1 || \
    sh -eu /dit/etc/package.sh \
        diffutils


# verify all dit commands work properly
WORKDIR /dit/test
COPY ./test ./

CMD [ "/bin/sh", "-eux", "main.sh" ]