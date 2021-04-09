<?php

final class GoGenerateCheckerTestEngine {
    private $project_root;
    private $files;

    public function __construct($project_root, $files) {
        $this->project_root = $project_root;
        $this->files = $files;
    }

    public function run() {
        chdir($this->project_root);

        $test_results = array();

        $go_generate = new ArcanistUnitTestResult();
        $go_generate->setName('Go-generated files up-to-date');

        // Get all the directories we should run go:generate in.
        exec('git grep -l "go:generate" -- "*.go"', $files, $return_var);
        foreach ($files as &$file) {
            exec('go generate ' . $this->project_root . "/" . $file, $output, $return_var);
        }

        // Checks to make sure there are no modified files after running go generate.
        //If there are, then the user needs to check in the updated files.
        exec('git ls-files -m', $file_output, $return_var);
        if (count($file_output) > 0) {
            $go_generate->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            $go_generate->setUserData('Please check in the updated generated files.');
        } else {
            $go_generate->setResult(ArcanistUnitTestResult::RESULT_PASS);
        }

        $test_results[] = $go_generate;

        return $test_results;
    }
}
