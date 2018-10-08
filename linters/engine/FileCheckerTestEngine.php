<?php

final class FileCheckerTestEngine extends ArcanistUnitTestEngine {
    private $project_root;
    private $files;

    private function checkFile($file, $fileToCheck, $res) {
        # Check that $fileToCheck exists.
        $existRes = new ArcanistUnitTestResult();

        $existRes->setName($fileToCheck . ' exists');

        if (in_array($fileToCheck, $this->files) && file_exists($this->project_root . '/' . $fileToCheck)) {
            $existRes->setResult(ArcanistUnitTestResult::RESULT_PASS);

            # Check that .pb.go is up-to-date.
            $updatedRes = new ArcanistUnitTestResult();
            $updatedRes->setName($fileToCheck . ' up-to-date');
            if (filemtime($this->project_root . '/' . $fileToCheck) > filemtime($this->project_root . '/' . $file)) {
                $updatedRes->setResult(ArcanistUnitTestResult::RESULT_PASS);
            } else {
                $updatedRes->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            }
            $res[] = $updatedRes;

        } else {
            $existRes->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            $existRes->setUserData($fileToCheck . ' has not been added to the diff.');
        }
        $res[] = $existRes;
        return $res;
    }

    public function run() {
        $this->project_root = $this->getWorkingCopy()->getProjectRoot();

        $test_results = array();

        $this->files = $this->getPaths();

        # Check that .proto files have corresponding .pb.go files.
        $protoFiles = array_filter($this->files, function($f) {
            return substr($f, -6) == '.proto';
        });

        foreach ($protoFiles as &$file) {
            $pbFilename = substr($file,0,-6) . '.pb.go';
            $test_results = $this->checkFile($file, $pbFilename, $test_results);
        }

        # Check .go files that may need a .gen.go file.
        $goFiles = array_filter($this->files, function($f) {
            return substr($f, -3) == '.go';
        });

        foreach($goFiles as &$file) {
            $genGoFile = false;
            # Find if the .go file contains //go:generate.
            foreach(file($this->project_root . '/' . $file) as $fli=>$fl) {
                if(strpos($fl, '//go:generate') !== false) {
                    $genGoFile = true;

                    # Find the name of the .gen.go output file.
                    $matches = array();
                    preg_match('/(?<=-o=)(.*)(?=\.gen\.go)/', $fl, $matches);
                    $genGoFilename = substr($file, 0, strrpos($file, '/') + 1) . $matches[0] . '.gen.go';
                    break;
                }
            }
            if ($genGoFile) {
                $test_results = $this->checkFile($file, $genGoFilename, $test_results);
            }
        }

        return $test_results;
    }
}
