<?php

final class ArcanistGoFmtLinter extends ArcanistLinter {

  const LINT_GO_UNFORMATTED = 0;

  public function getInfoName() {
    return 'Go fmt';
  }

  public function getInfoURI() {
    return 'https://golang.org/doc/install';
  }

  public function getInfoDescription() {
    return pht(
      'Gofmt formats Go programs. It uses tabs (width = 8) '.
      'for indentation and blanks for alignment.');
  }

  public function getLinterName() {
    return 'GOFMT';
  }

  public function getLinterConfigurationName() {
    return 'gofmt';
  }

  public function getBinary() {
    return 'gofmt';
  }

  protected function checkBinaryConfiguration() {
    $binary = $this->getBinary();
    if (!Filesystem::binaryExists($binary)) {
      throw new ArcanistMissingLinterException(
        sprintf(
          "%s\n%s",
          pht(
            'Unable to locate binary "%s" to run linter %s. You may need '.
            'to install the binary, or adjust your linter configuration.',
            $binary,
            get_class($this)),
          pht(
            'TO INSTALL: %s',
            $this->getInstallInstructions())));
    }
  }

  public function getInstallInstructions() {
    return pht(
      'Gofmt comes with Go, please '.
      'follow https://golang.org/doc/install to install go');
  }

  public function getLintNameMap() {
    return array(
      self::LINT_GO_UNFORMATTED => "File is not gofmt'd",
    );
  }

  // Gofmt must run before any other linter for Go.
  public function getLinterPriority() {
    return 10;
  }

  public function lintPath($path) {
    $this->checkBinaryConfiguration();

    $data = $this->getData($path);
    $future = new ExecFuture('%C', $this->getBinary());
    $future->write($data);
    list($err, $stdout, $stderr) = $future->resolve();
    if (empty($stdout) && $err) {
      throw new Exception(
        sprintf(
          "%s\n\nSTDOUT\n%s\n\nSTDERR\n%s",
          pht($this->getLinterName() . ' failed to parse output!'),
          $stdout,
          $stderr));
    }

    if ($stdout !== $data) {
      $lines = explode("\n", $data);
      $formatted_lines = explode("\n", $stdout);
      foreach ($lines as $line_idx => $line) {
        if ($line != $formatted_lines[$line_idx]) {
          $lines = array_slice($lines, $line_idx);
          $formatted_lines = array_slice($formatted_lines, $line_idx);
          break;
        }
      }

      $this->raiseLintAtLine(
        $line_idx + 1,
        1,
        self::LINT_GO_UNFORMATTED,
        pht(
          '%s was not formatted correctly. Please setup your '.
          'editor to run gofmt on save', $path),
        implode("\n", $lines),
        implode("\n", $formatted_lines));
    }
  }

}
