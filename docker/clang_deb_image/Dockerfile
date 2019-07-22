FROM cdrx/fpm-ubuntu:18.04

RUN apt-get update --fix-missing
RUN apt-get install -y bison build-essential cmake flex git libedit-dev \
  clang libclang-dev llvm llvm-dev\
  python2.7-dev swig libncurses5-dev zlib1g-dev libelf-dev subversion

ENV CC=clang
ENV CXX=clang++

WORKDIR /llvm_all
RUN svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_800/final llvm

WORKDIR /llvm_all/llvm/tools
RUN svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_800/final clang
RUN svn co http://llvm.org/svn/llvm-project/lldb/tags/RELEASE_800/final lldb
RUN svn co http://llvm.org/svn/llvm-project/clang-tools-extra/tags/RELEASE_800/final clang/tools/extra

WORKDIR /llvm_all/llvm/projects
RUN svn co http://llvm.org/svn/llvm-project/compiler-rt/tags/RELEASE_800/final compiler-rt
RUN svn co http://llvm.org/svn/llvm-project/libcxx/tags/RELEASE_800/final libcxx
RUN svn co http://llvm.org/svn/llvm-project/libcxxabi/tags/RELEASE_800/final libcxxabi
RUN svn co http://llvm.org/svn/llvm-project/polly/tags/RELEASE_800/final polly
RUN svn co http://llvm.org/svn/llvm-project/lld/tags/RELEASE_800/final lld
RUN svn co http://llvm.org/svn/llvm-project/openmp/tags/RELEASE_800/final openmp
RUN svn co http://llvm.org/svn/llvm-project/libunwind/tags/RELEASE_800/final libunwind

WORKDIR /llvm_all/build
RUN cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=/opt/clang-8.0 \
    -DLLVM_TARGETS_TO_BUILD="BPF;X86" \
    -DLLVM_ENABLE_SPHINX=Off \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_INCLUDE_TESTS=OFF \
    ../llvm

RUN make -j $(nproc)
RUN make install

# We need libclang.a, but the clang build system excludes it during make install.
RUN cp -a /llvm_all/build/lib/libclang.a /opt/clang-8.0/lib

WORKDIR /opt/clang-8.0
VOLUME /image
ENV deb_name clang-8.0-pl1.deb
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
        -s dir -t deb -n clang-8.0 -v 8.0-pl1 --prefix /opt/clang-8.0 ."]
