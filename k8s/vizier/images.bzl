def image_map_with_bundle_version(image_map):
    with_version = {}

    for k, v in image_map.items():
        k_with_version = "{0}:{1}".format(k, "$(BUNDLE_VERSION)")
        with_version[k_with_version] = v

    return with_version

def generate_vizier_yamls(name, srcs, out, image_map, **kwargs):
    kustomize_edits = []
    for k in image_map.keys():
        kustomize_edits.append("kustomize edit set image {0}={1}:{2}".format(k, k, "$(BUNDLE_VERSION)"))

    merged_edits = "\n".join(kustomize_edits)
    native.genrule(
        name = name,
        srcs = srcs,
        outs = [out],
        cmd = """
        T=`mktemp -d`
        cp -aL k8s/vizier $$T

        # Update the bundle versions.
        pushd $$T/vizier/prod
        {0}
        popd

        kustomize build $$T/vizier/prod -o $@
        """.format(merged_edits),
    )

def generate_vizier_bootstrap_yamls(name, srcs, out, image_map, **kwargs):
    kustomize_edits = []
    for k in image_map.keys():
        kustomize_edits.append("kustomize edit set image {0}={1}:{2}".format(k, k, "$(BUNDLE_VERSION)"))

    merged_edits = "\n".join(kustomize_edits)
    native.genrule(
        name = name,
        srcs = srcs,
        outs = [out],
        cmd = """
        T=`mktemp -d`
        cp -aL k8s/vizier_bootstrap $$T

        # Update the bundle versions.
        pushd $$T/vizier_bootstrap/prod
        {0}
        popd

        kustomize build $$T/vizier_bootstrap/prod -o $@
        """.format(merged_edits),
    )
