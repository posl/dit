ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as builder

RUN DEBIAN_FRONTEND=noninteractive \
    apt-get update && \
    apt-get install -y --no-install-recommends build-essential gcc && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY ./src/cmd/c ./

# compile c-files in /src, and generate executable files in /src/exe
RUN mkdir exe && \
    chmod +x compile.sh && \
    ./compile.sh


ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as product

# load self-made commands, shell's initialization file and files stored in shared directory
COPY --from=builder /src/exe /bin/
COPY ./src/cmd/sh /bin/
COPY ./src/cfg/dit_profile.sh /etc/profile.d/
COPY ./src/cfg/.ignore_* /dit/

ARG BASE=debian
ARG VERSION=latest
ENV BASE=${BASE}
ENV VERSION=${VERSION}