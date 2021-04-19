<?php

/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
