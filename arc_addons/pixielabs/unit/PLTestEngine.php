<?php

include 'FileCheckerTestEngine.php';
include 'GazelleCheckerTestEngine.php';
include 'GoGenerateCheckerTestEngine.php';

final class PLTestEngine extends ArcanistUnitTestEngine {
    private $project_root;
    private $files;

    public function run() {
        $this->project_root = $this->getWorkingCopy()->getProjectRoot();

        $test_results = array();

        $this->files = $this->getPaths();

        $file_checker = new FileCheckerTestEngine($this->project_root, $this->files);
        $test_results = array_merge($test_results, $file_checker->run());

        $gazelle_checker = new GazelleCheckerTestEngine($this->project_root, $this->files);
        $test_results = array_merge($test_results, $gazelle_checker->run());

        // TODO(michelle): Determine a more robust check.
        // $go_generate_checker = new GoGenerateCheckerTestEngine($this->project_root, $this->files);
        // $test_results = array_merge($test_results, $go_generate_checker->run());

        return $test_results;
    }
}
