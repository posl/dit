ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        gcc \
    && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /cmd
COPY ./src/cmd ./

# compile c-files and generate a executable file as dit
RUN gcc *.c -o dit -march=native -O2



ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as product

# load tool-specific commands, initialization file and configuration files
COPY --from=builder /cmd/dit /usr/local/bin/
COPY ./src/dit_profile.sh /etc/profile.d/
COPY ./conf /dit/conf/

ARG BASE=debian
ARG VERSION=latest
ENV BASE=${BASE}
ENV VERSION=${VERSION}

ENTRYPOINT [ "/bin/bash", "--login" ]