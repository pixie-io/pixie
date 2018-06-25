<?php

/**
 * Go Test Result Parsing utility
 *
 * (To generate test output, run something like: `go test -v`)
 */
final class ArcanistGoTestResultParser extends ArcanistTestResultParser {

  /**
   * Parse test results from Go test report
   * (e.g. `go test -v`)
   *
   * @param string $path Path to test
   * @param string $stdout the Stdout of the command.
   * @param string $stderr the Stderr of the command.
   *
   * @return array
   */
  public function parseTestResults($path, $stdout, $stderr = '') {
    $test_results = $stderr.$stdout;
    $test_results = explode("\n", $test_results);

    $results = array();
    // We'll get our full test case name at the end and add it back in
    $test_case_name = '';

    // Temp store for test case results (in case we run multiple test cases)
    $test_case_results = array();
    for ($i = 0; $i < count($test_results); $i++) {
      $line = $test_results[$i];

      if (strlen($line) >= 18
        && strncmp($line, '==================', 18) === 0
        && strncmp($test_results[$i + 1], 'WARNING: DATA RACE', 18) === 0) {
        // We have a race condition
        $i++; // Advance to WARNING: DATA RACE
        $reason = '';
        $test_name = '';

        // loop to collect all data and move to the === line
        while (strncmp($test_results[$i], '==================', 18) !== 0) {
          if (strncmp($test_results[$i], 'Goroutine', 9) === 0) {
            $meta = array();
            preg_match(
              '/^.*\.(?P<test_name>[^\.]+)$/',
              $test_results[$i + 1],
              $meta);
            $test_name = $meta['test_name'].' Race Detected';
          }
          $reason .= $test_results[$i++]."\n";

          // Are we out of lines?
          if ($i > count($test_results)) {
            return false;
          }
        }

        $result = new ArcanistUnitTestResult();
        $result->setName($test_name);
        $result->setResult(ArcanistUnitTestResult::RESULT_FAIL);
        $result->setUserData($reason);

        $test_case_results[] = $result;

        continue;
      }

      if (strncmp($line, '--- PASS', 8) === 0) {
        // We have a passing test
        $meta = array();
        preg_match(
          '/^--- PASS: (?P<test_name>.+) \((?P<time>.+)\s*s(?:econds)?\).*/',
          $line,
          $meta);

        $result = new ArcanistUnitTestResult();
        // For now set name without test case, we'll add it later
        $result->setName($meta['test_name']);
        $result->setResult(ArcanistUnitTestResult::RESULT_PASS);
        $result->setDuration((float)$meta['time']);

        $test_case_results[] = $result;

        continue;
      }

      if (strncmp($line, '--- FAIL', 8) === 0) {
        // We have a failing test
        $reason = trim($test_results[$i + 1]);
        $meta = array();
        preg_match(
          '/^--- FAIL: (?P<test_name>.+) \((?P<time>.+)\s*s(?:econds)?\).*/',
          $line,
          $meta);

        $result = new ArcanistUnitTestResult();
        $result->setName($meta['test_name']);
        $result->setResult(ArcanistUnitTestResult::RESULT_FAIL);
        $result->setDuration((float)$meta['time']);
        $result->setUserData($reason."\n");

        $test_case_results[] = $result;

        continue;
      }

      if (strncmp($line, 'ok', 2) === 0) {
        $meta = array();
        preg_match(
          '/^ok[\s\t]+(?P<test_name>\w.*)[\s\t]+(?P<time>.*)s.*/',
          $line,
          $meta);

        $test_case_name = str_replace('/', '::', $meta['test_name']);

        // Our test case passed
        // check to make sure we were in verbose (-v) mode
        if (empty($test_case_results)) {
          // We weren't in verbose mode
          // create one successful result for the whole test case
          $test_name = 'Go::TestCase::'.$test_case_name;

          $result = new ArcanistUnitTestResult();
          $result->setName($test_name);
          $result->setResult(ArcanistUnitTestResult::RESULT_PASS);
          $result->setDuration((float)$meta['time']);

          $results[] = $result;
        } else {
          $test_case_results = $this->fixNames(
            $test_case_results,
            $test_case_name);
          $results = array_merge($results, $test_case_results);
          $test_case_results = array();
        }

        continue;
      }

      if (strncmp($line, "FAIL\t", 5) === 0) {
        $meta = array();
        preg_match(
          '/^FAIL[\s\t]+(?P<test_name>\w.*)[\s\t]+.*/',
          $line,
          $meta);

        $test_case_name = str_replace('/', '::', $meta['test_name']);

        $test_case_results = $this->fixNames(
          $test_case_results,
          $test_case_name);
        $results = array_merge($results, $test_case_results);
        $test_case_results = array();

        continue;
      }
    }

    return $results;
  }

  private function fixNames($test_case_results, $test_case_name) {

    foreach ($test_case_results as &$result) {
      $test_name = $result->getName();
      $result->setName('Go::Test::'.$test_case_name.'::'.$test_name);
    }

    return $test_case_results;
  }

}
