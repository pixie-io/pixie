private_registry = "gcr.io/pl-dev-infra"
public_registry = "gcr.io/pixie-prod"

def image_map_with_bundle_version(image_map, public):
    with_version = {}

    for k, v in image_map.items():
        image_tag = k
        if public:
            image_tag = image_tag.replace(private_registry, public_registry)
        k_with_version = "{0}:{1}".format(image_tag, "$(BUNDLE_VERSION)")
        with_version[k_with_version] = v

    return with_version

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
        pushd $$T/vizier/prod
        {0}
        popd

        kustomize build $$T/vizier/prod -o $@
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
