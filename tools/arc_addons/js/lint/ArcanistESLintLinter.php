<?php

/*
 * Copyright 2016 Pinterest, Inc.
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

/**
 * Lints JavaScript and JSX files using ESLint
 */
final class ArcanistESLintLinter extends ArcanistExternalLinter {
  const ESLINT_WARNING = '1';
  const ESLINT_ERROR = '2';

  private $future;

  public function getInfoName() {
    return 'ESLint';
  }

  public function getInfoURI() {
    return 'https://eslint.org/';
  }

  public function getInfoDescription() {
    return pht('The pluggable linting utility for JavaScript and JSX');
  }

  public function getLinterName() {
    return 'ESLINT';
  }

  public function getLinterConfigurationName() {
    return 'eslint';
  }

  public function getDefaultBinary() {
    return 'eslint';
  }

  public function getVersion() {
    // Pixie specific patch. We `yarn install` to run eslint, so just
    // asking for a version is expensive. Since we don't have a version
    // check in this Linter anyway, just return a static version that will
    // not be used for anything.
    return '6.8.0';
  }

  protected function getMandatoryFlags() {
    return array('--format=json', '--no-color');
  }

  public function getInstallInstructions() {
    return 'yarn add eslint';
  }

  protected function canCustomizeLintSeverities() {
    return false;
  }

  public function willLintPaths(array $paths) {
    if ($this->future || empty($paths)) {
      return;
    }

    $ui_src_dir = $this->getProjectRoot().DIRECTORY_SEPARATOR.'src'.DIRECTORY_SEPARATOR.'ui';

    // Make sure js node modules are installed.
    $future = new ExecFuture('yarn install');
    $future->setCWD($ui_src_dir);
    $future->resolvex();

    $relative_paths = array();
    foreach ($paths as $path) {
      $relative_paths[] = '..'.DIRECTORY_SEPARATOR.'..'.DIRECTORY_SEPARATOR.$path;
    }

    $flags = $this->getCommandFlags();

    $this->future = new ExecFuture('yarn run eslint %Ls %Ls', $flags, $relative_paths);
    $this->future->setCWD($ui_src_dir);
    $this->future;
  }

  public function didLintPaths(array $paths) {
    if (!$this->future) {
      return;
    }

    list($err, $stdout, $stderr) = $this->future->resolve();
    $json = json_decode($stdout, true);

    foreach ($json as $file) {
      foreach ($file['messages'] as $offense) {
        // Skip file ignored warning: if a file is ignored by .eslintingore
        // but linted explicitly (by arcanist), a warning will be reported,
        // containing only: `{fatal:false,severity:1,message:...}`.
        if (strncmp($offense['message'], 'File ignored ', 13) === 0) {
          continue;
        }

        $message = new ArcanistLintMessage();
        $message->setPath($file['filePath']);
        $message->setSeverity($this->mapSeverity($offense['severity']));
        $message->setName($offense['ruleId'] || 'unknown');
        $message->setDescription($offense['message']);
        if (isset($offense['line'])) {
          $message->setLine($offense['line']);
        }
        if (isset($offense['column'])) {
          $message->setChar($offense['column']);
        }
        $message->setCode($this->getLinterName());

        $this->addLintMessage(id($message));
      }
    }

    $this->future = null;
  }

  private function mapSeverity($eslint_severity) {
    switch ($eslint_severity) {
      case '0':
      case '1':
        return ArcanistLintSeverity::SEVERITY_WARNING;
      case '2':
      default:
        return ArcanistLintSeverity::SEVERITY_ERROR;
    }
  }

  public function parseLinterOutput($path, $err, $stdout, $stderr) {
    return;
  }
}
