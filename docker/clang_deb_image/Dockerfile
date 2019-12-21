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
RUN cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=/opt/clang-9.0 \
    -DLLVM_TARGETS_TO_BUILD="BPF;X86" \
    -DLLVM_ENABLE_SPHINX=Off \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_ENABLE_PROJECTS="clang;compiler-rt;libcxx;libcxxabi;polly;lld;openmp;libunwind" \
    ../llvm-project/llvm

RUN make -j $(nproc)
RUN make install

# We need libclang.a, but the clang build system excludes it during make install.
RUN cp -a /llvm_all/build/lib/libclang.a /opt/clang-9.0/lib

WORKDIR /opt/clang-9.0
VOLUME /image
ENV deb_name clang-9.0-pl2.deb
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
        -s dir -t deb -n clang-9.0 -v 9.0-pl2 --prefix /opt/clang-9.0 ."]
