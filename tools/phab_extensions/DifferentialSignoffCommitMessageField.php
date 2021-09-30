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

final class DifferentialSignoffCommitMessageField extends DifferentialCommitMessageCustomField {

  const FIELDKEY = 'signed-off-by';

  public function getFieldName() {
    return 'Signed-off-by';
  }

  public function getCustomFieldKey() {
    return 'signed-off-by';
  }

  public function getFieldOrder() {
    return 110000;
  }

  public function isFieldEditable() {
    return true;
  }

  public function isTemplateField() {
    return true;
  }

  public function validateFieldValue($value) {
    if (!strlen($value)) {
      $this->raiseValidationException(
        'You must signoff commits. Use `-s|--signoff` when creating commits.'
      );
    }

    $ok = preg_match_all('/^(.*) <(.*)>$/', $value, $matches);
    if (!$ok || count($matches) < 3) {
      $this->raiseValidationException(
        'Commit signoff invalid. Expected format: `Signed-off-by: AUTHOR_NAME <AUTHOR_EMAIL>`'
      );
    }

    $email = $matches[2][0];
    if(!filter_var($email, FILTER_VALIDATE_EMAIL)) {
      $this->raiseValidationException(
        'Commit signoff email invalid. Expected format: `Signed-off-by: AUTHOR_NAME <AUTHOR_EMAIL>`'
      );
    }
  }
}
