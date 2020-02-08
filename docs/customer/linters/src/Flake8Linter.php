<?php
/**
 * Copyright 2018 Pinterest, Inc.
 * Copyright 2014 Phacility, Inc.
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
 * Lints Python source files using flake8.
 *
 * This is an extended version of the stock ArcanistFlake8Linter that adds
 * support for checking required Python and extension versions.
 */
final class Flake8Linter extends ArcanistExternalLinter {

  private $installInstructions = null;
  private $pythonVersion = null;
  private $extensionVersions = array();

  public function getInfoName() {
    return 'Python Flake8 Linter';
  }

  public function getInfoDescription() {
    return pht('Uses flake8 to lint Python source files');
  }

  public function getInfoURI() {
    return 'http://flake8.pycqa.org/';
  }

  public function getLinterName() {
    return 'flake8';
  }

  public function getLinterConfigurationName() {
    return 'flake8ext';
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'flake8.install' => array(
        'type' => 'optional string',
        'help' => pht('Installation instructions.'),
      ),
      'flake8.extensions' => array(
        'type' => 'optional map<string, string>',
        'help' => pht('Map of extension names to version requirments.'),
      ),
      'flake8.python' => array(
        'type' => 'optional string',
        'help' => pht('Python version requirement.'),
      ),
    );

    return $options + parent::getLinterConfigurationOptions();
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'flake8.install':
        $this->installInstructions = $value;
        return;
      case 'flake8.extensions':
        $this->extensionVersions = $value;
        return;
      case 'flake8.python':
        $this->pythonVersion = $value;
        return;
    }

    return parent::setLinterConfigurationValue($key, $value);
  }

  public function getDefaultBinary() {
    return 'flake8';
  }

  private function checkVersion($version, $compare_to) {
    $operator = '==';

    $matches = null;
    if (preg_match('/^([<>]=?|=)\s*(.*)$/', $compare_to, $matches)) {
      $operator = $matches[1];
      $compare_to = $matches[2];
      if ($operator === '=') {
        $operator = '==';
      }
    }

    return version_compare($version, $compare_to, $operator);
  }

  private function parseExtensionVersions($line) {
    $regex = '/([\w-]+): (\d+\.\d+(?:\.\d+)?)/';
    $matches = array();
    if (!preg_match_all($regex, $line, $matches, PREG_SET_ORDER)) {
      return array();
    }

    return ipull($matches, 2, 1);
  }

  public function getVersion() {
    list($stdout) = execx('%C --version', $this->getExecutableCommand());

    $regex =
      '/^(?P<version>\d+\.\d+(?:\.\d+)?) '. # flake8 version
      '\((?P<extensions>.*)\) '.            # extension list
      '.*(?P<python>\d+\.\d+\.\d+)/';       # python version
    $matches = array();
    if (!preg_match($regex, $stdout, $matches)) {
      return false;
    }

    if (!empty($this->pythonVersion) &&
        !$this->checkVersion($matches['python'], $this->pythonVersion)) {
      $message = pht(
        '%s requires %s using Python version %s but found Python version %s.',
        get_class($this),
        $this->getBinary(),
        $this->pythonVersion,
        $matches['python']);
      throw new ArcanistMissingLinterException($message);
    }

    if (!empty($this->extensionVersions)) {
      $versions = $this->parseExtensionVersions($matches['extensions']);

      foreach ($this->extensionVersions as $name => $required) {
        $installed = array_key_exists($name, $versions);

        if (!$installed || !$this->checkVersion($versions[$name], $required)) {
          $message = pht(
            "%s requires flake8 '%s' extension version %s.",
            get_class($this),
            $name,
            $required);

          if ($installed) {
            $message .= pht(' You have version %s.', $versions[$name]);
          }

          $instructions = $this->getInstallInstructions();
          if ($instructions) {
            $message .= "\n".pht('TO INSTALL: %s', $instructions);
          }

          throw new ArcanistMissingLinterException($message);
        }
      }
    }

    return $matches['version'];
  }

  public function getInstallInstructions() {
    if ($this->installInstructions) {
      return $this->installInstructions;
    }
    return pht('Install flake8 using `%s`.', 'pip install flake8');
  }

  public function getUpgradeInstructions() {
    return $this->getInstallInstructions();
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $lines = phutil_split_lines($stdout, false);

    // stdin:2: W802 undefined name 'foo'  # pyflakes
    // stdin:3:1: E302 expected 2 blank lines, found 1  # pep8
    $regexp =
      '/^(?:.*?):(?P<line>\d+):(?:(?P<char>\d+):)? (?P<code>\S+) (?P<msg>.*)$/';

    $messages = array();
    foreach ($lines as $line) {
      $matches = null;
      if (!preg_match($regexp, $line, $matches)) {
        continue;
      }
      foreach ($matches as $key => $match) {
        $matches[$key] = trim($match);
      }

      $message = new ArcanistLintMessage();
      $message->setPath($path);
      $message->setLine($matches['line']);
      if (!empty($matches['char'])) {
        $message->setChar($matches['char']);
      }
      $message->setCode($matches['code']);
      $message->setName($this->getLinterName().' '.$matches['code']);
      $message->setDescription($matches['msg']);
      $message->setSeverity($this->getLintMessageSeverity($matches['code']));

      $messages[] = $message;
    }

    return $messages;
  }

  protected function getDefaultMessageSeverity($code) {
    if (preg_match('/^C/', $code)) {
      // "C": Cyclomatic complexity
      return ArcanistLintSeverity::SEVERITY_ADVICE;
    } else if (preg_match('/^W/', $code)) {
      // "W": PEP8 Warning
      return ArcanistLintSeverity::SEVERITY_WARNING;
    } else {
      // "E": PEP8 Error
      // "F": PyFlakes Error
      //  or: Flake8 Extension Message
      return ArcanistLintSeverity::SEVERITY_ERROR;
    }
  }

  protected function getLintCodeFromLinterConfigurationKey($code) {
    if (!preg_match('/^[A-Z]\d+$/', $code)) {
      throw new Exception(
        pht(
          'Unrecognized lint message code "%s". Expected a valid flake8 '.
          'lint code like "%s", or "%s", or "%s", or "%s".',
          $code,
          'E225',
          'W291',
          'F811',
          'C901'));
    }

    return $code;
  }
}
