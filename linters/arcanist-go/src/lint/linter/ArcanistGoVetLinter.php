<?php

final class ArcanistGoVetLinter extends ArcanistLinter {

  public function getInfoName() {
    return 'Go vet';
  }

  public function getInfoURI() {
    return 'https://godoc.org/golang.org/x/tools/cmd/vet';
  }

  public function getInfoDescription() {
    return pht(
      'Vet examines Go source code and reports suspicious constructs.');
  }

  public function getLinterName() {
    return 'GOVET';
  }

  public function getLinterConfigurationName() {
    return 'govet';
  }

  public function getDefaultBinary() {
    $binary = 'go';
    if (Filesystem::binaryExists($binary)) {
      // Vet is only accessible through 'go vet' or 'go tool vet'
      // Let's manually try to find out if it's installed.
      list($err, $stdout, $stderr) = exec_manual('go vet');
      if ($err === 3) {
        throw new ArcanistMissingLinterException(
          sprintf(
            "%s\n%s",
            pht(
              'Unable to locate "go vet" to run linter %s. You may need '.
              'to install the binary, or adjust your linter configuration.',
              get_class($this)),
            pht(
              'TO INSTALL: %s',
              $this->getInstallInstructions())));
      }
    }

    return $binary;
  }

  public function getInstallInstructions() {
    return pht('Install Go vet using `go get golang.org/x/tools/cmd/vet`.');
  }

  public function shouldExpectCommandErrors() {
    return true;
  }

  protected function canCustomizeLintSeverities() {
    return true;
  }

  protected function getMandatoryFlags() {
    return ['vet'];
  }

  protected function getDefaultMessageSeverity($code) {
    return ArcanistLintSeverity::SEVERITY_WARNING;
  }

  public function lintPath($path) {
    // Get the package path from the file path.
    $fPath = $this->getProjectRoot() . '/' . $path;
    $splitPkgPath = explode('/', $fPath);

    $pkgPath = join("/", array_slice($splitPkgPath, 0, sizeof($splitPkgPath) - 1)) . '/...';

    // Run go vet on the package path.
    $future = new ExecFuture('go vet %s', $pkgPath);

    list($err, $stdout, $stderr) = $future->resolve();

    // Parse the output and raise lint errors as necessary.
    $messages = $this->parseLinterOutput($pkgPath, $err, $stdout, $stderr);
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $lines = phutil_split_lines($stderr, false);

    $messages = array();
    foreach ($lines as $line) {
      $matches = explode(':', $line, 6);

      if (count($matches) === 6) {
        if (is_numeric($matches[2]) && is_numeric($matches[3])) {
          $line = $matches[2];
          $char = $matches[3];
          $code = "E00";
          $desc = ucfirst(trim($matches[4]));

          $this->raiseLintAtLine(
            $line, $char, $code, $desc
          );
        } else if (is_numeric($matches[1]) && is_numeric($matches[2])) {
          $line = $matches[1];
          $char = $matches[2];
          $code = "E02";
          $desc = ucfirst(trim(implode(": ", array_slice($matches, 4))));

          $this->raiseLintAtLine(
            $line, $char, $code, $desc
          );
        }
      }

      if (count($matches) === 3) {
        $code = "E01";
        $desc = $matches[0] . ': ' . ucfirst(trim($matches[2]));
        $this->raiseLintAtPath(
          $code, $desc
        );
      }
    }

    return $messages;
  }

}
