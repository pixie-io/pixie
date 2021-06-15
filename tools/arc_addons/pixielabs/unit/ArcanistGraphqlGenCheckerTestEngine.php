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

final class ArcanistGraphqlGenCheckerTestEngine extends ArcanistBaseGenCheckerTestEngine {
  public function getEngineConfigurationName() {
    return 'graphql-gen-checker';
  }

  public function run() {
    $test_results = array();

    foreach ($this->getPaths() as $file) {
      if (!file_exists($this->getWorkingCopy()->getProjectRoot().DIRECTORY_SEPARATOR.$file)) {
        continue;
      }

      $test_results[] = $this->checkFile($file, 'src/ui/src/types/schema.ts', 'To regenerate, run src/cloud/api/controller/schema/update.sh');
      $test_results[] = $this->checkFile($file, 'src/cloud/api/controller/schema/complete/bindata.gen.go', 'To regenerate, run src/cloud/api/controller/schema/update.sh');
      $test_results[] = $this->checkFile($file, 'src/cloud/api/controller/schema/noauth/bindata.gen.go', 'To regenerate, run src/cloud/api/controller/schema/update.sh');
    }

    return $test_results;
  }
}
