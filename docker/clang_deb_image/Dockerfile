FROM cdrx/fpm-ubuntu:18.04

RUN apt-get update --fix-missing
RUN apt-get install -y bison build-essential cmake flex git libedit-dev \
  clang libclang-dev llvm llvm-dev\
  python2.7-dev swig libncurses5-dev zlib1g-dev libelf-dev subversion

ENV CC=clang
ENV CXX=clang++

WORKDIR /llvm_all
RUN git clone --branch llvmorg-9.0.0 --depth 1 https://github.com/llvm/llvm-project.git

WORKDIR /llvm_all/build
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=/opt/clang-9.0 \
    -DLLVM_TARGETS_TO_BUILD="BPF;X86;WebAssembly" \
    -DLLVM_ENABLE_SPHINX=Off \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_DEFAULT_TARGET_TRIPLE=${triple} \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;clangd;compiler-rt;libcxx;libcxxabi;polly;lld;openmp;libunwind" \
    ../llvm-project/llvm

RUN make -j $(nproc)
RUN make install

# We need libclang.a, but the clang build system excludes it during make install.
RUN cp -a /llvm_all/build/lib/libclang.a /opt/clang-9.0/lib

#######################################################
# This installs Clang/LLVM with libc++.
#
# Since we link Clang/LLVM with our own source code we
# need it to be built with the same underlying C++
# library.
#######################################################
WORKDIR /llvm_libc++
RUN git clone https://github.com/rsmmr/install-clang

WORKDIR /llvm_libc++/install-clang
RUN git checkout 4c25bb7486a786efd082ab3cb3abdcdb5c356bc4
COPY install_clang.patch install_clang.patch
RUN patch -p1 < install_clang.patch
RUN ./install-clang -j $(nproc) -A /opt/clang-9.0-libc++ -C
RUN rm -rf /opt/clang-9.0-libc++/src

# TODO(zasgar): MSAN version. It's currently failing to build because
# of an MSAN failure inside of LLVM.
#COPY msan.patch msan.patch
#RUN patch -p1 < msan.patch
#RUN ./install-clang -j $(nproc) -A /opt/clang-9.0-libc++-msan -C

WORKDIR /opt
VOLUME /image
ENV deb_name clang-9.0-pl5.deb
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
        -s dir -t deb -n clang-9.0 -v 9.0-pl5 --prefix /opt clang-9.0 clang-9.0-libc++"]
