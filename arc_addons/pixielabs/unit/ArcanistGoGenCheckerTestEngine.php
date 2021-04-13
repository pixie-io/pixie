<?php

final class ArcanistGoGenCheckerTestEngine extends ArcanistBaseGenCheckerTestEngine {
  private $goGenerateMap = array(
    'go-bindata' => '/(?<=-o=)(.*)(?=\.gen\.go)/',
    'mockgen' => '/(?<=-destination=)(.*)(?=\.gen\.go)/',
    'genny' => '/(?<=-out )(.*)(?=\.gen\.go)/',
  );

  public function getEngineConfigurationName() {
    return 'go-gen-checker';
  }

  public function shouldEchoTestResults() {
    // This func/flag is badly named. Setting it to false, says that we don't render results
    // and that ArcanistConfigurationDrivenUnitTestEngine should do so instead.
    return false;
  }

  public function run() {
    $test_results = array();

    foreach ($this->getPaths() as $file) {
      $file_path = $this->getWorkingCopy()->getProjectRoot().DIRECTORY_SEPARATOR.$file;

      // Find if the .go file contains //go:generate.
      foreach (file($file_path) as $line_num => $line) {
        if (strpos($line, '//go:generate') !== false) {
          $command = preg_split('/\s+/', $line)[1];

          if (!array_key_exists($command, $this->goGenerateMap)) {
            $res = new ArcanistUnitTestResult();
            $res->setName(get_class($this));
            $res->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            $res->setUserData('go:generate command '.$command.' has not been added to goGenerateMap. Please add'.
              ' an entry in $goGenerateMap in linters/engine/FileCheckerTestEngine.php, '.
              'where the key is '.$command.' and the value is a regex for the name of the'.
              ' generated output file.');
            $test_results[] = $res;
            break;
          }

          // Find the name of the .gen.go output file.
          $matches = array();
          preg_match($this->goGenerateMap[$command], $line, $matches);
          $gen_go_filename = substr($file, 0, strrpos($file, '/') + 1).$matches[0].'.gen.go';

          $test_results[] = $this->checkFile($file, $gen_go_filename, 'To regenerate, run "go generate" in the appropriate directory.');
          break;
        }
      }
    }

    return $test_results;
  }
}
