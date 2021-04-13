<?php

final class ArcanistGraphqlGenCheckerTestEngine extends ArcanistBaseGenCheckerTestEngine {
  public function getEngineConfigurationName() {
    return 'graphql-gen-checker';
  }

  public function run() {
    $test_results = array();

    foreach ($this->getPaths() as $file) {
      $schema_filename = substr($file, 0, -8).'.d.ts';
      $test_results[] = $this->checkFile($file, $schema_filename, 'To regenerate, run src/cloud/api/controller/schema/update.sh');
    }

    return $test_results;
  }
}
