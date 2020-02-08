<?php
/**
 * Copyright 2016-2020 Pinterest, Inc.
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
 * Ensures Python package requirements are sorted and unique.
 */
final class PythonRequirementsLinter extends ArcanistLinter {

  const LINT_DUPLICATES = 1;
  const LINT_UNSORTED = 2;
  const LINT_UNPINNED = 3;

  public function getInfoName() {
    return 'Python requirements.txt Linter';
  }

  public function getInfoDescription() {
    return pht('Ensures package requirements are sorted and unique.');
  }

  public function getInfoURI() {
    return 'https://pip.readthedocs.org/en/latest/user_guide/#requirements-files';
  }

  public function getLinterName() {
    return 'REQUIREMENTS-TXT';
  }

  public function getLinterConfigurationName() {
    return 'requirements-txt';
  }

  public function getLintSeverityMap() {
    return array(
      self::LINT_DUPLICATES => ArcanistLintSeverity::SEVERITY_ERROR,
      self::LINT_UNSORTED => ArcanistLintSeverity::SEVERITY_WARNING,
      self::LINT_UNPINNED => ArcanistLintSeverity::SEVERITY_WARNING,
    );
  }

  public function getLintNameMap() {
    return array(
      self::LINT_DUPLICATES => pht('Duplicate package requirement'),
      self::LINT_UNSORTED => pht('Unsorted package requirement'),
      self::LINT_UNPINNED => pht('Unpinned package version'),
    );
  }

  private function parseRequirement($line) {
    # PEP 508 (https://www.python.org/dev/peps/pep-0508/)
    $regex = "/^(?P<name>[[:alnum:]][[:alnum:]-_.]*(?:\[[[:alnum:]]+\])?)".
             "(?:\s*(?P<cmp>(~=|==|!=|<=|>=|<|>|===))\s*".
             "(?P<version>[[:alnum:]-_.*+!]+))?".
             "(?:\s*;\s*(?P<environment>[^#]*))?/";

    $matches = array();
    if (preg_match($regex, $line, $matches)) {
      return $matches;
    }

    return null;
  }

  private function formatRequirement($req) {
    return sprintf("%s%s%s", $req['name'], $req['cmp'], $req['version']);
  }

  private function lintDuplicates(array $reqs) {
    $packages = array();

    foreach ($reqs as $lineno => $req) {
      $package = strtolower($req['name']);
      $environment = strtolower(idx($req, 'environment', ''));
      if (array_key_exists($package.$environment, $packages)) {
        $first = $packages[$package.$environment];
        $this->raiseLintAtLine(
          $lineno,
          1,
          self::LINT_DUPLICATES,
          pht(
            'This line contains a duplicate package requirement for "%s". '.
            'The first reference appears on line %d ("%s")',
            $package, $first[0], $this->formatRequirement($first[1])),
          $package);
      } else {
        $packages[$package.$environment] = array($lineno, $req);
      }
    }
  }

  private function lintUnsorted(array $reqs) {
    $last_lineno = 0;
    $last_package = null;
    $last_version = null;

    foreach ($reqs as $lineno => $req) {
      // Only require consecutive requirement lines to be ordered. If we're
      // skipping over some other lines, clear $last_package to start a new
      // ordered "section".
      if ($lineno > $last_lineno + 1) {
        $last_package = null;
        $last_version = null;
      }

      $package = $req['name'];
      $version = $req['version'];

      // If the package names aren't sorted, or if the versions aren't sorted
      // for the same package name (presumably with different environment
      // markers), raise a warning.
      if ((strnatcasecmp($package, $last_package) < 0) ||
          ((strnatcasecmp($package, $last_package) == 0) &&
           (strnatcasecmp($version, $last_version) < 0))) {
        $this->raiseLintAtLine(
          $lineno,
          1,
          self::LINT_UNSORTED,
          pht(
            "This line doesn't appear in sorted order. Please keep ".
            "package requirements ordered alphabetically."));
      }

      $last_lineno = $lineno;
      $last_package = $package;
      $last_version = $version;
    }
  }

  private function lintUnpinned(array $reqs) {
    foreach ($reqs as $lineno => $req) {
      if ($req['cmp'] != '==') {
        $this->raiseLintAtLine(
          $lineno,
          1,
          self::LINT_UNPINNED,
          pht(
            "This package requirement isn't pinned to an exact version. ".
            "Use the `==` operator to specify a version."));
      }
    }
  }

  public function lintPath($path) {
    $lines = phutil_split_lines($this->getData($path), false);
    $lines = array_map('trim', $lines);

    // Build a sparse array mapping line numbers to parsed requirements.
    $reqs = array();
    foreach ($lines as $lineno => $line) {
      // Ignore any lines containing `noqa` comments.
      if (preg_match("/#\s+noqa/i", $line)) {
        continue;
      }
      $req = $this->parseRequirement($line);
      if (!empty($req)) {
        $reqs[$lineno + 1] = $req;
      }
    }

    if ($this->isMessageEnabled(self::LINT_DUPLICATES)) {
      $this->lintDuplicates($reqs);
    }
    if ($this->isMessageEnabled(self::LINT_UNSORTED)) {
      $this->lintUnsorted($reqs);
    }
    if ($this->isMessageEnabled(self::LINT_UNPINNED)) {
      $this->lintUnpinned($reqs);
    }
  }
}
