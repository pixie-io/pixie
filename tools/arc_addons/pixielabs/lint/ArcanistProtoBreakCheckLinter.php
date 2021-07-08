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

final class ArcanistProtoBreakCheckLinter extends ArcanistExternalLinter {
  private $future;

  public function getInfoName() {
    return 'proto-break-check';
  }

  public function getInfoURI() {
    return 'https://github.com/uber/prototool#prototool-break-check';
  }

  public function getInfoDescription() {
    return 'proto-break-check looks for backward incompatible protobuf changes';
  }

  public function getLinterName() {
    return 'proto-break-check';
  }

  public function getLinterConfigurationName() {
    return 'proto-break-check';
  }

  public function getDefaultBinary() {
    return 'prototool';
  }

  public function getInstallInstructions() {
    return 'Install from github releases '.
      'https://github.com/uber/prototool/releases';
  }

  protected function getMandatoryFlags() {
    return array('--git-branch=main', '--json');
  }

  // Running prototool once per file causes spurious errors around new proto files
  // or moving protos between files while maintaining the package name etc.
  // Instead we want to run prototool once over the entire repo.
  // So we override the arcanist default behavior of running a linter once per file
  // to trigger a repo wide prototool lint only once if any proto files change.
  public function willLintPaths(array $paths) {
    if ($this->future) {
      return;
    }
    $executable = $this->getExecutableCommand();
    $flags = $this->getCommandFlags();
    $this->future = new ExecFuture('%C break check %Ls', $executable, $flags);
    $this->future->setCWD($this->getProjectRoot());
  }

  public function didLintPaths(array $paths) {
    if (!$this->future) {
      return;
    }

    list($err, $stdout, $stderr) = $this->future->resolve();
    if ($err !== 0) {
      $lines = phutil_split_lines($stdout, false);
      foreach ($lines as $line) {
        $json = phutil_json_decode($line);
        $this->addLintMessage(id(new ArcanistLintMessage())
          ->setPath(isset($json['filename']) ? $json['filename'] : '')
          ->setCode(isset($json['lint_id']) ? $json['lint_id'] : '')
          ->setName('prototool')
          ->setDescription(isset($json['message']) ? $json['message'] : $line)
          ->setSeverity(ArcanistLintSeverity::SEVERITY_WARNING));
      }
    }

    $this->future = null;
  }

  public function parseLinterOutput($path, $err, $stdout, $stderr) {
    return;
  }
}
