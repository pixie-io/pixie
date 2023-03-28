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

ARG BASE_IMAGE
FROM $BASE_IMAGE as build

WORKDIR /tmp
RUN curl -L -O https://github.com/gperftools/gperftools/releases/download/gperftools-2.10/gperftools-2.10.tar.gz
RUN tar zxf gperftools-2.10.tar.gz
WORKDIR /tmp/gperftools-2.10
RUN ./configure --prefix=/opt/px_dev/tools/gperftools
RUN make -j $(nproc)
RUN make install

FROM cdrx/fpm-ubuntu:18.04@sha256:7962a6899bee416ddcda7890a6dd212f80789bc9240b064ab2241a28b1d9ecf8
COPY --from=build /opt/px_dev/tools/gperftools /opt/px_dev/tools/gperftools

WORKDIR /opt/px_dev/tools/gperftools
VOLUME /image
ENV DEB_NAME gperftools-pixie-2.10-dev.deb
ENV DEB_VERSION 2.10-dev
CMD ["sh", "-c",  "fpm -p /image/${DEB_NAME} \
  -s dir -t deb -n gperftools-pixie -v ${DEB_VERSION} --prefix /opt/px_dev/tools/gperftools ."]
