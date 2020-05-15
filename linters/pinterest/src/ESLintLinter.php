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
 * Lints JavaScript and JSX files using ESLint
 */
final class ESLintLinter extends ArcanistExternalLinter {
  const ESLINT_WARNING = '1';
  const ESLINT_ERROR = '2';

  private $cwd = '';
  private $flags = array();

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
    if ($this->cwd) {
      $realCWD = Filesystem::resolvePath($this->cwd, $this->getProjectRoot());
      list($err, $stdout, $stderr) = exec_manual('yarn -s --cwd %s bin eslint', $realCWD);
      if ($stdout) {
        return strtok($stdout, "\n");
      }
    } else {
      $localBinaryPath = Filesystem::resolvePath('./node_modules/.bin/eslint');

      if (Filesystem::binaryExists($localBinaryPath)) {
        return $localBinaryPath;
      }
    }

    // Fallback on global install & fallthrough to internal existence checks
    return 'eslint';
  }

  public function getVersion() {
    list($err, $stdout, $stderr) = exec_manual('%C -v', $this->getExecutableCommand());

    $matches = array();
    if (preg_match('/^v(\d\.\d\.\d)$/', $stdout, $matches)) {
      return $matches[1];
    } else {
      return false;
    }
  }

  protected function getMandatoryFlags() {
    return array(
      '--format=json',
      '--no-color',
    );
  }

  protected function getDefaultFlags() {
    return $this->flags;
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'eslint.config' => array(
        'type' => 'optional string',
        'help' => pht('Use configuration from this file or shareable config. (https://eslint.org/docs/user-guide/command-line-interface#-c---config)'),
      ),
      'eslint.cwd' => array(
        'type' => 'optional string',
        'help' => pht('Specify a project sub-directory for both the local eslint-cli install and the sub-directory to lint within.'),
      ),
      'eslint.env' => array(
        'type' => 'optional string',
        'help' => pht('Specify environments. To specify multiple environments, separate them using commas. (https://eslint.org/docs/user-guide/command-line-interface#--env)'),
      ),
      'eslint.fix' => array(
        'type' => 'optional bool',
        'help' => pht('Specify whether eslint should autofix issues. (https://eslint.org/docs/user-guide/command-line-interface#fixing-problems)'),
      ),
    );
    return $options + parent::getLinterConfigurationOptions();
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'eslint.config':
        $this->flags[] = '--config';
        $this->flags[] = $value;
        return;
      case 'eslint.cwd':
        $this->cwd = $value;
        return;
      case 'eslint.env':
        $this->flags[] = '--env ';
        $this->flags[] = $value;
        return;
      case 'eslint.fix':
        if ($value) {
          $this->flags[] = '--fix ';
        }
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
      'eslint',
      'npm install --global eslint',
      'eslint',
      'npm install --save-dev eslint',
      'yarn add --dev eslint'
    );
  }

  protected function canCustomizeLintSeverities() {
    return false;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    // Gate on $stderr b/c $err (exit code) is expected.
    if ($stderr) {
      return false;
    }

    $json = json_decode($stdout, true);
    $messages = array();

    foreach ($json as $file) {
      foreach ($file['messages'] as $offense) {
        // Skip file ignored warning: if a file is ignored by .eslintingore
        // but linted explicitly (by arcanist), a warning will be reported,
        // containing only: `{fatal:false,severity:1,message:...}`.
        if (strpos($offense['message'], "File ignored ") === 0) {
          continue;
        }

        $message = new ArcanistLintMessage();
        $message->setPath($file['filePath']);
        $message->setSeverity($this->mapSeverity($offense['severity']));
        $message->setName($offense['ruleId'] || 'unknown');
        $message->setDescription($offense['message']);
        $message->setLine($offense['line']);
        $message->setChar($offense['column']);
        $message->setCode($this->getLinterName());
        $messages[] = $message;
      }
    }

    return $messages;
  }

  private function mapSeverity($eslintSeverity) {
    switch($eslintSeverity) {
      case '0':
      case '1':
        return ArcanistLintSeverity::SEVERITY_WARNING;
      case '2':
      default:
        return ArcanistLintSeverity::SEVERITY_ERROR;
    }
  }
}
