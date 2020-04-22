package cmd

import (
	"errors"
	"io/ioutil"
	"os"
	"path/filepath"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle.json"

func mustCreateBundleReader() *script.BundleManager {
	br, err := createBundleReader()
	if err != nil {
		log.WithError(err).Fatal("Failed to load bundle scripts")
	}
	return br
}

func createBundleReader() (*script.BundleManager, error) {
	bundleFile := viper.GetString("bundle")
	if bundleFile == "" {
		bundleFile = defaultBundleFile
	}
	br, err := script.NewBundleManager(bundleFile)
	if err != nil {
		return nil, err
	}
	return br, nil
}

func listBundleScripts(br *script.BundleManager, format string) {
	w := components.CreateStreamWriter(format, os.Stdout)
	defer w.Finish()
	w.SetHeader("script_list", []string{"Name", "Description"})
	for _, script := range br.GetScripts() {
		w.Write([]interface{}{script.ScriptName, script.ShortDoc})
	}
}

func loadScriptFromFile(path string) (*script.ExecutableScript, error) {
	var qb []byte
	var err error
	if path == "-" {
		// Read from STDIN.
		qb, err = ioutil.ReadAll(os.Stdin)
		if err != nil {
			panic(err)
		}
	} else {
		r, err := os.Open(path)
		if err != nil {
			return nil, err
		}

		qb, err = ioutil.ReadAll(r)
		if err != nil {
			return nil, err
		}
	}

	if len(qb) == 0 {
		return nil, errors.New("script string is empty")
	}
	scriptName := "stdin_script"
	if path != "-" {
		scriptName = filepath.Base(path)
	}
	return &script.ExecutableScript{
		ScriptName:   scriptName,
		ScriptString: string(qb),
		ShortDoc:     "Script supplied by user",
		LongDoc:      "Script supplied by user",
	}, nil
}
