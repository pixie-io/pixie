<?php

/**
 * Uses the clang format to format C/C++/Obj-C code
 */
final class ClangFormatLinter extends ArcanistExternalLinter {

  public function getInfoName() {
    return 'clang-format';
  }

  public function getInfoURI() {
    return '';
  }

  public function getInfoDescription() {
    return pht('Use clang-format for processing specified files.');
  }

  public function getLinterName() {
    return 'clang-format';
  }

  public function getLinterConfigurationName() {
    return 'clang-format';
  }

  public function getLinterConfigurationOptions() {
    $options = array(
    );

    return $options + parent::getLinterConfigurationOptions();
  }

  public function getDefaultBinary() {
    return 'clang-format';
  }

  public function getInstallInstructions() {
    return pht('Make sure clang-format is in directory specified by $PATH');
  }

  public function shouldExpectCommandErrors() {
    return false;
  }

  protected function getMandatoryFlags() {
    return array(
    );
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $ok = ($err == 0);

    if (!$ok) {
      return false;
    }

    $root = $this->getProjectRoot();
    $path = Filesystem::resolvePath($path, $root);
    $orig = file_get_contents($path);
    if ($orig == $stdout) {
      return array();
    }

    $message = id(new ArcanistLintMessage())
      ->setPath($path)
      ->setLine(1)
      ->setChar(1)
      ->setGranularity(ArcanistLinter::GRANULARITY_FILE)
      ->setCode('CFMT')
      ->setSeverity(ArcanistLintSeverity::SEVERITY_AUTOFIX)
      ->setName('Code style violation')
      ->setDescription("'$path' has code style errors.")
      ->setOriginalText($orig)
      ->setReplacementText($stdout);
    return array($message);
  }
}
