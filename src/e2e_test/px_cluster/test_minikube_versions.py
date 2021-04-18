#!/usr/bin/env python3

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

import os
import argparse
import platform
import subprocess
from itertools import cycle
################################################################################
################################################################################


def system_with_output(cmd):
    output = subprocess.check_output(cmd, shell=True, encoding='UTF-8')
    return output


def sys_check(cmd):
    assert 0 == os.system(cmd), cmd


def get_minikube_versions():
    # grab a list of all minikube versions using the github API
    #
    # the following curl command will yield something like this:
    # v1.15.1
    # v1.15.0
    # v1.14.2
    # v1.14.1
    # v1.14.0
    # v1.14.0-beta.0
    # ... etc.
    cmds = [
        'curl -s https://api.github.com/repos/kubernetes/minikube/releases',
        'grep tag_name',
        "cut -d':' -f2",
        "sed 's/[ \",]//g'",
    ]
    cmd = ' | '.join(cmds)
    versions = system_with_output(cmd)
    versions = versions.split('\n')
    versions = [v for v in versions if v != '']
    return versions


def filter_out_untested_versions(versions):
    # remove versions that pattern match to the following:
    untesteds = [
        'beta',
        'v1.7',
        'v1.8',
    ]
    for u in untesteds:
        versions = [v for v in versions if u not in v]
    return versions


def filter_out_minor_versions(versions):
    # if we have v1.14.2, v1.14.1, and v1.14.0
    # this will return only v1.14.2
    #
    # here, we assume that the list of versions is sorted
    # which apparently it is, after using the github api... caveat emptor
    #
    # at the time of writing this, the final list of minikube versions looked like:
    # v1.15.1, v1.14.2, v1.13.1, v1.12.3, v1.11.0, v1.10.1, v1.9.2
    majors = set()
    filtered_versions = []
    for v in versions:
        toks = v.split('.')
        version = toks.pop(0)
        major = toks.pop(0)
        key = '.'.join([version, major])
        if key not in majors:
            # first time we've seen something like v1.15
            # add this version to the accepted set of versions here
            filtered_versions.append(v)
            majors.add(key)
    return filtered_versions


def remote_link_by_version(version):
    # returns an https path to the remotely hosted binary
    # this uses google storage, but we could alternately use github
    #
    # to test this on your command line:
    # curl -sLO https://storage.googleapis.com/minikube/releases/latest/minikube-darwin-amd64
    system = platform.system().lower()
    minikube_bin = f'minikube-{system}-amd64'
    link = f'https://storage.googleapis.com/minikube/releases/{version}/{minikube_bin}'
    return link


def local_minikube_path_by_version(version):
    system = platform.system().lower()
    minikube_bin = f'minikube-{system}-amd64'
    minikube_path = f'minikubes/{version}/{minikube_bin}'
    return minikube_path


def get_minikubes(versions):
    # downloads a minikube per version in the version list
    # nb, method local_minikube_path_by_version switches based on underlying OS (linux/mac)
    for version in versions:
        minikube_file_path = local_minikube_path_by_version(version)
        minikube_path = os.path.dirname(minikube_file_path)
        remote_link = remote_link_by_version(version)
        mkdir_cmd = f'mkdir -p {minikube_path}'
        curl__cmd = f'curl -sL {remote_link} -o {minikube_file_path}'
        chmod_cmd = f'chmod +x {minikube_file_path}'
        print(f'Downloading minikube {version} to {minikube_file_path}')
        sys_check(mkdir_cmd)
        sys_check(curl__cmd)
        sys_check(chmod_cmd)


def test_minikubes(version_runtime_tuples):
    num_tests = len(version_runtime_tuples)
    for idx, (version, runtime) in enumerate(version_runtime_tuples):
        n = 1 + idx
        minikube_path = local_minikube_path_by_version(version)
        log_file_path = os.path.join(os.path.dirname(minikube_path), 'px.test.log')
        test_msg = f'Testing minikube {version} ({n} of {num_tests})'
        logf_msg = f'writing log to file {log_file_path}'
        runt_msg = f'using container runtime {runtime}'
        print(', '.join([test_msg, logf_msg, runt_msg]) + '.')
        test_cmd = f'./test_px_on_minikube.sh {minikube_path} {runtime} 1> {log_file_path} 2>&1'
        sys_check(test_cmd)

################################################################################
################################################################################


parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--num-tests', type=int, default=0)
parser.add_argument('--runtimes', nargs='+', default=['containerd', 'docker', 'cri-o'])
args = parser.parse_args()

versions = get_minikube_versions()
versions = filter_out_untested_versions(versions)
versions = filter_out_minor_versions(versions)

# if num_tests is given on the cmd. line, pick up first N tests only:
versions = versions[:args.num_tests] if args.num_tests > 0 else versions

# Create a list of tuples like [('v1.15.1', 'containerd'), ('v1.14.2', 'docker'), ...].
# This list will include all the versions, subject to N (command line arg '--num-tests'),
# and will cycle through the runtimes. If N < len(args.runtimes), then N takes
# priority and some runtimes will *not* be tested.
version_runtime_tuples = list(zip(versions, cycle(args.runtimes)))

get_minikubes(versions)
test_minikubes(version_runtime_tuples)
