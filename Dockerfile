ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
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

# load self-made commands, and shell's initialization file
COPY --from=builder /src/exe /bin/
COPY ./src/cmd/sh /bin/
COPY ./src/cfg/dit_profile.sh /etc/profile.d/

ARG BASE=debian
ARG VERSION=latest
ENV BASE=${BASE}
ENV VERSION=${VERSION}

# FIXME: bash -> sh
CMD ["bash"]