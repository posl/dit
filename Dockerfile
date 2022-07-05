ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends build-essential gcc && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ENV WORK_DIR /src/
WORKDIR ${WORK_DIR}

COPY ./src/cmd/c ${WORK_DIR}

#
# RUN: gcc で c-file をコンパイルし，実行ファイルを /src/exe ディレクトリに生成
#


ARG BASE=debian
ARG VERSION=latest
FROM ${BASE}:${VERSION} as product

COPY --from=builder /src/exe /bin/
COPY ./src/cmd/sh /bin/
COPY ./src/cfg/dit_profile.sh /etc/profile.d/

ARG BASE=debian
ARG VERSION=latest
ENV BASE=${BASE}
ENV VERSION=${VERSION}
CMD ["sh"]