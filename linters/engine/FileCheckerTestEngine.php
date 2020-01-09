<?php

final class FileCheckerTest {
    private $project_root;
    private $files;
    private $go_generate_map = array(
        'go-bindata' => '/(?<=-o=)(.*)(?=\.gen\.go)/',
        'mockgen' => '/(?<=-destination=)(.*)(?=\.gen\.go)/'
    );

    public function __construct($project_root, $files) {
        $this->project_root = $project_root;
        $this->files = $files;
    }

    private function checkFile($file, $fileToCheck, $res, $fileExt, $instructions) {
        # Check that $fileToCheck exists.
        $existRes = new ArcanistUnitTestResult();

        $existRes->setName($fileToCheck . ' exists');

        if (in_array($fileToCheck, $this->files) && file_exists($this->project_root . '/' . $fileToCheck)) {
            $existRes->setResult(ArcanistUnitTestResult::RESULT_PASS);

            # Check that .pb.go is up-to-date.
            $updatedRes = new ArcanistUnitTestResult();
            $updatedRes->setName($fileToCheck . ' is older than the ' . $fileExt . ' file from which it was generated');
            if (filemtime($this->project_root . '/' . $fileToCheck) >= filemtime($this->project_root . '/' . $file)) {
                $updatedRes->setResult(ArcanistUnitTestResult::RESULT_PASS);
            } else {
                $updatedRes->setUserData($instructions);
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
        $test_results = array();

        // Filter out files in the experimental directory.
        $this->files = array_filter($this->files, function($f) {
            return strpos($this->project_root . '/' . $f, 'experimental/') == false;
        });

        // Filter out deleted files.
        $this->files = array_filter($this->files, function($f) {
            return file_exists($this->project_root . '/' . $f);
        });

        # Check that .proto files have corresponding .pb.go files.
        $protoFiles = array_filter($this->files, function($f) {
            return substr($f, -6) == '.proto';
        });

        # Check that .graphql files have corresponding .schema.d.ts files.
        $gqlFiles = array_filter($this->files, function($f) {
            return substr($f, -8) == '.graphql';
        });        

        foreach ($protoFiles as &$file) {
            $pbFilename = substr($file,0,-6) . '.pb.go';
            $test_results = $this->checkFile($file, $pbFilename, $test_results, '.proto', 'To regenerate, run this command:' .
                    'python $(bazel info workspace)/scripts/update_go_protos.sh');
        }

        foreach ($gqlFiles as &$file) {
            $schemaFilename = substr($file,0,-8) . '.d.ts';
            $test_results = $this->checkFile($file, $schemaFilename, $test_results, '.graphql', 'To regenerate, run this command in the directory:' .
                    'graphql-schema-typescript generate-ts schema.graphql --output schema.d.ts');
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
                    $command = preg_split('/\s+/', $fl)[1];
                    if (array_key_exists($command, $this->go_generate_map)) {
                        $genGoFile = true;
                        # Find the name of the .gen.go output file.
                        $matches = array();
                        preg_match($this->go_generate_map[$command], $fl, $matches);
                        $genGoFilename = substr($file, 0, strrpos($file, '/') + 1) . $matches[0] . '.gen.go';
                        break;
                    } else {
                        print('go:generate command ' . $command . ' has not been added to go_generate_map. Please add' .
                            ' an entry in $go_generate_map in linters/engine/FileCheckerTestEngine.php, ' .
                            'where the key is ' . $command . ' and the value is a regex for the name of the' .
                            ' generated output file.');
                        break;
                    }
                }
            }
            if ($genGoFile) {
                $test_results = $this->checkFile($file, $genGoFilename, $test_results, '.go', 'To regenerate, run "go generate" in the appropriate directory.');
            }
        }

        return $test_results;
    }
}
