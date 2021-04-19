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

final class ArcanistShellCheckLinter extends ArcanistExternalLinter {

  private $shell;
  private $exclude;

  public function getInfoName() {
    return 'ShellCheck';
  }

  public function getInfoURI() {
    return 'http://www.shellcheck.net/';
  }

  public function getInfoDescription() {
    return pht(
      'ShellCheck is a static analysis and linting tool for %s scripts.',
      'sh/bash');
  }

  public function getLinterName() {
    return 'SHELLCHECK';
  }

  public function getLinterConfigurationName() {
    return 'shellcheck';
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'shellcheck.shell' => array(
        'type' => 'optional string',
        'help' => pht(
          'Specify shell dialect (%s, %s, %s, %s).',
          'bash',
          'sh',
          'ksh',
          'zsh'),
      ),
      'shellcheck.exclude' => array(
        'type' => 'optional list<string>',
        'help' => pht('Specify excluded checks, e.g.: SC2035.'),
      ),
    );

    return $options + parent::getLinterConfigurationOptions();
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'shellcheck.shell':
        $this->setShell($value);
        return;

      case 'shellcheck.exclude':
        $this->setExclude($value);
        return;

      default:
        return parent::setLinterConfigurationValue($key, $value);
    }
  }

  public function setShell($shell) {
    $this->shell = $shell;
    return $this;
  }

  public function setExclude($exclude) {
    $this->exclude = $exclude;
    return $this;
  }

  public function getDefaultBinary() {
    return 'shellcheck';
  }

  public function getInstallInstructions() {
    return pht(
      'Install ShellCheck with `%s`.',
      'cabal install shellcheck');
  }

  protected function getMandatoryFlags() {
    $options = array();

    $options[] = '-x';
    $options[] = '--format=checkstyle';

    if ($this->shell) {
      $options[] = '--shell='.$this->shell;
    }

    if ($this->exclude) {
      foreach ($this->exclude as $code) {
        $options[] = '--exclude='.$code;
      }
    }

    return $options;
  }

  public function getVersion() {
    list($stdout, $stderr) = execx(
      '%C --version', $this->getExecutableCommand());

    $matches = null;
    if (preg_match('/version: (\d(?:\.\d){2})/', $stdout, $matches)) {
      return $matches[1];
    }

    return null;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $report_dom = new DOMDocument();
    $ok = @$report_dom->loadXML($stdout);

    if (!$ok) {
      return false;
    }

    $files = $report_dom->getElementsByTagName('file');
    $messages = array();

    foreach ($files as $file) {
      foreach ($file->getElementsByTagName('error') as $child) {
        $code = str_replace('ShellCheck.', '', $child->getAttribute('source'));

        $message = id(new ArcanistLintMessage())
          ->setPath($path)
          ->setLine($child->getAttribute('line'))
          ->setChar($child->getAttribute('column'))
          ->setName($this->getLinterName())
          ->setCode($code)
          ->setDescription($child->getAttribute('message'));

        switch ($child->getAttribute('severity')) {
          case 'error':
            $message->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR);
            break;

          case 'warning':
            $message->setSeverity(ArcanistLintSeverity::SEVERITY_WARNING);
            break;

          case 'info':
            $message->setSeverity(ArcanistLintSeverity::SEVERITY_ADVICE);
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
