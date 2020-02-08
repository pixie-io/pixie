<?php
/**
 * Copyright 2018 Pinterest, Inc.
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
 * Lints JavaScript and JSX files using Prettier
 */
final class PrettierLinter extends ArcanistExternalLinter {
  private $cwd = '';

  public function getInfoName() {
    return 'Prettier';
  }

  public function getInfoURI() {
    return 'https://prettier.io/';
  }

  public function getInfoDescription() {
    return pht('An opinionated code formatter with canonicalized AST-derived output');
  }

  public function getLinterName() {
    return 'PRETTIER';
  }

  public function getLinterConfigurationName() {
    return 'prettier';
  }

  public function getDefaultBinary() {
    if ($this->cwd) {
      $realCWD = Filesystem::resolvePath($this->cwd, $this->getProjectRoot());
      list($err, $stdout, $stderr) = exec_manual('yarn -s --cwd %s bin prettier', $realCWD);
      if ($stdout) {
        return strtok($stdout, "\n");
      }
    } else {
      $localBinaryPath = Filesystem::resolvePath('./node_modules/.bin/prettier');

      if (Filesystem::binaryExists($localBinaryPath)) {
        return $localBinaryPath;
      }
    }

    // Fallback on global install & fallthrough to internal existence checks
    return 'prettier';
  }

  public function getVersion() {
    list($err, $stdout, $stderr) = exec_manual('%C -v', $this->getExecutableCommand());
    return $stdout;
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'prettier.cwd' => array(
        'type' => 'optional string',
        'help' => pht('Specify a project sub-directory for both the local prettier install and the sub-directory to lint within.'),
      ),
    );
    return $options + parent::getLinterConfigurationOptions();
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'prettier.cwd':
        $this->cwd = $value;
        return;
    }
    return parent::setLinterConfigurationValue($key, $value);
  }

  public function getInstallInstructions() {
    return pht(
      "\n\t%s[%s globally] run: `%s`\n\t[%s locally] run either: `%s` OR `%s`",
      $this->cwd ? pht("[%s globally] (required for %s) run: `%s`\n\t",
        'yarn',
        '--cwd',
        'npm install --global yarn@1') : '',
      'prettier',
      'npm install --global prettier',
      'prettier',
      'npm install --save-dev prettier',
      'yarn add --dev prettier'
    );
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    if ($err) {
      return false;
    }

    if ($this->getData($path) == $stdout) {
        return array();
    }

    $message = new ArcanistLintMessage();
    $message->setPath($path);
    $message->setSeverity(ArcanistLintSeverity::SEVERITY_AUTOFIX);
    $message->setName('Prettier Format');
    $message->setLine(1);
    $message->setCode($this->getLinterName());
    $message->setChar(1);
    $message->setDescription('This file has not been prettier-ified');
    $message->setOriginalText($this->getData($path));
    $message->setReplacementText($stdout);
    $message->setBypassChangedLineFiltering(true);
    $messages[] = $message;

    return $messages;
  }
}
