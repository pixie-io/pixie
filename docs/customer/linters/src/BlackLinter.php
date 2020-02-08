<?php
/**
 * Copyright 2019-2020 Pinterest, Inc.
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
 * Formats Python files using Black.
 */
final class BlackLinter extends ArcanistExternalLinter {

  const LINT_STYLE = 0;
  const LINT_ERROR = 1;

  public function getInfoName() {
    return 'Black';
  }

  public function getInfoURI() {
    return 'https://black.readthedocs.io/';
  }

  public function getInfoDescription() {
    return pht('Black is an opinionated code formatter for Python.');
  }

  public function getLinterName() {
    return 'BLACK';
  }

  public function getLinterConfigurationName() {
    return 'black';
  }

  public function getLinterPriority() {
    return 0.5;
  }

  public function getDefaultBinary() {
    return 'black';
  }

  protected function getMandatoryFlags() {
    return array('--quiet', '--diff');
  }

  public function getVersion() {
    list($err, $stdout, $stderr) = exec_manual(
      '%C --version', $this->getExecutableCommand());

    $matches = array();
    if (preg_match('/version (?P<version>[^\s]+)$/', $stderr, $matches)) {
      $this->version = $matches['version'];
      return $this->version;
    } else {
      return false;
    }
  }

  public function getInstallInstructions() {
    return pht('pip3 install black');
  }

  public function getLintSeverityMap() {
    return array(
      self::LINT_STYLE => ArcanistLintSeverity::SEVERITY_AUTOFIX,
      self::LINT_ERROR => ArcanistLintSeverity::SEVERITY_ERROR,
    );
  }

  public function getLintNameMap() {
    return array(
      self::LINT_STYLE => pht('Black code style'),
      self::LINT_ERROR => pht('Black formatting error'),
    );
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    // A non-zero error code means an error occured.
    if ($err != 0) {
      $messages = array();
      $lines = phutil_split_lines($stderr, false);

      foreach ($lines as $line) {
        if (preg_match('/^error: (?:.*?): (.*)/', $line, $matches)) {
          $messages[] = id(new ArcanistLintMessage())
            ->setPath($path)
            ->setCode($this->getLintMessageFullCode(self::LINT_ERROR))
            ->setSeverity($this->getLintMessageSeverity(self::LINT_ERROR))
            ->setName($this->getLintMessageName(self::LINT_ERROR))
            ->setDescription($matches[1]);
        }
      }

      return $messages;
    }

    // No error code and no output means that the file is already formatted.
    if (empty($stdout)) {
      return array();
    }

    $messages = array();
    $parser = new ArcanistDiffParser();
    $changes = $parser->parseDiff($stdout);

    foreach ($changes as $change) {
      foreach ($change->getHunks() as $hunk) {
        $repl = array();
        $orig = array();

        $lines = phutil_split_lines($hunk->getCorpus(), false);
        foreach ($lines as $line) {
          if (empty($line)) {
            continue;
          }

          $char = $line[0];
          $rest = substr($line, 1);

          switch ($char) {
            case '-':
              $orig[] = $rest;
              break;

            case '+':
              $repl[] = $rest;
              break;

            case '~':
              break;

            case ' ':
              $orig[] = $rest;
              $repl[] = $rest;
              break;
          }
        }

        $messages[] = id(new ArcanistLintMessage())
          ->setPath($path)
          ->setLine($hunk->getOldOffset())
          ->setChar(1)
          ->setCode($this->getLintMessageFullCode(self::LINT_STYLE))
          ->setSeverity($this->getLintMessageSeverity(self::LINT_STYLE))
          ->setName($this->getLintMessageName(self::LINT_STYLE))
          ->setOriginalText(join("\n", $orig))
          ->setReplacementText(join("\n", $repl))
          ->setBypassChangedLineFiltering(true);
      }
    }

    return $messages;
  }
}
