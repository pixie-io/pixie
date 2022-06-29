#!/usr/bin/env bash

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


copy_artifact_to_gcs() {
    output_path="$1"
    binary_path="$2"
    name="$3"

    gpg --no-tty --batch --yes --import "${BUILDBOT_GPG_KEY_FILE}"
    gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "${binary_path}"

    sha256sum "${binary_path}" | awk '{print $1}' > sha
    gsutil -h 'Content-Disposition:filename=px' cp "${binary_path}" "${output_path}/${name}"
    gsutil cp sha "${output_path}/${name}.sha256"
    gsutil cp "${binary_path}.asc" "${output_path}/${name}.asc"
    # Updating the ACL for a specific object will error if there are top-level permissions
    # applied to the bucket.
    gsutil acl ch -u allUsers:READER "${output_path}/${name}" || true
    gsutil acl ch -u allUsers:READER "${output_path}/${name}.sha256" || true
}
