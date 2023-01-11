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

# This file manages the generation of tasks.json (builds) and launch.json (debug commands).
# Simply run: python scripts/generate_vscode_tasks.py from the bazel workspace root.
import argparse
import json
import os
import subprocess
import sys


def getOutputBase():
    return subprocess.check_output(
        ['bazel', 'info', 'output_base']).rstrip().decode("utf-8")


# Returns source file map. Some of these are only relevant for Mac or Linux, respectively,
# but they don't cause any negative side-effects so we leave them all in here.
def getSourceFileMap(output_base):
    return {
        '/proc/self/cwd/': '${workspaceFolder}/',
        '{0}/execroot/px/src/'.format(output_base): '${workspaceFolder}/src/',
        '{0}/execroot/px/src/'.format(output_base.replace('/private/', '/')):
            '${workspaceFolder}/src/',
    }


# Generates a path based on the Bazel test target.
def getPathFromTestTarget(target):
    return target.replace('//', '').replace(':', '/')


# Generate task segment based on compilation and exec modes.
def generateTaskSegment(target, exec_mode, comp_mode, output_std, v_level):
    if exec_mode not in ['build', 'test']:
        raise ValueError('exec_mode must be either build or test')
    if comp_mode not in ['dbg', 'opt']:
        raise ValueError('comp_mode must be either dbg or opt')
    args = []
    args.append('{0}'.format(exec_mode))
    args.append('--compilation_mode={0}'.format(comp_mode))
    if output_std:
        args.append("--test_output=all")
        args.append("--action_env=\"GTEST_COLOR=1\"")
        args.append("--action_env=\"GLOG_logtostderr=1\"")
        args.append("--action_env=\"GLOG_colorlogtostderr=1\"")
        args.append("--action_env=\"GLOG_log_prefix=0\"")
        args.append("--action_env=\"GLOG_v={}\"".format(v_level))
    args.append(target)

    return {
        'label': '(bazel:{0}:{1}) {2}'.format(comp_mode, exec_mode, target),
        'type': 'shell',
        'command': 'bazel',
        'args': args,
        'group': '{0}'.format(exec_mode),
    }


def generateTaskSegments(target, output_std, v_level):
    tasks = []
    exec_mode_list = ['build', 'test']
    for exec_mode in exec_mode_list:
        for comp_mode in ['dbg', 'opt']:
            tasks += [generateTaskSegment(target,
                                          exec_mode, comp_mode, output_std, v_level)]

    return tasks


def generateLaunchSegments(lldb_mode, target, output_base):
    if lldb_mode:
        return [{
            "args": [],
            "cwd": "${workspaceFolder}",
            'name': '(lldb) Launch {0}'.format(target),
            'preLaunchTask': '(bazel:dbg:build) {0}'.format(target),
            'program': '{0}/bazel-bin/{1}'.format('${workspaceFolder}',
                                                  getPathFromTestTarget(target)),
            "request": "launch",
            "sourceMap": getSourceFileMap(output_base),
            "type": "lldb"
        }]
    else:
        return [{
            'name': '(lldb-mi) Launch {0}'.format(target),
            'type': 'cppdbg',
            'request': 'launch',
            'program': '{0}/bazel-bin/{1}'.format('${workspaceFolder}',
                                                  getPathFromTestTarget(target)),
            'args': [],
            'stopAtEntry': False,
            'cwd': '${workspaceFolder}',
            'environment': [],
            'externalConsole': False,
            'MIMode': 'lldb',
            'preLaunchTask': '(bazel:dbg:build) {0}'.format(target),
            'sourceFileMap': getSourceFileMap(output_base),
            # This is required to get the debugger to work properly on macos.
            'miDebuggerPath': 'lldb-mi',
        }]


# Formats a directory target from a given path.
def format_directory_target(path):
    if path[-1] != '/':
        path += '/'
    return path + "..."


# Combines path sections into a formatted directory target.
def split_path_format(path_sections):
    return format_directory_target('//{}'.format('/'.join(path_sections)))


# Gets the parent directory targets for a given target path.
def get_all_parent_directories(target_path):
    split_path = target_path.split('/')
    if not (split_path[0] == '' and split_path[1] == ''):
        return []

    split_paths_content = split_path[2:]
    new_paths = []
    for i in range(0, len(split_paths_content) + 1):
        new_paths.append(split_path_format(split_paths_content[:i]))

    return new_paths


# Takes in the list of targets and adds all of the parent directories as targets.
def get_build_directories(targets):
    seen_test_dirs = set()
    for t in targets:
        path_target_split = t.split(':')
        if len(path_target_split) != 2:
            continue
        path, target = path_target_split
        seen_test_dirs.update(get_all_parent_directories(path))

    return seen_test_dirs


def main():
    parser = argparse.ArgumentParser(description='Generate files for vscode.')
    parser.add_argument('--lldb', action='store_true',
                        help='Generate for LLDB (defaults to vscode mode)', default=False)
    parser.add_argument('--all_output', action='store_true',
                        help='Output all of the output', default=False)
    parser.add_argument(
        '--v', help='Verbosity level (if supported) of the test.', default=0, type=int)
    parsed = parser.parse_args(sys.argv[1:])
    if parsed.lldb:
        print('In LLDB mode')

    targets_str = subprocess.check_output(
        ['bazel', 'query', 'kind(\'cc_test rule\', //...)']).decode("utf-8")
    output_base = getOutputBase()
    task_list = []
    launch_list = []
    targets = targets_str.split('\n')
    for target in targets:
        if target != '':
            task_list += generateTaskSegments(target,
                                              parsed.all_output, parsed.v)
            launch_list += generateLaunchSegments(
                parsed.lldb, target, output_base)

    # directory targets don't have a launch segment.
    directory_targets = get_build_directories(targets)
    for target in directory_targets:
        if target != '':
            task_list += generateTaskSegments(target,
                                              parsed.all_output, parsed.v)

    # Task to regnerate the file.
    task_list += [{
        'label': 'generate tasks/launch json files (lldb)',
        'type': 'shell',
        'command': 'python',
        'args': [
            '${workspaceFolder}/scripts/generate_vscode_tasks.py',
            '--lldb',
            '--all_output',
        ],
        'group': 'build'
    }]

    task_list += [{
        'label': 'generate tasks/launch json files',
        'type': 'shell',
        'command': 'python',
        'args': [
            '${workspaceFolder}/scripts/generate_vscode_tasks.py',
        ],
        'group': 'build'
    }]

    task_list += [{
        'label': 'generate tasks/launch json files with output, v=1',
        'type': 'shell',
        'command': 'python',
        'args': [
            '${workspaceFolder}/scripts/generate_vscode_tasks.py',
            '--all_output',
            '--v=1'
        ],
        'group': 'build'
    }]

    task_list += [{
        'label': 'generate tasks/launch json files with output, v=2',
        'type': 'shell',
        'command': 'python',
        'args': [
            '${workspaceFolder}/scripts/generate_vscode_tasks.py',
            '--all_output',
            '--v=2'
        ],
        'group': 'build'
    }]

    # Task to generate all bazel targets.
    task_list += [{
        'label': '(bazel:{0}:build) all'.format(comp_mode),
        'type': 'shell',
        'command': 'bazel',
        'args': [
            'build',
            '--compilation_mode={0}'.format(comp_mode),
            '//...',
            '--',
            '-//src/ui/...',
        ],
        'group': 'build',
    } for comp_mode in ['opt', 'dbg']]

    tasks = {
        'version': '2.0.0',
        'tasks': task_list,
    }
    launch = {
        'version': '0.2.0',
        'configurations': launch_list,
    }
    workspace_root = subprocess.check_output(
        ['bazel', 'info', 'workspace']).rstrip().decode("utf-8")

    with open(os.path.join(workspace_root, '.vscode/launch.json'), 'w') as launch_file:
        launch_file.write(json.dumps(launch, indent=4, sort_keys=True))

    with open(os.path.join(workspace_root, '.vscode/tasks.json'), 'w') as task_file:
        task_file.write(json.dumps(tasks, indent=4, sort_keys=True))


if __name__ == '__main__':
    main()
