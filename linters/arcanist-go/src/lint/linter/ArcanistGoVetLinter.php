<?php

final class ArcanistGoVetLinter extends ArcanistExternalLinter {

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
      list($err, $stdout, $stderr) = exec_manual('go tool vet');
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
    return ['tool', 'vet'];
  }

  protected function getDefaultMessageSeverity($code) {
    return ArcanistLintSeverity::SEVERITY_WARNING;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $lines = phutil_split_lines($stderr, false);

    $messages = array();
    foreach ($lines as $line) {
      $matches = explode(':', $line, 6);

      if (count($matches) === 6) {
        $message = new ArcanistLintMessage();
        $message->setPath($path);
        $message->setLine($matches[3]);
        $message->setChar($matches[4]);
        $code = "E00";
        $message->setCode($code);
        $message->setName($this->getLinterName());
        $message->setDescription(ucfirst(trim($matches[5])));
        $severity = $this->getLintMessageSeverity($code);
        $message->setSeverity($severity);

        $messages[] = $message;
      }

      if (count($matches) === 3) {
        $message = new ArcanistLintMessage();
        $message->setPath($path);
        $message->setLine($matches[1]);
        $code = "E01";
        $message->setCode($code);
        $message->setName($this->getLinterName());
        $message->setDescription(ucfirst(trim($matches[2])));
        $severity = $this->getLintMessageSeverity($code);
        $message->setSeverity($severity);

        $messages[] = $message;
      }
    }

    return $messages;
  }

}
