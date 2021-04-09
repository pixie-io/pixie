<?php

final class ArcanistGoFmtLinter extends ArcanistExternalLinter {
  public function getInfoName() {
    return 'Go fmt';
  }

  public function getInfoURI() {
    return 'https://golang.org/doc/install';
  }

  public function getInfoDescription() {
    return 'Gofmt formats Go programs. It uses tabs (width = 8) '.
      'for indentation and blanks for alignment.';
  }

  public function getLinterName() {
    return 'GOFMT';
  }

  public function getLinterConfigurationName() {
    return 'gofmt';
  }

  public function getDefaultBinary() {
    return 'gofmt';
  }

  public function getInstallInstructions() {
    return 'Gofmt comes with Go, please '.
      'follow https://golang.org/doc/install to install go';
  }

  // Gofmt must run before any other linter for Go.
  public function getLinterPriority() {
    return 10;
  }

  protected function buildFutures(array $paths) {
    $executable = $this->getExecutableCommand();

    $futures = array();
    foreach ($paths as $path) {
      $data = $this->getData($path);
      $future = new ExecFuture('%C', $executable);
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
          'editor to run gofmt on save', $path);

      $message = id(new ArcanistLintMessage())
        ->setPath($path)
        ->setLine($line_idx + 1)
        ->setChar(1)
        ->setCode('E00')
        ->setName('gofmt')
        ->setDescription($desc)
        ->setSeverity(ArcanistLintSeverity::SEVERITY_ERROR)
        ->setOriginalText(implode("\n", $lines))
        ->setReplacementText(implode("\n", $formatted_lines));

      $messages[] = $message;
    }
    return $messages;
  }
}
