#! /usr/bin/env python3

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

import argparse
import gzip
import os
import subprocess
import tempfile
import glob
import json
import string
import hashlib

import pyzstd
import requests

MIRROR_LIST = [
    "http://mirrors.kernel.org/ubuntu/pool/",
    "http://ftp.osuosl.org/pub/ubuntu/pool/",
    "http://lug.mtu.edu/ubuntu/pool/",
    "http://ubuntu.mirrors.tds.net/ubuntu/pool/",
    "http://ubuntu.secs.oakland.edu/pool/",
    "http://mirror.mcs.anl.gov/pub/ubuntu/pool/",
    "http://mirrors.cat.pdx.edu/ubuntu/pool/",
    "http://ubuntu.cs.utah.edu/ubuntu/pool/",
    "http://ftp.ussg.iu.edu/linux/ubuntu/pool/",
    "http://mirrors.xmission.com/ubuntu/pool/",
    "http://mirrors.cs.wmich.edu/ubuntu/pool/",
    "http://gulus.usherbrooke.ca/pub/distro/ubuntu/pool/",
]

MTIME = 1655507056


def extract_ar_files(path):
    # TODO(james): specify dependence on `ar` binary somewhere.
    # Every debian-based distro always has `ar` so its not a big problem, but probably worth doing at some point.
    result = subprocess.run(['ar', 't', path], check=True, capture_output=True)
    filelist = result.stdout.decode('utf-8')
    files = [x.strip() for x in filelist.split('\n')]
    output_files = dict()
    for fname in files:
        if not fname:
            continue
        result = subprocess.run(
            ['ar', 'p', path, fname], check=True, capture_output=True)
        output_files[fname] = result.stdout
    return output_files


def create_ar_archive(path, files):
    with tempfile.TemporaryDirectory() as tmpdirname:
        for fname, contents in files.items():
            with open(f'{tmpdirname}/{fname}', 'wb') as f:
                f.write(contents)
                os.utime(f'{tmpdirname}/{fname}', times=(MTIME, MTIME))
        content_paths = [f'{tmpdirname}/{fname}' for fname in files.keys()]
        content_paths.sort()
        for content_path in content_paths:
            subprocess.run(['ar', 'q', path, content_path], check=True)


def remove_zstd_compression_from_deb(input_path, output_path):
    files = extract_ar_files(input_path)
    new_files = dict()
    for fname, contents in files.items():
        splits = fname.split('.')
        if splits[-1] == 'zst':
            uncompressed = pyzstd.decompress(contents)
            compressed = gzip.compress(uncompressed, mtime=MTIME)
            new_fname = '.'.join(splits[:-1] + ['gz'])
            new_files[new_fname] = compressed
        else:
            new_files[fname] = contents
    create_ar_archive(output_path, new_files)


def calc_checksum(content):
    m = hashlib.sha256()
    m.update(content)
    return m.hexdigest()


def download_deb(ubuntu_deb_path, checksum, tmpdir):
    deb_content = None
    last_reason = ''
    for mirror_prefix in MIRROR_LIST:
        url = f'{mirror_prefix}{ubuntu_deb_path}'
        resp = requests.get(url)
        if not resp.ok:
            last_reason = resp.reason
            print(f'Warning: mirror {url} failed with: {resp.reason}')
            continue

        download_checksum = calc_checksum(resp.content)
        if download_checksum != checksum:
            last_reason = f'Expected checksum {checksum}, but got {download_checksum}'
            print(f'Checksum mismatch for {mirror_prefix}')
            continue

        deb_content = resp.content
        break

    if deb_content is None:
        raise Exception(
            'All mirrors failed to download deb: %s' % last_reason)

    deb_name = os.path.basename(ubuntu_deb_path)
    dirname = f'{tmpdir}/downloads'
    os.makedirs(dirname, exist_ok=True)

    deb_path = f'{dirname}/{deb_name}'
    with open(deb_path, 'wb') as f:
        f.write(deb_content)

    return deb_path


def push_debs_to_gcs(gcs_prefix, deb_dir):
    deb_files = glob.glob(f'{deb_dir}/*.deb')

    gcs_paths = [
        f'{gcs_prefix}/{MTIME}/{os.path.basename(deb_path)}' for deb_path in deb_files]

    for local, remote in zip(deb_files, gcs_paths):
        subprocess.run(['gsutil', 'cp', local, remote], check=True)

    return gcs_paths


def calculate_new_checksums(deb_dir):
    deb_files = glob.glob(f'{deb_dir}/*.deb')
    new_checksums = dict()
    for path in deb_files:
        with open(path, 'rb') as f:
            checksum = calc_checksum(f.read())
        new_checksums[os.path.basename(path)] = checksum
    return new_checksums


def update_packages_bzl_with_paths(gcs_paths, checksums):
    http_download_template = """
    http_file(
        name = "$name",
        urls = ["$url"],
        sha256 = "$sha256",
        downloaded_file_path = "out.deb",
    )"""
    package_dict_template = """
    "$name": "@$name//file:out.deb","""
    downloads_str = ''
    package_dict_str = ''
    gcs_paths.sort()
    for gcs_path in gcs_paths:
        file_name = os.path.basename(gcs_path)
        name = file_name.split('_')[0]
        csum = checksums[file_name]
        storage_url = 'https://storage.googleapis.com/' + \
            gcs_path.removeprefix('gs://')

        downloads_str += string.Template(http_download_template).substitute(
            name=name,
            url=storage_url,
            sha256=csum,
        )
        package_dict_str += string.Template(package_dict_template).substitute(
            name=name,
        )

    packages_bzl_template = """# Copyright 2018- The Pixie Authors.
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

# This file is autogenerated, do not edit directly.
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

def download_ubuntu_packages():$ubuntu_package_downloads

packages = {$package_dict
}
"""
    workspace_dir = os.environ.get('BUILD_WORKSPACE_DIRECTORY')
    with open(f'{workspace_dir}/bazel/external/ubuntu_packages/packages.bzl', 'w') as f:
        content = string.Template(packages_bzl_template).substitute(
            ubuntu_package_downloads=downloads_str,
            package_dict=package_dict_str,
        )
        f.write(content)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--debs-json', required=True)
    parser.add_argument('--gcs-prefix', required=True)
    parsed_args = parser.parse_args()

    with open(parsed_args.debs_json, 'r') as f:
        debs = json.load(f)

    with tempfile.TemporaryDirectory() as tmpdir:
        output_dir = f'{tmpdir}/output'
        os.makedirs(output_dir, exist_ok=True)

        for deb in debs:
            deb_path = deb["path"]
            checksum = deb["checksum"]
            deb_name = os.path.basename(deb_path)
            print(f'Downloading and converting {deb_name}....')
            input_deb_path = download_deb(deb_path, checksum, tmpdir)
            output_path = f'{output_dir}/{deb_name}'
            remove_zstd_compression_from_deb(input_deb_path, output_path)

        print('Pushing debs to gcs...')
        gcs_paths = push_debs_to_gcs(parsed_args.gcs_prefix, output_dir)

        new_checksums = calculate_new_checksums(output_dir)

        update_packages_bzl_with_paths(gcs_paths, new_checksums)


if __name__ == '__main__':
    main()
