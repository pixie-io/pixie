<?php

final class FileCheckerTestEngine {
    private $project_root;
    private $files;
    private $go_generate_map = array(
        'go-bindata' => '/(?<=-o=)(.*)(?=\.gen\.go)/',
        'mockgen' => '/(?<=-destination=)(.*)(?=\.gen\.go)/',
        'genny' => '/(?<=-out )(.*)(?=\.gen\.go)/'
    );

    public function __construct($project_root, $files) {
        $this->project_root = $project_root;
        $this->files = $files;
    }

    private function checkFile($file, $fileToCheck, $res, $fileExt, $instructions) {
        // Check that pb.go file is up-to-date.
        // Note that this check is only enforced after first check-in.
        // The initial check-in is not enforced, because generating a pb.go is optional.

        if (in_array($fileToCheck, $this->files) && file_exists($this->project_root . '/' . $fileToCheck)) {
            $updatedRes = new ArcanistUnitTestResult();
            if (filemtime($this->project_root . '/' . $fileToCheck) >= filemtime($this->project_root . '/' . $file)) {
                $updatedRes->setName($fileToCheck . ' is newer than the ' . $fileExt . ' file from which it was generated');
                $updatedRes->setResult(ArcanistUnitTestResult::RESULT_PASS);
            } else {
                $updatedRes->setUserData($instructions);
                $updatedRes->setName($fileToCheck . ' is older than the ' . $fileExt . ' file from which it was generated');
                $updatedRes->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            }
            $res[] = $updatedRes;
        } else if (file_exists($this->project_root . '/' . $fileToCheck)) {
            // Check that $fileToCheck exists.
            $existRes = new ArcanistUnitTestResult();
            $existRes->setName($fileToCheck . ' exists');
            $existRes->setResult(ArcanistUnitTestResult::RESULT_FAIL);
            $existRes->setUserData($fileToCheck . ' has not been added to the diff.');
            $res[] = $existRes;
        }

        return $res;
    }

    private function isGRPCWebProto($file) {
        $pbDir = substr($file, 0, strrpos($file, "/"));
        $buildFile = $pbDir . '/BUILD.bazel';
        $bazelFile = '//' . $pbDir . ':' . substr($file, strrpos($file, "/") + 1);

        $readFile = fopen($buildFile, "r");
        $searchName = false;
        $isGRPCProto = false;
        while(!feof($readFile))
          {
            $line = fgets($readFile);
            if ($searchName) { // Previous line was pl_grpc_web_library(.
                // Get name of target.
                $matches = null;
                preg_match('/name = "(.*?)"/', $line, $matches);
                $target = '//' . $pbDir . ':' . $matches[1];

                // Check the dependencies of the grpc_web_library target and verify whether
                // the current file is a source.
                exec('bazel query \'kind("source file", deps(\'' . $target . '\'))\'', $file_output, $return_var);
                foreach ($file_output as $srcFile) {
                    if ($srcFile == $bazelFile) {
                        $isGRPCProto = true;
                        break;
                    }
                }

                if ($isGRPCProto) {
                    break;
                }
                $searchName = false;
            }
            if (strpos($line, "pl_grpc_web_library(") === 0) {
                // If the line begins with pl_grpc_web_library, we search the next line for the name.
                $searchName = true;
            }
          }
        fclose($readFile);

        return $isGRPCProto;
    }

    public function run() {
        $currDir = getcwd();
        chdir($this->project_root);

        $test_results = array();

        // Filter out files in the experimental directory.
        $this->files = array_filter($this->files, function($f) {
            return strpos($this->project_root . '/' . $f, 'experimental/') == false;
        });

        // Filter out deleted files.
        $this->files = array_filter($this->files, function($f) {
            return file_exists($this->project_root . '/' . $f);
        });

        // Check that .proto files have corresponding .pb.go files.
        $protoFiles = array_filter($this->files, function($f) {
            return substr($f, -6) == '.proto';
        });

        foreach ($protoFiles as &$file) {
            $pbFilename = substr($file,0,-6) . '.pb.go';
            $test_results = $this->checkFile($file, $pbFilename, $test_results, '.proto', 'To regenerate, run this command:' .
                    'python $(bazel info workspace)/scripts/update_go_protos.sh');

            if ($this->isGRPCWebProto($file)) {
                // Check generated files exist. We assume they are all in src/ui/src/types/generated for now.
                $fname = substr($file, strrpos($file, "/")+ 1, -6);
                // TODO(nick): Not all of these are in use in the main UI code anymore. Only check for the ones we need.
                // Check $fname_pb.d.ts.
                $test_results = $this->checkFile($file, 'src/ui/src/types/generated/' . $fname . '_pb.d.ts', $test_results, '', 'To regenerate, build the grpc_web target and move the files to the correct directory');
                // Check $fname_pb.js.
                $test_results = $this->checkFile($file, 'src/ui/src/types/generated/' . $fname . '_pb.js', $test_results, '', 'To regenerate, build the grpc_web target and move the files to the correct directory');
                // Check $fname_pb.d.ts in the pixie-api package.
                $test_results = $this->checkFile($file, 'src/ui/packages/pixie-api/src/types/generated/' . $fname . '_pb.d.ts', $test_results, '', 'To regenerate, build the grpc_web target and move the files to the correct directory');
                // Check $fname_pb.js in the pixie-api package.
                $test_results = $this->checkFile($file, 'src/ui/packages/pixie-api/src/types/generated/' . $fname . '_pb.js', $test_results, '', 'To regenerate, build the grpc_web target and move the files to the correct directory');
                // Check $fnameServiceClientPb.ts.
                // TODO(michelle): Figure out a way to make this check smarter for non-grpc protos.
                // $test_results = $this->checkFile($file, 'src/ui/src/types/generated/' . ucfirst($fname) . 'ServiceClientPb.ts', $test_results, '', 'To regenerate, build the grpc_web  target and move the files to the correct directory');
            }
        }

        // Check that .graphql files have corresponding .schema.d.ts files.
        $gqlFiles = array_filter($this->files, function($f) {
            return substr($f, -8) == '.graphql';
        });

        foreach ($gqlFiles as &$file) {
            $schemaFilename = substr($file,0,-8) . '.d.ts';
            $test_results = $this->checkFile($file, $schemaFilename, $test_results, '.graphql', 'To regenerate, run update.sh in //src/cloud/api/controller/schema');
        }

        // Check .go files that may need a .gen.go file.
        $goFiles = array_filter($this->files, function($f) {
            return substr($f, -3) == '.go';
        });

        foreach($goFiles as &$file) {
            $genGoFile = false;
            // Find if the .go file contains //go:generate.
            foreach(file($this->project_root . '/' . $file) as $fli=>$fl) {
                if(strpos($fl, '//go:generate') !== false) {
                    $command = preg_split('/\s+/', $fl)[1];
                    if (array_key_exists($command, $this->go_generate_map)) {
                        $genGoFile = true;
                        // Find the name of the .gen.go output file.
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

        chdir($currDir);
        return $test_results;
    }
}
