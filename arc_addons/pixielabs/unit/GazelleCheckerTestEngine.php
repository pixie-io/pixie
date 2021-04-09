<?php

final class GazelleCheckerTestEngine {
    private $project_root;
    private $files;

    public function __construct($project_root, $files) {
        $this->project_root = $project_root;
        $this->files = $files;
    }

    public function run() {
        chdir($this->project_root);

        $test_results = array();

        // Check if there are any modified go files.
        $goFiles = array_filter($this->files, function($f) {
            return substr($f, -3) == '.go';
        });

        // Check if there are any modified build files.
        $buildFiles = array_filter($this->files, function($f) {
            return substr($f, -11) == 'BUILD.bazel';
        });

        // If there are no modified go/build files, there is no need to check gazelle.
        if (sizeof($goFiles) === 0 && sizeof($buildFiles) === 0) {
            return $test_results;
        }

        $gazelle_build = new ArcanistUnitTestResult();
        $gazelle_build->setName('Gazelle builds successfully');
        exec('make gazelle  2>&1', $output, $return_var);
        // Get the last line of the Gazelle run.
        // This should say "Build completed successfully" if it built successfully.
        $gazelle_status = $output[count($output) - 1];

        if (strpos($gazelle_status, 'Build completed successfully') !== false) {
            $gazelle_build->setResult(ArcanistUnitTestResult::RESULT_PASS);
        } else {
            $gazelle_build->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            $gazelle_build->setUserData('"make gazelle" did not build successfully: ' . $gazelle_status);
        }

        $test_results[] = $gazelle_build;

        $gazelle_updated = new ArcanistUnitTestResult();
        $gazelle_updated->setName('Gazelle updated');
        // Checks to make sure there are no modified files after running gazelle.
        //If there are, then the user needs to run gazelle and check in the updated files.
        exec('git ls-files -m', $file_output, $return_var);
        $updated = true;
        foreach ($file_output as $file) {
            if (strpos($file, 'BUILD.bazel') !== false) {
              $updated = false;
              break;
            }
        }

        if ($updated == true) {
            $gazelle_updated->setResult(ArcanistUnitTestResult::RESULT_PASS);
        } else {
            $gazelle_updated->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            $gazelle_updated->setUserData('Please run "make gazelle" and check in the updated build files.');
        }
        $test_results[] = $gazelle_updated;


        return $test_results;
    }
}
