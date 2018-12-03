FROM cdrx/fpm-ubuntu:18.04

RUN apt-get update --fix-missing
RUN apt-get install -y bison build-essential cmake flex git libedit-dev \
  clang libclang-dev llvm llvm-dev\
  python zlib1g-dev libelf-dev subversion

ENV CC=clang
ENV CXX=clang++

WORKDIR /llvm_all
RUN svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_700/final llvm

WORKDIR /llvm_all/llvm/tools
RUN svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_700/final clang

WORKDIR /llvm_all/llvm/projects
RUN svn co http://llvm.org/svn/llvm-project/compiler-rt/tags/RELEASE_700/final compiler-rt
RUN svn co http://llvm.org/svn/llvm-project/libcxx/tags/RELEASE_700/final libcxx
RUN svn co http://llvm.org/svn/llvm-project/libcxxabi/tags/RELEASE_700/final libcxxabi
RUN svn co http://llvm.org/svn/llvm-project/polly/tags/RELEASE_700/final polly
RUN svn co http://llvm.org/svn/llvm-project/lld/tags/RELEASE_700/final lld
RUN svn co http://llvm.org/svn/llvm-project/openmp/tags/RELEASE_700/final openmp
RUN svn co http://llvm.org/svn/llvm-project/libunwind/tags/RELEASE_700/final libunwind
    
WORKDIR /llvm_all/build
RUN cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=/opt/clang-7.0 ../llvm

RUN make -j ${nproc}
RUN make install

WORKDIR /opt/clang-7.0
VOLUME /image
ENV deb_name clang-7.0-pl.deb
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
        -s dir -t deb -n clang-7.0 --prefix /opt/clang-7.0 ."]
