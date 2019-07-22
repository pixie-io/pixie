# Not very sensitive to version, just need something that can build C++ libraries with correct deps installed.
FROM gcr.io/pl-dev-infra/dev_image_with_extras:201907212144 as build

WORKDIR /tmp
RUN curl -L -O  https://github.com/gperftools/gperftools/releases/download/gperftools-2.7/gperftools-2.7.tar.gz
RUN tar -zxvf gperftools-2.7.tar.gz
WORKDIR /tmp/gperftools-2.7
RUN ./configure --prefix=/opt/gperftools
RUN make -j10
RUN make install


FROM cdrx/fpm-ubuntu:18.04
COPY --from=build /opt/gperftools /opt/gperftools

WORKDIR /opt/gperftools
VOLUME /image
ENV deb_name gperftools-pixie-2.7-pl2.deb
ENV deb_version 2.7-pl2
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
         -s dir -t deb -n gperftools-pixie -v ${deb_version} --prefix /opt/gperftools ."]
