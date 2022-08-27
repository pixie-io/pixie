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

final class ArcanistGolangCiLinter extends ArcanistExternalLinter {
  public function getInfoName() {
    return 'golangci-lint';
  }

  public function getInfoURI() {
    return 'https://golangci-lint.run/';
  }

  public function getInfoDescription() {
    return 'golangci-lint is a Go linters aggregator.';
  }

  public function getLinterName() {
    return 'GOLANGCI-LINT';
  }

  public function getLinterConfigurationName() {
    return 'golangci-lint';
  }

  public function getDefaultBinary() {
    return 'golangci-lint';
  }

  public function getInstallInstructions() {
    return 'Install golangci-lint using '.
      '`curl -sSfL https://raw.githubusercontent.com/golangci/golangci-lint/master/install.sh | sh -s -- -b $(go env GOPATH)/bin v1.38.0`';
  }

  protected function getDefaultFlags() {
    return array('--out-format=checkstyle');
  }

  protected function getPathArgumentForLinterFuture($path) {
    return dirname($path);
  }

  protected function buildFutures(array $paths) {
    $executable = $this->getExecutableCommand();

    $bin = csprintf('%C run %Ls', $executable, $this->getCommandFlags());

    $futures = array();
    foreach ($paths as $path) {
      $disk_path = $this->getEngine()->getFilePathOnDisk($path);
      $path_argument = $this->getPathArgumentForLinterFuture($disk_path);
      $future = new ExecFuture('%C %C', $bin, $path_argument);
      $future->setEnv(array('CGO_ENABLED' => 0));

      $future->setCWD($this->getProjectRoot());
      $futures[$path] = $future;
    }

    return $futures;
  }

  public function shouldExpectCommandErrors() {
    return true;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $report_dom = new DOMDocument();

    if (!$stdout) {
      // Lack of a XML output usually means that the linter failed to even run and
      // there some larger underlying error. This output ends up in stderr, so just
      // print it.
      $message = id(new ArcanistLintMessage())
          ->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR)
          ->setPath($path)
          ->setCode('E000')
          ->setName('golangci-lint')
          ->setDescription($stderr);
      return [$message];
    }

    $ok = @$report_dom->loadXML($stdout);

    if (!$ok) {
      return false;
    }

    $files = $report_dom->getElementsByTagName('file');
    $messages = array();

    foreach ($files as $file) {
      if ($file->getAttribute('name') != $path) {
        continue;
      }
      foreach ($file->childNodes as $child) {
        if ($child->nodeType == XML_TEXT_NODE) {
          continue;
        }

        $line = $child->getAttribute('line');
        $char = $child->getAttribute('column');

        if ($line === '') {
          $line = null;
        }

        if ($char === '') {
          $char = null;
        }

        $message = id(new ArcanistLintMessage())
          ->setPath($path)
          ->setLine($line)
          ->setChar($char)
          ->setCode($this->getLinterName())
          ->setName($child->getAttribute('source'))
          ->setDescription($child->getAttribute('message'));

        switch ($child->getAttribute('severity')) {
          case 'error':
            $message->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR);
            break;

          case 'warning':
            $message->setSeverity(ArcanistLintSeverity::SEVERITY_WARNING);
            break;

          default:
            $message->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR);
            break;
        }

        $messages[] = $message;
      }
    }

    return $messages;
  }
}
