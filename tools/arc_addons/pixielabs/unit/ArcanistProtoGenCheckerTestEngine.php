<?php

/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

final class ArcanistProtoGenCheckerTestEngine extends ArcanistBaseGenCheckerTestEngine {
  public function getEngineConfigurationName() {
    return 'proto-gen-checker';
  }

  private function isGRPCWebProto($file) {
    $proto_dir = dirname($file);
    $bazel_source = '//'.$proto_dir.':'.basename($file);

    list($err, $stdout) = exec_manual('bazel query --noshow_progress "kind(pl_grpc_web_library, %s/...)"', $proto_dir);
    if ($err) {
      return false;
    }
    $targets = phutil_split_lines($stdout, false);

    foreach ($targets as $target) {
      list($err, $stdout) = exec_manual('bazel query "kind(\'source file\', deps(%s)) intersect %s"', $target, $bazel_source);
      if ($err) {
        return false;
      }
      return count(phutil_split_lines($stdout)) > 0;
    }

    return false;
  }

  public function run() {
    $test_results = array();

    foreach ($this->getPaths() as $file) {
      if (!file_exists($this->getWorkingCopy()->getProjectRoot().DIRECTORY_SEPARATOR.$file)) {
        continue;
      }

      $pb_filename = substr($file, 0, -6).'.pb.go';
      $test_results[] = $this->checkFile($file, $pb_filename, 'To regenerate, run: '.
          'scripts/update_go_protos.sh');

      if ($this->isGRPCWebProto($file)) {
        // Check generated files exist. We assume they are all in src/ui/src/types/generated for now.
        $fname = substr($file, strrpos($file, '/') + 1, -6);

        // TODO(nick): Not all of these are in use in the main UI code anymore. Only check for the ones we need.
        // Check $fname_pb.d.ts.
        $test_results[] = $this->checkFile($file, 'src/ui/src/types/generated/'.$fname.'_pb.d.ts', 'To regenerate, build the grpc_web target and move the files to the correct directory');
        // Check $fname_pb.js.
        $test_results[] = $this->checkFile($file, 'src/ui/src/types/generated/'.$fname.'_pb.js', 'To regenerate, build the grpc_web target and move the files to the correct directory');
        // Check $fnameServiceClientPb.ts.
        // TODO(michelle): Figure out a way to make this check smarter for non-grpc protos.
        // $test_results = $this->checkFile($file, 'src/ui/src/types/generated/' . ucfirst($fname) . 'ServiceClientPb.ts', 'To regenerate, build the grpc_web  target and move the files to the correct directory');
      }
    }

    return $test_results;
  }
}
