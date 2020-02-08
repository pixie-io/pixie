<?php
/**
 * Copyright 2019 Pinterest, Inc.
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
 * Lints Python imports using `isort`.
 */
final class PythonIsortLinter extends ArcanistExternalLinter {

  const LINT_STYLE = 1;

  private $version = null;

  public function getInfoName() {
    return 'isort';
  }

  public function getInfoDescription() {
    return pht('isort is a Python utility to sort imports.');
  }

  public function getInfoURI() {
    return 'https://github.com/timothycrosley/isort';
  }

  public function getLinterName() {
    return 'ISORT';
  }

  public function getLinterConfigurationName() {
    return 'isort';
  }

  public function getLinterPriority() {
    return 0.4;
  }

  public function getDefaultBinary() {
    return 'isort';
  }

  public function getVersion() {
    list($err, $stdout, $stderr) = exec_manual(
      '%C --version-number',
      $this->getExecutableCommand());

    $matches = array();
    if (preg_match('/^(?P<version>\d+\.\d+\.\d+)$/', $stderr, $matches)) {
      $this->version = $matches['version'];
      return $this->version;
    } else {
      return false;
    }
  }

  public function getInstallInstructions() {
    return pht('Install isort using `pip install isort`.');
  }

  protected function getMandatoryFlags() {
    $flags = array('--quiet', '--check-only', '--diff');
    if (version_compare($this->version, '4.3.18', '>=')) {
      $flags[] = '--filter-files';
    }
    $flags[] = '--';

    return $flags;
  }

  protected function getDefaultMessageSeverity($code) {
    return ArcanistLintSeverity::SEVERITY_AUTOFIX;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    if (empty($stdout)) {
        return array();
    }

    // Expected output includes a single header optionally followed by a
    // multiline diff. We're only interested in the latter case (which implies
    // a lint violation), but we could also parse the header for more detailed
    // information if we need that later.
    list($header, $diff) = explode("\n", $stdout, 2);
    if (empty($diff)) {
        return array();
    }

    $messages = array();
    $parser = new ArcanistDiffParser();
    $changes = $parser->parseDiff($diff);

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

          // We *could* differentiate between different kinds of violations
          // here (sorting, missing blank lines, too many blank lines), but
          // for now we just classify the entire chunk as a single generic
          // "style" issue.
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
          ->setCode($this->getLinterName())
          ->setSeverity($this->getLintMessageSeverity(self::LINT_STYLE))
          ->setName('Python imports style')
          ->setOriginalText(join("\n", $orig))
          ->setReplacementText(join("\n", $repl))
          ->setBypassChangedLineFiltering(true);
      }
    }

    if ($err && !$messages) {
      return false;
    }

    return $messages;
  }
}
