<?php

final class ExpCheckerTest {
    private $project_root;
    private $files;

    public function __construct($project_root, $files) {
        $this->project_root = $project_root;
        $this->files = $files;
    }

    private function checkFile($file, $res) {
        $buildRes = new ArcanistUnitTestResult();
        $buildRes->setName($file . ' contains only non-experimental PL versions of build rules');

        // Read the file to check for pl_go and pl_cc.
        $readFile = fopen($file,"r");
        $failed = false;
        while(!feof($readFile))
          {
            $line = fgets($readFile);
            if (strpos($line, "load(//") == 0) {
                // If the line begins with 'load("//', check to see if it contains any pl_cc/pl_go rules.
                if (strpos($line, "pl_go") !== false || strpos($line, "pl_cc") !== false) {
                    $failed = true;
                    break;
                }

            }
          }
        fclose($readFile);

        if ($failed) {
          $buildRes->setResult(ArcanistUnitTestResult::RESULT_FAIL);
                    $buildRes->setUserData($file . ' should use pl_exp_cc_* and pl_exp_go_*.');
        } else {
            $buildRes->setResult(ArcanistUnitTestResult::RESULT_PASS);
        }

        $res[] = $buildRes;
        return $res;
    }

    public function run() {
        $test_results = array();

        // Filter out deleted files.
        $this->files = array_filter($this->files, function($f) {
            return file_exists($this->project_root . '/' . $f);
        });

        // Filter to only BUILD.bazel files in the experimental directory.
        $this->files = array_filter($this->files, function($f) {
            return strpos($f, 'experimental') == 0 && substr($f, -11) == 'BUILD.bazel';
        });

        foreach ($this->files as &$file) {
            $test_results = $this->checkFile($file, $test_results);
        }

        return $test_results;
    }
}
