<?php

final class PrototoolCheckerTestEngine {
    private $project_root;
    private $files;

    public function __construct($project_root, $files) {
        $this->project_root = $project_root;
        $this->files = $files;
    }

    public function run() {
        $test_results = array();

        chdir($this->project_root);

        // Skip prototool if no .proto files are present, since prototool is time-consuming.
        $protoFiles = array_filter($this->files, function($f) {
            return substr($f, -6) == '.proto';
        });

        if (count($protoFiles) == 0) {
            return $test_results;
        }

        $output = array();
        exec('prototool break check . --git-branch main --walk-timeout 10s', $output, $return_var);
        $updatedRes = new ArcanistUnitTestResult();
        $updatedRes->setName('Protobuf Breaking API check');
        if ($return_var == 0) {
            $updatedRes->setResult(ArcanistUnitTestResult::RESULT_PASS);
        } else {
            $updatedRes->setUserData(implode("\n", $output));
            $updatedRes->setResult(ArcanistUnitTestResult::RESULT_FAIL);
        }

        $test_results[] = $updatedRes;
        return $test_results;
    }
}
