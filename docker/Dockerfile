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

WORKDIR /cmd
COPY ./cmd ./

# compile c-files and generate a executable file as dit
RUN gcc *.c -o dit -march=native -O2



ARG BASE_IMAGE=debian
ARG BASE_VERSION=latest
FROM ${BASE_IMAGE}:${BASE_VERSION} as product

# load dit command, initialization file and configuration files
COPY --from=builder /cmd/dit /usr/local/bin/
COPY ./script /dit/script/

ARG BASE_IMAGE=debian
ARG BASE_VERSION=latest
ENV BASE_IMAGE=${BASE_IMAGE}
ENV BASE_VERSION=${BASE_VERSION}

USER root
ENTRYPOINT [ "/bin/bash", "/dit/script/dit_entrypoint.sh" ]