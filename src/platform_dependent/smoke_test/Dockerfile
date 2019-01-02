FROM ubuntu:18.04
RUN apt-get update && apt-get -y install \
    python
RUN mkdir -p /opt/usr/local/pl
COPY smoke_test /opt/usr/local/pl/
RUN mkdir -p /opt/usr/local/pl/bcc-pixie/
COPY bcc-pixie-1.0.deb /opt/usr/local/pl/bcc-pixie-1.0.deb
RUN dpkg -i /opt/usr/local/pl/bcc-pixie-1.0.deb
RUN cd /usr/lib && ln -s /opt/bcc/lib/libbcc.so.0 && \
    ln -s /opt/bcc/lib/libbpf.so.0 && \
    ln -s /opt/bcc/lib/python2.7/dist-packages/bcc/ /usr/lib/python2.7/dist-packages/
WORKDIR /opt/usr/local/pl/
RUN mkdir -p /pl_mount/
COPY CPUDistribution_bcc /opt/usr/local/pl/
COPY run_smoke_test.sh /opt/usr/local/pl/
CMD ["/bin/bash", "-c", "./run_smoke_test.sh > /pl_mount/smoke_test.log 2>&1"]
