#!/bin/bash

usage() {
    echo "Usage: $0 [workspace] [kernel_version] [arch] [cross_compile] [output_dir]"
    echo "Example: $0 /px 5.4.0-42-generic x86_64 /usr/bin/x86_64-linux-gnu- /output"
}

if [ "$#" -ne 5 ]; then
    usage
    exit 1
fi

WORKSPACE=$1
KERN_VERSION=$2
ARCH=$3
# shellcheck disable=SC2034
CROSS_COMPILE=$4
OUTPUT_DIR=$5

if [ "${CROSS_COMPILE}" == "false" ]; then
    CROSS_COMPILE=""
fi
echo "Cross compile: ${CROSS_COMPILE}"

mkdir -p "${WORKSPACE}"/src
pushd "${WORKSPACE}"/src || exit

KERN_MAJ=$(echo "${KERN_VERSION}" | cut -d'.' -f1);
KERN_MIN=$(echo "${KERN_VERSION}" | cut -d'.' -f2);
wget -nv http://mirrors.edge.kernel.org/pub/linux/kernel/v"${KERN_MAJ}".x/linux-"${KERN_VERSION}".tar.gz

tar zxf linux-"${KERN_VERSION}".tar.gz

pushd linux-"${KERN_VERSION}" || exit

cp /configs/"${ARCH}" .config
make ARCH="${ARCH}" olddefconfig
make ARCH="${ARCH}" clean

LOCALVERSION="-pl"

DEB_ARCH="${ARCH//x86_64/amd64}"
# binary builds are required for non git trees after linux v6.3 (inclusive).
# The .deb file suffix is also different.
TARGET='bindeb-pkg'
DEB_SUFFIX="-1_${DEB_ARCH}.deb"
if [ "${KERN_MAJ}" -lt 6 ] || { [ "${KERN_MAJ}" -le 6 ] && [ "${KERN_MIN}" -lt 3 ]; }; then
    TARGET='deb-pkg'
    DEB_SUFFIX="${LOCALVERSION}-1_${DEB_ARCH}.deb"
fi
echo "Building ${TARGET} for ${KERN_VERSION}${LOCALVERSION} (${ARCH})"

make ARCH="${ARCH}" -j "$(nproc)" "${TARGET}" LOCALVERSION="${LOCALVERSION}"

popd || exit
popd || exit

# Extract headers into a tarball
dpkg -x src/linux-headers-"${KERN_VERSION}${LOCALVERSION}_${KERN_VERSION}${DEB_SUFFIX}" .

# Remove broken symlinks
find usr/src/linux-headers-"${KERN_VERSION}${LOCALVERSION}" -xtype l -exec rm {} +

# Remove uneeded files to reduce size
# Keep only:
# - usr/src/linux-headers-x.x.x-pl/include
# - usr/src/linux-headers-x.x.x-pl/arch/${ARCH}
# This reduces the size by a little over 2x.
rm -rf usr/share
find usr/src/linux-headers-"${KERN_VERSION}${LOCALVERSION}" -maxdepth 1 -mindepth 1 ! -name include ! -name arch -type d \
    -exec rm -rf {} +
find usr/src/linux-headers-"${KERN_VERSION}${LOCALVERSION}"/arch -maxdepth 1 -mindepth 1 ! -name "${ARCH//x86_64/x86}" -type d -exec rm -rf {} +

tar zcf linux-headers-"${ARCH}"-"${KERN_VERSION}".tar.gz usr

cp linux-headers-*.tar.gz "${OUTPUT_DIR}"/
