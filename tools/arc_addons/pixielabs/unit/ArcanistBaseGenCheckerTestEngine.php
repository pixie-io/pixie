<?php

abstract class ArcanistBaseGenCheckerTestEngine extends ArcanistUnitTestEngine {
  public function checkFile($source_file, $file_to_check, $instructions) {
    $source_path = $this->getWorkingCopy()->getProjectRoot().DIRECTORY_SEPARATOR.$source_file;
    $check_path = $this->getWorkingCopy()->getProjectRoot().DIRECTORY_SEPARATOR.$file_to_check;

    $res = new ArcanistUnitTestResult();
    $res->setName(get_class($this).':'.$source_file);
    if (!file_exists($check_path)) {
      // Generating files is optional. Lack of a generated file means there isn't anything to check in.
      $res->setResult(ArcanistUnitTestResult::RESULT_SKIP);
      return $res;
    }

    if (filemtime($check_path) >= filemtime($source_path)) {
      $res->setUserData($file_to_check.' is newer than '.$source_file.' file from which it was generated');
      $res->setResult(ArcanistUnitTestResult::RESULT_PASS);
    } else {
      $res->setUserData($instructions."\n".$file_to_check.' is older than '.$source_file.' file from which it was generated');
      $res->setResult(ArcanistUnitTestResult::RESULT_FAIL);
    }
    return $res;
  }

  public function shouldEchoTestResults() {
    // This func/flag is badly named. Setting it to false, says that we don't render results
    // and that ArcanistConfigurationDrivenUnitTestEngine should do so instead.
    return false;
  }
}
