ARG BASE_IMAGE=debian
ARG BASE_VERSION=latest
FROM ${BASE_IMAGE}:${BASE_VERSION} as builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        gcc \
        libc-dev \
    && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /dit/src
COPY ./cmd ./

# compile c-files and generate a executable file as dit
RUN gcc *.c -o /usr/local/sbin/dit -march=native -O2



ARG BASE_IMAGE=debian
ARG BASE_VERSION=latest
FROM ${BASE_IMAGE}:${BASE_VERSION} as product

# load the dit command and the script files
COPY --from=builder /usr/local/sbin/dit /usr/local/sbin/
COPY ./script /dit/src/

ARG BASE_IMAGE=debian
ARG BASE_VERSION=latest
ENV BASE_IMAGE=${BASE_IMAGE}
ENV BASE_VERSION=${BASE_VERSION}

USER root
ENTRYPOINT [ "/bin/bash", "/dit/src/dit_entry.sh" ]