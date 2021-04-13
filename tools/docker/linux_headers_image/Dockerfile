FROM ubuntu:18.04

ARG KERN_MAJ
ARG KERN_VERSION

# Install required packages
RUN apt-get update
RUN apt-get upgrade -y -q
RUN apt-get install -y -q build-essential \
  bc \
  libelf-dev \
  libssl-dev \
  flex \
  bison \
  kmod \
  cpio \
  rsync \
  wget

# Download Linux sources
WORKDIR /pl/src
RUN wget -nv http://mirrors.edge.kernel.org/pub/linux/kernel/v${KERN_MAJ}.x/linux-${KERN_VERSION}.tar.gz
RUN tar zxf linux-${KERN_VERSION}.tar.gz

# Build Linux kernel
WORKDIR /pl/src/linux-${KERN_VERSION}
ADD config .config
RUN make olddefconfig
RUN make clean
RUN make -j $(nproc) deb-pkg LOCALVERSION=-pl

# Extract headers into a tarball
WORKDIR /pl
RUN dpkg -x src/linux-headers-${KERN_VERSION}-pl_${KERN_VERSION}-pl-1_amd64.deb .

# Remove broken symlinks
RUN find usr/src/linux-headers-${KERN_VERSION}-pl -xtype l -exec rm {} +
RUN tar zcf linux-headers-${KERN_VERSION}.tar.gz usr

# Remove uneeded files to reduce size
# Keep only:
# - usr/src/linux-headers-x.x.x-pl/include
# - usr/src/linux-headers-x.x.x-pl/arch/x86
# This reduces the size by a little over 2x.
RUN rm -rf usr/share
RUN find usr/src/linux-headers-${KERN_VERSION}-pl -maxdepth 1 -mindepth 1 ! -name include ! -name arch -type d \
    -exec rm -rf {} +
RUN find usr/src/linux-headers-${KERN_VERSION}-pl/arch -maxdepth 1 -mindepth 1 ! -name x86 -type d -exec rm -rf {} +
RUN tar zcf linux-headers-${KERN_VERSION}-trimmed.tar.gz usr
