<?php

final class ArcanistGoImportsLinter extends ArcanistExternalLinter {
  public function getInfoName() {
    return 'Go imports';
  }

  public function getInfoURI() {
    return 'https://github.com/golang/tools';
  }

  public function getInfoDescription() {
    return 'Goimports formats Go imports.';
  }

  public function getLinterName() {
    return 'GOIMPORTS';
  }

  public function getLinterConfigurationName() {
    return 'goimports';
  }

  public function getDefaultBinary() {
    return 'goimports';
  }

  public function getInstallInstructions() {
    return 'Install Go imports using '.
      '`go get -u golang.org/x/tools/cmd/goimports`';
  }

  // Run before go vet.
  public function getLinterPriority() {
    return 2.0;
  }

  protected function getMandatoryFlags() {
    return array('-local=pixielabs.ai');
  }

  protected function buildFutures(array $paths) {
    $executable = $this->getExecutableCommand();
    $flags = $this->getCommandFlags();

    $futures = array();
    foreach ($paths as $path) {
      $data = $this->getData($path);
      $future = new ExecFuture('%C %Ls', $executable, $flags);
      $future->write($data);
      $futures[$path] = $future;
    }
    return $futures;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    if (empty($stdout) && $err) {
      throw new Exception(
        sprintf(
          "%s failed to parse output!\n\nSTDOUT\n%s\n\nSTDERR\n%s",
          $this->getLinterName(),
          $stdout,
          $stderr
        )
      );
    }

    $data = $this->getData($path);

    $messages = array();
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

      $desc = sprintf(
          '%s was not formatted correctly. Please setup your '.
          'editor to run goimports on save', $path);

      $message = id(new ArcanistLintMessage())
        ->setPath($path)
        ->setLine($line_idx + 1)
        ->setChar(1)
        ->setCode('E00')
        ->setName('goimports')
        ->setDescription($desc)
        ->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR)
        ->setOriginalText(implode("\n", $lines))
        ->setReplacementText(implode("\n", $formatted_lines));

      $messages[] = $message;
    }
    return $messages;
  }

}
