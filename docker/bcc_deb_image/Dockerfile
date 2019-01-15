# this is our first build stage, it will not persist in the final image
FROM ubuntu as intermediate

# install git
RUN apt-get update --fix-missing
RUN apt-get install -y git

# add credentials on build
ARG SSH_PRIVATE_KEY
RUN mkdir /root/.ssh/
RUN echo "${SSH_PRIVATE_KEY}" > /root/.ssh/id_rsa && chmod 600 /root/.ssh/id_rsa
# make sure your domain is accepted
RUN touch /root/.ssh/known_hosts

RUN cat "/root/.ssh/id_rsa"

RUN ssh-keyscan github.com >> /root/.ssh/known_hosts

RUN git clone git@github.com:pixie-labs/bcc.git

FROM cdrx/fpm-ubuntu:18.04

RUN apt-get update --fix-missing
RUN apt install -y bison build-essential cmake flex git libedit-dev \
  python zlib1g-dev libelf-dev wget

RUN wget https://storage.googleapis.com/pl-infra-dev-artifacts/clang-7.0-pl4.deb \
    && dpkg -i clang-7.0-pl4.deb

ENV PATH=/opt/clang-7.0/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/clang-7.0/lib:$LD_LIBRARY_PATH
ENV CC=clang
ENV CXX=clang++
ENV LLVM_DIR=/opt/clang-7.0/lib/cmake/llvm/

WORKDIR /src/
COPY --from=intermediate /bcc/ /src/bcc
RUN mkdir bcc/build;
RUN cd bcc/build \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/bcc \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    && make -j $(nproc)\
    && make install

WORKDIR /bcc
VOLUME /image
ENV deb_name bcc-pixie-1.1.deb
ENV deb_version 1.1
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
        -s dir -t deb -n bcc-pixie -v ${deb_version} --prefix /opt/bcc ."]
