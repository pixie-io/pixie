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
ARG BASE_IMAGE_DIGEST

FROM ${BASE_IMAGE}@sha256:${BASE_IMAGE_DIGEST}

ARG USER_ID
ARG USER_NAME
ARG GROUP_ID
ARG DOCKER_ID
ARG SHELL

ENV RUNNING_IN_DOCKER true

RUN addgroup --gid ${GROUP_ID} ${USER_NAME} \
  && addgroup --gid ${DOCKER_ID} docker-host \
  && adduser --disabled-password \
  --gecos '' \
  --uid ${USER_ID} \
  --gid ${GROUP_ID} \
  ${USER_NAME} \
  --shell ${SHELL} \
  && usermod -a -G sudo ${USER_NAME} \
  && usermod -a -G docker-host ${USER_NAME}

# Ensure sudo group users are not asked for a password when using
# sudo command by ammending sudoers file.
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> \
  /etc/sudoers

USER ${USER_NAME}
VOLUME /home/${USER_NAME}
ENV SHELL=${SHELL}
CMD ["/usr/bin/zsh"]
