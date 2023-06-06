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

FROM quay.io/coreos/etcd:3.5.9@sha256:18ca110b5ce9a177bb80d6b4a08d73bda54b549d7a0eb6f66e6da69bf919c63f
COPY --from=busybox:1.36.1-musl@sha256:a0f497089b970e099fe76fc28f352851c6a99d8191c67c9269b02f1e96db5db9 /bin /bin

COPY ./*.sh /etc/etcd/scripts/

CMD ["/etc/etcd/scripts/run.sh"]
