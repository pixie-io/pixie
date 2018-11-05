FROM python:3-alpine as base

FROM base as builder

RUN apk add --update --no-cache \
    gcc \
    linux-headers \
    make \
    musl-dev \
    python-dev \
    g++

COPY load_generation/requirements.txt .

RUN pip install --install-option="--prefix=/install" -r requirements.txt

FROM base
COPY --from=builder /install /usr/local

ARG configDir
COPY $configDir/load.pbtxt .
COPY $configDir/locustfile.loadgen.tmpl .

COPY load_generation .

RUN mkdir /lib64 && ln -s /lib/libc.musl-x86_64.so.1 /lib64/ld-linux-x86-64.so.2
RUN apk add g++

ENTRYPOINT ./loadgen.sh
