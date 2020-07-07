#!/usr/bin/env bash

if [ "$#" -ne 1 ]; then
  echo "This script requires exactly one argument: <path_to_vega_repo>"
  exit 1
fi
vega_repo=$1
workspace=$(bazel info workspace 2> /dev/null)
vega_patch_path="${workspace}/src/ui/patches"

function get_pkg_version() {
  pkg_name="$1"
  pkg_version=$(jq '.version' "${workspace}/src/ui/node_modules/${pkg_name}/package.json")
  pkg_version="${pkg_version%\"}"
  pkg_version="${pkg_version#\"}"
  echo "${pkg_version}"
}

vega_version=$(get_pkg_version "vega")
vega_version_tag="v${vega_version}"

function create_pkg_patch() {
  pkg="$1"
  pkg_name=$(basename "${pkg}")
  pkg_version=$(get_pkg_version "${pkg_name}")
  pkg_diff=$(
    git diff "${vega_version_tag}" "${pkg}" | sed 's/packages\/'"${pkg_name}"'\//node_modules\/'"${pkg_name}"'\//g')
  patch_path="${vega_patch_path}/${pkg_name}+${pkg_version}.patch"
  echo "${pkg_diff}" > "${patch_path}"
  echo "Created patch file ${patch_path}"
}

echo "Make sure that the diff in the vega repo is based of off the current vega version tag: ${vega_version_tag}".
read -rp "Do you wish to continue? [Y/n] " yn
case $yn in
  [Yy]* ) ;;
  * ) exit 1;;
esac

pushd "${vega_repo}" &> /dev/null || exit 1
packages=$(git diff --name-only "${vega_version_tag}" | grep -o "packages/[^\/]*" | sort | uniq)

for pkg in ${packages}; do
  create_pkg_patch "${pkg}"
done

popd &> /dev/null || exit 1
