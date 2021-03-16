<?php

final class ArcanistGoImportsLinter extends ArcanistLinter {

  const LINT_GO_IMPORTS = 0;

  public function getInfoName() {
    return 'Go imports';
  }

  public function getInfoURI() {
    return 'https://github.com/golang/tools';
  }

  public function getInfoDescription() {
    return pht(
      'Goimports formats Go imports.');
  }

  public function getLinterName() {
    return 'GOIMPORTS';
  }

  public function getLinterConfigurationName() {
    return 'goimports';
  }

  public function getBinary() {
    return 'goimports';
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
    return pht('Install Go imports using `go get golang.org/x/tools/cmd/goimports`');
  }

  public function getLintNameMap() {
    return array(
      self::LINT_GO_IMPORTS => "File is not goimports'd",
    );
  }

  // Run after gofmt but before go vet.
  // TODO: Examine if this order makes sense.
  public function getLinterPriority() {
    return 5;
  }

  public function lintPath($path) {
    $this->checkBinaryConfiguration();

    $data = $this->getData($path);
    $future = new ExecFuture('%C -local "pixielabs.ai"', $this->getBinary());
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
        self::LINT_GO_IMPORTS,
        pht(
          '%s was not formatted correctly. Please setup your '.
          'editor to run goimports on save', $path),
        implode("\n", $lines),
        implode("\n", $formatted_lines));
    }
  }

}
