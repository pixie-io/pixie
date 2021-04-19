<?php

/*
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

final class ArcanistShellCheckLinterTestCase extends PhutilTestCase {

  public function testGetVersion() {
    $linter = new ArcanistShellCheckLinter();
    $actualVersion = $linter->getVersion();

    $this->assertFalse($actualVersion === null, "The version can't be extracted from the binary output.");
    $this->assertTrue(strpos($actualVersion, '.') > -1, "The version doesn't match expected format: does not contain a dot.");
  }

}
