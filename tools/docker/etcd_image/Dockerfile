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

FROM --platform=$TARGETPLATFORM quay.io/coreos/etcd:3.5.8@sha256:522b262f24209d7f60c01eaa98a925aa9c20a5c17aa307aeb9dff0345ffb92db

ARG BUILDPLATFORM
ARG TARGETPLATFORM

FROM --platform=$TARGETPLATFORM alpine:3.17@sha256:124c7d2707904eea7431fffe91522a01e5a861a624ee31d03372cc1d138a3126
COPY --from=0 /usr/local/bin/etcd /usr/local/bin/etcd
COPY --from=0 /usr/local/bin/etcdctl /usr/local/bin/etcdctl
COPY --from=0 /usr/local/bin/etcdutl /usr/local/bin/etcdutl

COPY ./*.sh /etc/etcd/scripts/

CMD ["/etc/etcd/scripts/run.sh"]
