<?php

final class PrototoolCheckerTest {
    private $project_root;
    private $files;
    private $go_modules = array(
        'github.com/gogo/protobuf' => 'com_github_gogo_protobuf'
    );
    private $vendorPath = '/tmp/pixie/vendor';

    public function __construct($project_root, $files) {
        $this->project_root = $project_root;
        $this->files = $files;
    }

    public function run() {
        $test_results = array();

        chdir($this->project_root);

        # Skip prototool if no .proto files are present, since prototool is time-consuming.
        $protoFiles = array_filter($this->files, function($f) {
            return substr($f, -6) == '.proto';
        });

        if (count($protoFiles) == 0) {
            return $test_results; 
        }

        // Build proto files.
        exec('bazel query "kind(\'go_proto_library rule\', //src/...)"', $output, $return_var);

        foreach ($output as $o) {
            exec('bazel build ' . $o . '> /dev/null 2>&1');
        }

        // Copy over vendor files to a tmp directory.
        exec('rm -rf ' . $this->vendorPath);
        foreach ($this->go_modules as $m => $p) {
            $cpModuleDir = $this->vendorPath . '/' . $m;
            exec('mkdir -p ' . dirname($cpModuleDir));

            // Copy over the directory from bazel-pixielabs/external/.
            exec(sprintf('cp -r %s/bazel-pixielabs/external/%s %s', $this->project_root, $p, $cpModuleDir));
            exec('chmod -R 755 '. $cpModuleDir);
        }

        exec('prototool break check . --git-branch master', $output, $return_var);
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
