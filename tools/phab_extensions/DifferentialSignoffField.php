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

final class DifferentialSignoffField extends DifferentialStoredCustomField {

  public function getFieldKey() {
    return 'signed-off-by';
  }

  public function getFieldName() {
    return 'Signed-off-by';
  }

  public function getFieldDescription() {
    return 'Indicates if this commit was signed off by the author for DCO purposes.';
  }

  public function getHeraldFieldStandardType() {
    return 'standard.text';
  }

  public function getHeraldFieldValueType($condition) {
    return new HeraldTextFieldValue();
  }

  public function getHeraldFieldConditions() {
    return array(
      HeraldAdapter::CONDITION_CONTAINS,
      HeraldAdapter::CONDITION_NOT_CONTAINS,
      HeraldAdapter::CONDITION_IS,
      HeraldAdapter::CONDITION_IS_NOT,
      HeraldAdapter::CONDITION_REGEXP,
    );
  }

  public function shouldAppearInHerald() {
    return true;
  }

  public function getHeraldFieldValue() {
    return $this->getValue();
  }

  public function shouldDisableByDefault() {
    return false;
  }

  public function shouldAppearInPropertyView() {
    return true;
  }

  public function renderPropertyViewLabel() {
    return $this->getFieldName();
  }

  public function renderPropertyViewValue(array $handles) {
    if (!strlen($this->getValue())) {
      return null;
    }

    return $this->getValue();
  }

  public function shouldAppearInApplicationTransactions() {
    return true;
  }

  public function getOldValueForApplicationTransactions() {
    return $this->getValue();
  }

  public function getNewValueForApplicationTransactions() {
    return $this->getValue();
  }

  public function shouldAppearInEditView() {
    return true;
  }

  public function renderEditControl(array $handles) {
    return id(new AphrontFormTextControl())
      ->setName($this->getFieldKey())
      ->setValue($this->getValue())
      ->setLabel($this->getFieldName());
  }

  public function readValueFromRequest(AphrontRequest $request) {
    $this->setValue($request->getStr($this->getFieldKey()));
  }

  public function shouldAppearInConduitDictionary() {
    return true;
  }

  public function shouldAppearInConduitTransactions() {
    return true;
  }

  protected function newConduitEditParameterType() {
    return new ConduitStringParameterType();
  }
}
