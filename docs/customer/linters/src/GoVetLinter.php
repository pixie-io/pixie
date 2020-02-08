<?php
/**
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
 */

/**
 * Uses the Go vet command to lint for suspicious code constructs.
 */
final class GoVetLinter extends ArcanistExternalLinter {

  public function getInfoName() {
    return 'Vet';
  }

  public function getInfoDescription() {
    return pht('Vet examines Go source code and reports suspicious constructs.');
  }

  public function getInfoURI() {
    return 'https://golang.org/cmd/vet/';
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

  public function getVersion() {
    list($err, $stdout, $stderr) = exec_manual(
      '%C version',
      $this->getExecutableCommand());

    $matches = array();
    if (preg_match('/\bgo(?P<version>\d+\.\d+(?:.\d+|rc\d+))\b/', $stdout, $matches)) {
      return $matches['version'];
    } else {
      return false;
    }
  }

  public function getInstallInstructions() {
    return pht('Vet is part of the go tool.');
  }

  protected function getMandatoryFlags() {
    return array('tool', 'vet');
  }

  public function shouldExpectCommandErrors() {
    return true;
  }

  protected function canCustomizeLintSeverities() {
    return true;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $lines = phutil_split_lines($stderr, false);

    $messages = array();
    foreach ($lines as $line) {
      $matches = explode(':', $line, 3);

      if (count($matches) === 3) {
        $message = new ArcanistLintMessage();
        $message->setPath($path);
        $message->setLine($matches[1]);
        $message->setCode($this->getLinterName());
        $message->setName($this->getLinterName());
        $message->setDescription(ucfirst(trim($matches[2])));
        $message->setSeverity(ArcanistLintSeverity::SEVERITY_ADVICE);

        $messages[] = $message;
      }
    }

    return $messages;
  }
}
