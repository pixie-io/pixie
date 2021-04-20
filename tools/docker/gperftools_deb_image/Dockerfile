# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

# Not very sensitive to version, just need something that can build C++ libraries with correct deps installed.
FROM gcr.io/pixie-oss/pixie-dev-public/dev_image_with_extras:202104202215 as build

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
