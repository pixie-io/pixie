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

private_registry = "gcr.io/pixie-oss/pixie-dev"
public_registry = "gcr.io/pixie-oss/pixie-prod"

# TODO(michellenguyen, PP-2630): Old vizier versions expect update_job images in the
# old registry. Remove 5/20/21.
old_public_registry = "gcr.io/pixie-prod"
old_private_registry = "gcr.io/pl-dev-infra"

def image_map_with_bundle_version(image_map, public, old_registry):
    with_version = {}

    for k, v in image_map.items():
        image_tag = k

        if not public and old_registry:
            image_tag = image_tag.replace(private_registry, old_private_registry)
        if public and not old_registry:
            image_tag = image_tag.replace(private_registry, public_registry)
        if public and old_registry:
            image_tag = image_tag.replace(private_registry, old_public_registry)
        k_with_version = "{0}:{1}".format(image_tag, "$(BUNDLE_VERSION)")
        with_version[k_with_version] = v

    return with_version

def generate_cloud_yamls(name, srcs, out, image_map, public, **kwargs):
    kustomize_edits = []
    kustomize_dir = "prod" if public else "staging"

    for k in image_map.keys():
        image_path = k
        if public:
            image_path = image_path.replace(private_registry, public_registry)
        kustomize_edits.append("kustomize edit set image {0}={1}:{2}".format(k, image_path, "$(BUNDLE_VERSION)"))

    merged_edits = "\n".join(kustomize_edits)
    native.genrule(
        name = name,
        srcs = srcs,
        outs = [out],
        cmd = """
        T=`mktemp -d`
        cp -aL k8s/cloud $$T

        # Update the bundle versions.
        pushd $$T/cloud/{0}
        {1}
        popd

        kustomize build $$T/cloud/{0} -o $@
        """.format(kustomize_dir, merged_edits),
    )

def generate_vizier_yamls(name, srcs, out, image_map, public, **kwargs):
    kustomize_edits = []
    for k in image_map.keys():
        image_path = k
        if public:
            image_path = image_path.replace(private_registry, public_registry)
        kustomize_edits.append("kustomize edit set image {0}={1}:{2}".format(k, image_path, "$(BUNDLE_VERSION)"))

    merged_edits = "\n".join(kustomize_edits)
    native.genrule(
        name = name,
        srcs = srcs,
        outs = [out],
        cmd = """
        T=`mktemp -d`
        cp -aL k8s/vizier $$T

        # Update the bundle versions.
        pushd $$T/vizier/etcd_metadata
        {0}
        popd

        kustomize build $$T/vizier/etcd_metadata -o $@
        """.format(merged_edits),
    )

def generate_vizier_bootstrap_yamls(name, srcs, out, image_map, public, **kwargs):
    kustomize_edits = []
    for k in image_map.keys():
        image_path = k
        if public:
            image_path = image_path.replace(private_registry, public_registry)
        kustomize_edits.append("kustomize edit set image {0}={1}:{2}".format(k, image_path, "$(BUNDLE_VERSION)"))

    merged_edits = "\n".join(kustomize_edits)
    native.genrule(
        name = name,
        srcs = srcs,
        outs = [out],
        cmd = """
        T=`mktemp -d`
        mkdir -p $$T/k8s/vizier
        cp -aL k8s/vizier/bootstrap $$T/k8s/vizier

        # Update the bundle versions.
        pushd $$T/k8s/vizier/bootstrap
        {0}
        popd

        kustomize build $$T/k8s/vizier/bootstrap/ -o $@
        """.format(merged_edits),
    )

def generate_vizier_metadata_persist_yamls(name, srcs, out, image_map, public, **kwargs):
    kustomize_edits = []
    for k in image_map.keys():
        image_path = k
        if public:
            image_path = image_path.replace(private_registry, public_registry)
        kustomize_edits.append("kustomize edit set image {0}={1}:{2}".format(k, image_path, "$(BUNDLE_VERSION)"))

    merged_edits = "\n".join(kustomize_edits)
    native.genrule(
        name = name,
        srcs = srcs,
        outs = [out],
        cmd = """
        T=`mktemp -d`
        cp -aL k8s/vizier $$T

        # Update the bundle versions.
        pushd $$T/vizier/persistent_metadata
        {0}
        popd

        kustomize build $$T/vizier/persistent_metadata -o $@
        """.format(merged_edits),
    )
