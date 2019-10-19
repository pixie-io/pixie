<?php

final class ArcanistShellCheckLinterTestCase extends PhutilTestCase {

  public function testGetVersion() {
    $linter = new ArcanistShellCheckLinter();
    $actualVersion = $linter->getVersion();

    $this->assertFalse($actualVersion === null, "The version can't be extracted from the binary output.");
    $this->assertTrue(strpos($actualVersion, '.') > -1, "The version doesn't match expected format: does not contain a dot.");
  }

}
