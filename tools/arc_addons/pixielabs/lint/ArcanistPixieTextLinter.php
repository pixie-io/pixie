<?php

/*
 * Copyright 2012 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Enforces basic text file rules. Modified from the original to disable
 * checking of the tab literal and line length.
 */
final class ArcanistPixieTextLinter extends ArcanistLinter {

  const LINT_DOS_NEWLINE          = 1;
  const LINT_TAB_LITERAL          = 2;
  const LINT_LINE_WRAP            = 3;
  const LINT_EOF_NEWLINE          = 4;
  const LINT_BAD_CHARSET          = 5;
  const LINT_TRAILING_WHITESPACE  = 6;
  const LINT_BOF_WHITESPACE       = 8;
  const LINT_EOF_WHITESPACE       = 9;
  const LINT_EMPTY_FILE           = 10;

  private $maxLineLength = 80;

  public function getInfoName() {
    return pht('Basic Text Linter');
  }

  public function getInfoDescription() {
    return pht(
      'Enforces basic text rules like line length, character encoding, '.
      'and trailing whitespace.');
  }

  public function getLinterPriority() {
    return 0.5;
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'text.max-line-length' => array(
        'type' => 'optional int',
        'help' => pht(
          'Adjust the maximum line length before a warning is raised. By '.
          'default, a warning is raised on lines exceeding 80 characters.'),
      ),
    );

    return $options + parent::getLinterConfigurationOptions();
  }

  public function setMaxLineLength($new_length) {
    $this->maxLineLength = $new_length;
    return $this;
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'text.max-line-length':
        $this->setMaxLineLength($value);
        return;
    }

    return parent::setLinterConfigurationValue($key, $value);
  }

  public function getLinterName() {
    return 'PX_TXT';
  }

  public function getLinterConfigurationName() {
    return 'pxtext';
  }

  public function getLintSeverityMap() {
    return array(
      self::LINT_LINE_WRAP           => ArcanistLintSeverity::SEVERITY_WARNING,
      self::LINT_TRAILING_WHITESPACE => ArcanistLintSeverity::SEVERITY_AUTOFIX,
      self::LINT_BOF_WHITESPACE      => ArcanistLintSeverity::SEVERITY_AUTOFIX,
      self::LINT_EOF_WHITESPACE      => ArcanistLintSeverity::SEVERITY_AUTOFIX,
    );
  }

  public function getLintNameMap() {
    return array(
      self::LINT_DOS_NEWLINE         => pht('DOS Newlines'),
      self::LINT_TAB_LITERAL         => pht('Tab Literal'),
      self::LINT_LINE_WRAP           => pht('Line Too Long'),
      self::LINT_EOF_NEWLINE         => pht('File Does Not End in Newline'),
      self::LINT_BAD_CHARSET         => pht('Bad Charset'),
      self::LINT_TRAILING_WHITESPACE => pht('Trailing Whitespace'),
      self::LINT_BOF_WHITESPACE      => pht('Leading Whitespace at BOF'),
      self::LINT_EOF_WHITESPACE      => pht('Trailing Whitespace at EOF'),
      self::LINT_EMPTY_FILE          => pht('Empty File'),
    );
  }

  public function lintPath($path) {
    // $this->lintEmptyFile($path);

    if (!strlen($this->getData($path))) {
      // If the file is empty, don't bother; particularly, don't require
      // the user to add a newline.
      return;
    }

    if ($this->didStopAllLinters()) {
      return;
    }

    $this->lintNewlines($path);
    // $this->lintTabs($path);

    if ($this->didStopAllLinters()) {
      return;
    }

    // $this->lintCharset($path);

    if ($this->didStopAllLinters()) {
      return;
    }

    // $this->lintLineLength($path);
    $this->lintEOFNewline($path);
    $this->lintTrailingWhitespace($path);

    $this->lintBOFWhitespace($path);
    $this->lintEOFWhitespace($path);
  }

  protected function lintEmptyFile($path) {
    $data = $this->getData($path);

    // It is reasonable for certain file types to be completely empty,
    // so they are excluded here.
    switch ($filename = basename($this->getActivePath())) {
      case '__init__.py':
        return;

      default:
        if (strlen($filename) && $filename[0] == '.') {
          return;
        }
    }

    if (preg_match('/^\s*$/', $data)) {
      $this->raiseLintAtPath(
        self::LINT_EMPTY_FILE,
        pht("Empty files usually don't serve any useful purpose."));
      $this->stopAllLinters();
    }
  }

  protected function lintNewlines($path) {
    $data = $this->getData($path);
    $pos  = strpos($this->getData($path), "\r");

    if ($pos !== false) {
      $this->raiseLintAtOffset(
        0,
        self::LINT_DOS_NEWLINE,
        pht('You must use ONLY Unix linebreaks ("%s") in source code.', '\n'),
        $data,
        str_replace("\r\n", "\n", $data));

      if ($this->isMessageEnabled(self::LINT_DOS_NEWLINE)) {
        $this->stopAllLinters();
      }
    }
  }

  protected function lintTabs($path) {
    $pos = strpos($this->getData($path), "\t");
    if ($pos !== false) {
      $this->raiseLintAtOffset(
        $pos,
        self::LINT_TAB_LITERAL,
        pht('Configure your editor to use spaces for indentation.'),
        "\t");
    }
  }

  protected function lintLineLength($path) {
    $lines = explode("\n", $this->getData($path));

    $width = $this->maxLineLength;
    foreach ($lines as $line_idx => $line) {
      if (strlen($line) > $width) {
        $this->raiseLintAtLine(
          $line_idx + 1,
          1,
          self::LINT_LINE_WRAP,
          pht(
            'This line is %s characters long, but the '.
            'convention is %s characters.',
            new PhutilNumber(strlen($line)),
            $width),
          $line);
      }
    }
  }

  protected function lintEOFNewline($path) {
    $data = $this->getData($path);
    if (!strlen($data) || $data[strlen($data) - 1] != "\n") {
      $this->raiseLintAtOffset(
        strlen($data),
        self::LINT_EOF_NEWLINE,
        pht('Files must end in a newline.'),
        '',
        "\n");
    }
  }

  protected function lintCharset($path) {
    $data = $this->getData($path);

    $matches = null;
    $bad = '[^\x09\x0A\x20-\x7E]';
    $preg = preg_match_all(
      "/{$bad}(.*{$bad})?/",
      $data,
      $matches,
      PREG_OFFSET_CAPTURE);

    if (!$preg) {
      return;
    }

    foreach ($matches[0] as $match) {
      list($string, $offset) = $match;
      $this->raiseLintAtOffset(
        $offset,
        self::LINT_BAD_CHARSET,
        pht(
          'Source code should contain only ASCII bytes with ordinal '.
          'decimal values between 32 and 126 inclusive, plus linefeed. '.
          'Do not use UTF-8 or other multibyte charsets.'),
        $string);
    }

    if ($this->isMessageEnabled(self::LINT_BAD_CHARSET)) {
      $this->stopAllLinters();
    }
  }

  protected function lintTrailingWhitespace($path) {
    $data = $this->getData($path);

    $matches = null;
    $preg = preg_match_all(
      '/[[:blank:]]+$/m',
      $data,
      $matches,
      PREG_OFFSET_CAPTURE);

    if (!$preg) {
      return;
    }

    foreach ($matches[0] as $match) {
      list($string, $offset) = $match;
      $this->raiseLintAtOffset(
        $offset,
        self::LINT_TRAILING_WHITESPACE,
        pht(
          'This line contains trailing whitespace. Consider setting '.
          'up your editor to automatically remove trailing whitespace.'),
        $string,
        '');
    }
  }

  protected function lintBOFWhitespace($path) {
    $data = $this->getData($path);

    $matches = null;
    $preg = preg_match(
      '/^\s*\n/',
      $data,
      $matches,
      PREG_OFFSET_CAPTURE);

    if (!$preg) {
      return;
    }

    list($string, $offset) = $matches[0];
    $this->raiseLintAtOffset(
      $offset,
      self::LINT_BOF_WHITESPACE,
      pht(
        'This file contains leading whitespace at the beginning of the file. '.
        'This is unnecessary and should be avoided when possible.'),
      $string,
      '');
  }

  protected function lintEOFWhitespace($path) {
    $data = $this->getData($path);

    $matches = null;
    $preg = preg_match(
      '/(?<=\n)\s+$/',
      $data,
      $matches,
      PREG_OFFSET_CAPTURE);

    if (!$preg) {
      return;
    }

    list($string, $offset) = $matches[0];
    $this->raiseLintAtOffset(
      $offset,
      self::LINT_EOF_WHITESPACE,
      pht('This file contains unnecessary trailing whitespace.'),
      $string,
      '');
  }

}
