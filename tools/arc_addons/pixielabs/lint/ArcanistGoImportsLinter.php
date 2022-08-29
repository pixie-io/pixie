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

final class ArcanistGoImportsLinter extends ArcanistExternalLinter {
  public function getInfoName() {
    return 'Go imports';
  }

  public function getInfoURI() {
    return 'https://github.com/golang/tools';
  }

  public function getInfoDescription() {
    return 'Goimports formats Go imports.';
  }

  public function getLinterName() {
    return 'GOIMPORTS';
  }

  public function getLinterConfigurationName() {
    return 'goimports';
  }

  public function getDefaultBinary() {
    return 'goimports';
  }

  public function getInstallInstructions() {
    return 'Install Go imports using '.
      '`go get -u golang.org/x/tools/cmd/goimports`';
  }

  // Run before go vet.
  public function getLinterPriority() {
    return 2.0;
  }

  protected function getMandatoryFlags() {
    return array('-local=px.dev');
  }

  protected function buildFutures(array $paths) {
    $executable = $this->getExecutableCommand();
    $flags = $this->getCommandFlags();

    $futures = array();
    foreach ($paths as $path) {
      $data = $this->getData($path);
      $future = new ExecFuture('%C %Ls', $executable, $flags);
      $future->write($data);
      $futures[$path] = $future;
    }
    return $futures;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    if (empty($stdout) && $err) {
      throw new Exception(
        sprintf(
          "%s failed to parse output!\n\nSTDOUT\n%s\n\nSTDERR\n%s",
          $this->getLinterName(),
          $stdout,
          $stderr)
      );
    }

    $data = $this->getData($path);

    $messages = array();
    if ($stdout !== $data) {
      $desc = sprintf(
          '%s was not formatted correctly. Please setup your '.
          'editor to run goimports on save', $path);

      $message = id(new ArcanistLintMessage())
        ->setPath($path)
        ->setLine(1)
        ->setChar(1)
        ->setCode('E00')
        ->setName('goimports')
        ->setDescription($desc)
        ->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR)
        ->setOriginalText($data)
        ->setReplacementText($stdout);

      $messages[] = $message;
    }
    return $messages;
  }

}
