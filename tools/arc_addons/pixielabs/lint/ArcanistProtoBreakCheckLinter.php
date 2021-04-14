<?php

final class ArcanistProtoBreakCheckLinter extends ArcanistExternalLinter {
  public function getInfoName() {
    return 'proto-break-check';
  }

  public function getInfoURI() {
    return 'https://github.com/uber/prototool#prototool-break-check';
  }

  public function getInfoDescription() {
    return 'proto-break-check looks for backward incompatible protobuf changes';
  }

  public function getLinterName() {
    return 'proto-break-check';
  }

  public function getLinterConfigurationName() {
    return 'proto-break-check';
  }

  public function getDefaultBinary() {
    return 'prototool';
  }

  public function getInstallInstructions() {
    return 'Install from github releases '.
      'https://github.com/uber/prototool/releases';
  }

  protected function getMandatoryFlags() {
    return array('--git-branch=main');
  }

  protected function buildFutures(array $paths) {
    $executable = $this->getExecutableCommand();
    $flags = $this->getCommandFlags();

    $futures = array();
    foreach ($paths as $path) {
      $future = new ExecFuture('%C break check %Ls %s', $executable, $flags, $path);
      $futures[$path] = $future;
      $future->setCWD($this->getProjectRoot());
    }
    return $futures;
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $messages = array();
    if ($err !== 0) {
      $message = id(new ArcanistLintMessage())
        ->setPath($path)
        ->setCode('breaking-change')
        ->setName('prototool')
        ->setDescription($stdout)
        ->setSeverity(ArcanistLintSeverity::SEVERITY_WARNING);

      $messages[] = $message;
    }
    return $messages;
  }

}
