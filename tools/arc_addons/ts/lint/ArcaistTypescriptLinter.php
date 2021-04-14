<?php

/* Modified by zasgar@ */
/*
 Copyright 2016-present The Material Motion Authors. All Rights Reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/** Uses tslint to lint typescript */
final class ArcaistTypescriptLinter extends ArcanistExternalLinter {

  private $project = null;
  private $config = null;

  public function getInfoName() {
    return 'tslint';
  }

  public function getInfoURI() {
    return '';
  }

  public function getInfoDescription() {
    return pht('Use tslint for processing specified files.');
  }

  public function getLinterName() {
    return 'tslint';
  }

  public function getLinterConfigurationName() {
    return 'tslint';
  }

  public function getDefaultBinary() {
    return 'tslint';
  }

  public function getInstallInstructions() {
    return pht('Install tslint with `npm install tslint typescript -g`');
  }

  public function shouldExpectCommandErrors() {
    return false;
  }

  protected function getDefaultMessageSeverity($code) {
    return ArcanistLintSeverity::SEVERITY_WARNING;
  }

  public function getVersion() {
    list($stdout) = execx('%C --version', $this->getExecutableCommand());

    $matches = array();
    $regex = '/(?P<version>\d+\.\d+\.\d+)/';
    if (preg_match($regex, $stdout, $matches)) {
      return $matches['version'];
    } else {
      return false;
    }
  }

  protected function getMandatoryFlags() {
    $flags = array(
      '--force',
      '--format',
      'json'
    );
    if ($this->project) {
      array_push($flags, '--project', $this->project);
    }
    if ($this->config) {
      array_push($flags, '--config', $this->config);
    }
    return $flags;
  }

  public function getLinterConfigurationOptions() {
    $options = array(
      'tslint.project' => array(
        'type' => 'optional string',
        'help' => pht(
          'The path to your tsconfig.json file. Will be provided as --project <path> to tslint.'),
      ),
      'tslint.config' => array(
        'type' => 'optional string',
        'help' => pht(
          'The path to your tslint.json file.'),
      ),
    );
    return $options + parent::getLinterConfigurationOptions();
  }

  public function setLinterConfigurationValue($key, $value) {
    switch ($key) {
      case 'tslint.project':
        $this->project = $value;
        return;
      case 'tslint.config':
        $this->config = $value;
        return;
      default:
        parent::setLinterConfigurationValue($key, $value);
        return;
    }
  }

  protected function parseLinterOutput($path, $err, $stdout, $stderr) {
    $output = json_decode($stdout, TRUE);
    $messages = array();
    foreach ($output as $warning) {
      $messages []= id(new ArcanistLintMessage())
        ->setPath($path)
        ->setLine($warning['startPosition']['line'] + 1)
        ->setChar($warning['startPosition']['character'] + 1)
        ->setCode($warning['ruleName'])
        ->setSeverity($this->getLintMessageSeverity($warning['ruleName']))
        ->setName('tslint violation')
        ->setDescription($warning['failure']);
    }
    return $messages;
  }
}
