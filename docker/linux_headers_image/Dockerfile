FROM ubuntu:18.04

ARG KERN_VERSION=4.14.104

RUN apt-get update
RUN apt-get upgrade -y -q

# Download Linux sources
RUN apt-get install -y -q wget
WORKDIR /pl/src
RUN wget -nv http://mirrors.edge.kernel.org/pub/linux/kernel/v4.x/linux-${KERN_VERSION}.tar.gz
RUN tar zxf linux-${KERN_VERSION}.tar.gz

# Build Linux kernel into a deb package
RUN apt-get install -y -q build-essential \
  bc \
  libelf-dev \
  libssl-dev
WORKDIR /pl/src/linux-${KERN_VERSION}
ADD config .config
RUN make O=/pl/build oldconfig
RUN rm .config
RUN make -j $(nproc) O=/pl/build deb-pkg

# Extract headers into a tarball
WORKDIR /pl
RUN mkdir build-deb && mv linux* build-deb
RUN dpkg -x build-deb/linux-headers-${KERN_VERSION}_${KERN_VERSION}-1_amd64.deb tmp
RUN mv tmp/usr/src/linux-headers-${KERN_VERSION} .
RUN rm -rf tmp
RUN find linux-headers-${KERN_VERSION} -xtype l | xargs rm
RUN tar zcf linux-headers-${KERN_VERSION}.tar.gz linux-headers-${KERN_VERSION}
