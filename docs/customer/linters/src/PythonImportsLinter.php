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
 * Lints for illegal Python module imports.
 */
final class PythonImportsLinter extends ArcanistLinter {

  const LINT_IMPORT = 1;

  private $pattern;
  private $message;

  public function getInfoName() {
    return 'Python Imports Linter';
  }

  public function getInfoDescription() {
    return pht('Lints for illegal Python module imports.');
  }

  public function getLinterName() {
    return 'PYTHON-IMPORTS';
  }

  public function getLinterConfigurationName() {
    return 'python-imports';
  }

  public function getLintNameMap() {
    return array(
      self::LINT_IMPORT => pht('Illegal Import'),
    );
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'python-imports.pattern' => array(
        'type' => 'string',
        'help' => pht('Module name pattern.'),
      ),
      'python-imports.message' => array(
        'type' => 'optional string',
        'help' => pht('Message to display when caught.'),
      ),
    );

    return $options + parent::getLinterConfigurationOptions();
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'python-imports.pattern':
        $this->pattern = $value;
        return;
      case 'python-imports.message':
        $this->message = $value;
        return;

      default:
        return parent::setLinterConfigurationValue($key, $value);
    }
  }

  public function lintPath($path) {
    $lines = phutil_split_lines($this->getData($path), false);
    $regex = "/^\s*(from|import)\s+(?P<module>{$this->pattern})\b/";

    foreach ($lines as $lineno => $line) {
      $matches = array();
      if (preg_match($regex, $line, $matches, PREG_OFFSET_CAPTURE)) {
        list($module, $offset) = $matches['module'];
        $this->raiseLintAtLine(
          $lineno + 1,
          $offset + 1,
          self::LINT_IMPORT,
          pht(
              'This line imports a module ("%s") that is not allowed in '.
              'this file. %s',
              $module, $this->message),
          $module);
      }
    }
  }
}
