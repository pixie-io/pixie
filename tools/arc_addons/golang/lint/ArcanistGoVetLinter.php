<?php

/*
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

/* Modified from the original by vihang@ */

final class ArcanistGoVetLinter extends ArcanistExternalLinter {

  public function getInfoName() {
    return 'Go vet';
  }

  public function getInfoURI() {
    return 'https://godoc.org/golang.org/x/tools/cmd/vet';
  }

  public function getInfoDescription() {
    return 'Vet examines Go source code and reports suspicious constructs.';
  }

  public function getLinterName() {
    return 'GOVET';
  }

  public function getLinterConfigurationName() {
    return 'govet';
  }

  public function getDefaultBinary() {
    return 'go';
  }

  public function getInstallInstructions() {
    return 'Govet comes with Go, please '.
      'follow https://golang.org/doc/install to install go';
  }

  public function shouldExpectCommandErrors() {
    return true;
  }

  protected function canCustomizeLintSeverities() {
    return true;
  }

  protected function getMandatoryFlags() {
    return array('vet');
  }

  protected function buildFutures(array $paths) {
    $executable = $this->getExecutableCommand();
    $flags = $this->getCommandFlags();

    $futures = array();
    foreach ($paths as $path) {
      // Get the package path from the file path.
      $file_path = $this->getProjectRoot().'/'.$path;
      $split_pkg_path = explode('/', $file_path);

      $pkg_path = implode('/', array_slice($split_pkg_path, 0, count($split_pkg_path) - 1)).'/...';

      // Run go vet on the package path.
      $future = new ExecFuture('%s %Ls %s', $executable, $flags, $pkg_path);
      $future->setEnv(array('CGO_ENABLED' => 0));
      $futures[$path] = $future;
    }

    return $futures;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $lines = phutil_split_lines($stderr, false);

    $messages = array();
    foreach ($lines as $line) {
      if (substr($line, 0, 1) === '#') {
        continue;
      }

      $message = id(new ArcanistLintMessage())
        ->setPath($path)
        ->setName('govet')
        ->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR);

      $matches = explode(':', $line);
      if (count($matches) <= 3) {
        $message
          ->setCode('E01')
          ->setDescription($line);
      }

      if (count($matches) >= 4) {
        $p_idx = 0;
        $found = false;
        foreach ($matches as $idx => $match) {
          if ($match === $path) {
            $p_idx = $idx;
            $found = true;
          }
        }

        if ($found && $p_idx < count($matches) - 2) {
          $l = $matches[$p_idx + 1];
          $c = $matches[$p_idx + 2];

          if (is_numeric($l) && is_numeric($c)) {
            $desc = ucfirst(trim(implode(':', array_slice($matches, $p_idx + 3))));
            if (strlen($desc) === 0) {
              $desc = $line;
            }

            $message
              ->setLine($l)
              ->setChar($c)
              ->setCode('E02')
              ->setDescription($desc);
          } else {
            $message
              ->setCode('E03')
              ->setDescription($line);
          }
        } else {
          $message
            ->setCode('E04')
            ->setDescription($line);
        }
        $messages[] = $message;
      }
    }
    return $messages;
  }

}
