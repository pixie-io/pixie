<?php

final class ArcanistGoVetLinter extends ArcanistExternalLinter {

  public function getInfoName() {
    return 'Go vet';
  }

  public function getInfoURI() {
    return 'https://godoc.org/golang.org/x/tools/cmd/vet';
  }

  public function getInfoDescription() {
    return 'Vet examines Go source code and reports suspicious constructs.';
  }

  public function getLinterName() {
    return 'GOVET';
  }

  public function getLinterConfigurationName() {
    return 'govet';
  }

  public function getDefaultBinary() {
    return 'go';
  }

  public function getInstallInstructions() {
    return 'Govet comes with Go, please '.
      'follow https://golang.org/doc/install to install go';
  }

  public function shouldExpectCommandErrors() {
    return true;
  }

  protected function canCustomizeLintSeverities() {
    return true;
  }

  protected function getMandatoryFlags() {
    return array('vet');
  }

  protected function buildFutures(array $paths) {
    $executable = $this->getExecutableCommand();
    $flags = $this->getCommandFlags();

    $futures = array();
    foreach ($paths as $path) {
      // Get the package path from the file path.
      $fPath = $this->getProjectRoot() . '/' . $path;
      $splitPkgPath = explode('/', $fPath);

      $pkgPath = join('/', array_slice($splitPkgPath, 0, sizeof($splitPkgPath) - 1)) . '/...';

      // Run go vet on the package path.
      $future = new ExecFuture('%s %Ls %s', $executable, $flags, $pkgPath);
      $future->setEnv(array('CGO_ENABLED' => 0));
      $futures[$path] = $future;
    }

    return $futures;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $lines = phutil_split_lines($stderr, false);

    $messages = array();
    foreach ($lines as $line) {
      if (substr($line, 0, 1) === '#') {
        continue;
      }

      $message = id(new ArcanistLintMessage())
        ->setPath($path)
        ->setName('govet')
        ->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR);

      $matches = explode(':', $line);
      if (count($matches) <= 3) {
        $message
          ->setCode('E01')
          ->setDescription($line);
      }

      if (count($matches) >= 4) {
        $pIdx = 0;
        $found = false;
        foreach ($matches as $idx=>$match) {
          if ($match === $path) {
            $pIDX = $idx;
            $found = true;
          }
        }

        if ($found && $pIdx < count($matches) - 2) {
          $l = $matches[$pIdx+1];
          $c = $matches[$pIdx+2];

          if (is_numeric($l) && is_numeric($c)) {
            $desc = ucfirst(trim(implode(':', array_slice($matches, $pIdx+3))));
            if (strlen($desc) === 0) {
              $desc = $line;
            }

            $message
              ->setLine($l)
              ->setChar($c)
              ->setCode('E02')
              ->setDescription($desc);
          } else {
            $message
              ->setCode('E03')
              ->setDescription($line);
          }
        } else {
          $message
            ->setCode('E04')
            ->setDescription($line);
        }
        $messages[] = $message;
      }
    }
    return $messages;
  }

}
