ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as builder

RUN DEBIAN_FRONTEND=noninteractive \
    apt-get update && \
    apt-get install -y --no-install-recommends build-essential gcc && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY ./src/cmd ./

# compile c-files in /src, and generate executable files in /src/exe
RUN mkdir exe && \
    for file in ./*.c; do \
        gcc "${file}" -o exe/"${file%.c}" -march=native -O2; \
    done


ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as product

# load tool-specific commands, shell's initialization file and files stored in shared directory
COPY --from=builder /src/exe /usr/local/bin/
COPY ./src/conf/.dit_profile /etc/profile.d/
COPY ./src/conf/.cmd_ignore.* /dit/conf/

ARG BASE=debian
ARG VERSION=latest
ENV BASE=${BASE}
ENV VERSION=${VERSION}

ENTRYPOINT [ "/bin/bash", "--login" ]