#!/usr/bin/env python
import os
import subprocess
import shutil
import argparse

pj = os.path.join


def call_cmd(cmd):
    ''' Calls a command in the shell. '''
    assert type(cmd) in [str, list], "cmd must be list or str"
    if isinstance(cmd, str):
        cmd_res = cmd.split()
    elif isinstance(cmd, list):
        cmd_res = cmd

    p = subprocess.Popen(cmd_res, stdout=subprocess.PIPE)
    output, err = p.communicate()
    return output


def copy_go_proto_bazel_rule(bazel_workspace, bazel_rule, verbose=False):
    ''' Copies the go proto created by the bazel rule.'''
    # should fail if there are extra colons
    bazel_rule_path, bazel_rule_name = bazel_rule.split(':')
    # remove leading forward slashes.
    bazel_rule_path = bazel_rule_path.strip('//')

    # hack to find the architectures in the path
    arch_path_parent = pj(bazel_workspace, "bazel-bin", bazel_rule_path)
    if not os.path.exists(arch_path_parent):
        return bazel_rule
    matching_paths = [p for p in os.listdir(
        arch_path_parent) if "_stripped" in p]
    if len(matching_paths) != 1:
        return bazel_rule
    arch_path = pj(arch_path_parent, matching_paths[0])

    # find the pb.go file within the architecture and bazel_rule_name path
    gen_pb_dir = pj(arch_path, "{}%".format(bazel_rule_name))
    if not os.path.exists(gen_pb_dir):
        return bazel_rule
    pbgo_files = []
    for dirpath, dirnames, filenames in os.walk(gen_pb_dir):
        for filename in [f for f in filenames if f.endswith(".pb.go")]:
            pbgo_files.append(os.path.join(dirpath, filename))

    # fail if there are multiple matches, or none
    # (given that we're looking at one rule, there should only be one)
    assert_msg = "For '{2}', Expected 1 pbgo, got {0} : [{1}]".format(
        len(pbgo_files), ",".join(pbgo_files), bazel_rule)

    assert len(pbgo_files) == 1, assert_msg

    gen_proto_path = pbgo_files[0]
    assert os.path.exists(
        gen_proto_path), "{} doesn't exist.".format(gen_proto_path)
    if verbose:
        print("\t" + gen_proto_path)

    # copy overfile to the source directory of the proto.
    dest_path = pj(bazel_workspace, bazel_rule_path,
                   os.path.basename(gen_proto_path))
    if os.path.exists(dest_path):
        os.remove(dest_path)
    shutil.copy2(gen_proto_path, dest_path)

    # fails if the new copy doesn't exist.
    assert os.path.exists(
        dest_path), "didn't successfully copy to {0}".format(dest_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-r', '--rule', action='append', help='Rules to run', required=False)
    parser.add_argument('-v', '--verbose', action='store_true', help='verbose mode')
    args = parser.parse_args()
    if args.rule:
        all_rules = args.rule
    else:
        # get all of the go proto library rules
        get_all_rules_cmd = ['bazel', 'query',
                             "kind(\"go_proto_library rule\", //...)"]

        all_rules = call_cmd(get_all_rules_cmd).strip().split('\n')
    # get the top of the workspace.
    bazel_workspace = call_cmd("bazel info workspace").strip()
    failed_rules = []
    if args.verbose:
        print("Source files:")
    for bazel_rule in all_rules:
        # bazel_rule = "//src/carnot/proto:plan_pl_go_proto"
        res = copy_go_proto_bazel_rule(bazel_workspace, bazel_rule, args.verbose)
        if res is not None:
            failed_rules.append(res)

    print("Rules that succeeded")
    print("\t" + "\n\t".join(set(all_rules) - set(failed_rules)))
    # print out any rules that failed because of missing build files.
    print("Rules that failed (you might need to build these.)")
    print("\t" + "\n\t".join(failed_rules))
