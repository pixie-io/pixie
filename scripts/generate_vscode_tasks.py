# This file manages the generation of tasks.json (builds) and launch.json (debug commands).
# Simply run: python scripts/generate_vscode_tasks.py from the bazel workspace root.
import argparse
import json
import os
import subprocess
import sys


def getOutputBase():
    return subprocess.check_output(
        ['bazel', 'info', 'output_base']).rstrip()


# Returns source file map. Some of these are only relevant for Mac or Linux, respectively,
# but they don't cause any negative side-effects so we leave them all in here.
def getSourceFileMap(output_base):
    return {
        '/proc/self/cwd/': '${workspaceFolder}/',
        '{0}/execroot/pl/src/'.format(output_base): '${workspaceFolder}/src/',
        '{0}/execroot/pl/src/'.format(output_base.replace('/private/', '/')):
            '${workspaceFolder}/src/',
    }


# Generates a path based on the Bazel test target.
def getPathFromTestTarget(target):
    return target.replace('//', '').replace(':', '/')


# Generate task segment based on compilation and exec modes.
def generateTaskSegment(target, exec_mode, comp_mode, output_std):
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
    args.append(target)

    return {
        'label': '(bazel:{0}:{1}) {2}'.format(comp_mode, exec_mode, target),
        'type': 'shell',
        'command': 'bazel',
        'args': args,
        'group': '{0}'.format(exec_mode),
    }


def generateTaskSegments(target, output_std):
    tasks = []
    for exec_mode in ['build', 'test']:
        for comp_mode in ['dbg', 'opt']:
            tasks += [generateTaskSegment(target, exec_mode, comp_mode, output_std)]

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
            'name': '(lldb) Launch {0}'.format(target),
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


def main():
    parser = argparse.ArgumentParser(description='Generate files for vscode.')
    parser.add_argument('--lldb', action='store_true',
                        help='Generate for LLDB (defaults to vscode mode)', default=False)
    parser.add_argument('--all_output', action='store_true',
                        help='Output all of the output', default=False)
    parsed = parser.parse_args(sys.argv[1:])
    if parsed.lldb:
        print('In LLDB mode')

    targets = subprocess.check_output(
        ['bazel', 'query', 'kind(\'cc_test rule\', //...)'])
    output_base = getOutputBase()
    task_list = []
    launch_list = []
    for target in targets.split('\n'):
        if target != '':
            task_list += generateTaskSegments(target, parsed.all_output)
            launch_list += generateLaunchSegments(
                parsed.lldb, target, output_base)

    # Task to regnerate the file.
    task_list += [{
        'label': 'generate tasks/launch json files (lldb)',
        'type': 'shell',
        'command': 'python',
        'args': [
            '${workspaceFolder}/scripts/generate_vscode_tasks.py',
            '--lldb'
        ],
        'group': 'build'
    }]

    task_list += [{
        'label': 'generate tasks/launch json files',
        'type': 'shell',
        'command': 'python',
        'args': [
            '${workspaceFolder}/scripts/generate_vscode_tasks.py',
            '--lldb'
        ],
        'group': 'build'
    }]

    task_list += [{
        'label': 'generate tasks/launch json files with output',
        'type': 'shell',
        'command': 'python',
        'args': [
            '${workspaceFolder}/scripts/generate_vscode_tasks.py',
            '--all_output'
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
        ['bazel', 'info', 'workspace']).rstrip()

    with open(os.path.join(workspace_root, '.vscode/launch.json'), 'w') as launch_file:
        launch_file.write(json.dumps(launch, indent=4, sort_keys=True))

    with open(os.path.join(workspace_root, '.vscode/tasks.json'), 'w') as task_file:
        task_file.write(json.dumps(tasks, indent=4, sort_keys=True))


if __name__ == '__main__':
    main()
