<?php
/**
 * Copyright 2017 Pinterest, Inc.
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
 * Lints Java source files using the "checkstyle" tool.
 */
final class CheckstyleLinter extends ArcanistExternalLinter {

  private $config = null;

  public function getInfoName() {
    return 'Checkstyle Linter';
  }

  public function getInfoDescription() {
    return pht('Checks Java files');
  }

  public function getInfoURI() {
    return 'http://checkstyle.sourceforge.net/';
  }

  public function getLinterName() {
    return 'CHECKSTYLE';
  }

  public function getLinterConfigurationName() {
    return 'checkstyle';
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'checkstyle.config' => array(
        'type' => 'string',
        'help' => pht("Configuration file to use."),
      ),
    );

    return $options + parent::getLinterConfigurationOptions();
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'checkstyle.config':
        $this->config = $value;
        return;
    }

    return parent::setLinterConfigurationValue($key, $value);
  }

  public function getDefaultBinary() {
    return 'checkstyle';
  }

  public function getVersion() {
    list($err, $stdout, $stderr) = exec_manual(
      '%C -v',
      $this->getExecutableCommand());

    $matches = array();
    if (preg_match('/^Checkstyle version: (.*)$/', $stdout, $matches)) {
      return $matches[1];
    } else {
      return false;
    }
  }

  public function getInstallInstructions() {
    return pht(
      'Install checkstyle using `%s` (macOS) or `%s` (Linux).',
      'brew install checkstyle',
      'apt-get install checkstyle');
  }

  protected function getMandatoryFlags() {
    return array('-c', $this->config);
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $lines = phutil_split_lines($stdout, false);

    // [ERROR] /path/to/file.java:31: Message text [Indentation]
    // [WARN] /path/to/file.java:31:10: Message text [Indentation]
    $regex = '/^\[(?P<severity>[A-Z]+)\] '.
      '(?P<path>.*?):(?P<line>\d+):(?:(?P<char>\d+):)? '.
      '(?P<message>.*) \[(?P<type>.*)\]$/';

    $messages = array();
    foreach ($lines as $line) {
      $matches = null;
      if (preg_match($regex, $line, $matches)) {
        $message = new ArcanistLintMessage();
        $message->setPath($path);
        $message->setLine($matches['line']);
        if (!empty($matches['char'])) {
          $message->setChar($matches['char']);
        }
        $message->setCode($matches['type']);
        $message->setName($this->getLinterName());
        $message->setDescription($matches['message']);
        $message->setSeverity($this->getMatchSeverity($matches['severity']));
        $messages[] = $message;
      }
    }

    return $messages;
  }

  private function getMatchSeverity($name) {
    $map = array(
      'ERROR' => ArcanistLintSeverity::SEVERITY_ERROR,
      'WARN'  => ArcanistLintSeverity::SEVERITY_WARNING,
    );

    if (array_key_exists($name, $map)) {
       return $map[$name];
    }

    return ArcanistLintSeverity::SEVERITY_ERROR;
  }
}
