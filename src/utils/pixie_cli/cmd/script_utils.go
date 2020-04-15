package cmd

import (
	"errors"
	"io/ioutil"
	"os"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

func mustCreateBundleReader() *script.BundleReader {
	br, err := createBundleReader()
	if err != nil {
		log.WithError(err).Fatal("Failed to load bundle scripts")
	}
	return br
}

func createBundleReader() (*script.BundleReader, error) {
	bundleFile := viper.GetString("bundle")
	if bundleFile == "" {
		bundleFile = defaultBundleFile
	}
	br, err := script.NewBundleReader(bundleFile)
	if err != nil {
		return nil, err
	}
	return br, nil
}

func listBundleScripts(br *script.BundleReader, format string) {
	w := components.CreateStreamWriter(format, os.Stdout)
	defer w.Finish()
	w.SetHeader("script_list", []string{"Name", "Description"})
	for _, script := range br.GetScriptMetadata() {
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
	return script.NewExecutableScript(script.Metadata{
		ScriptName: "custom_input",
		ShortDoc:   "N/A",
		LongDoc:    "N/A",
		HasVis:     false,
	}, string(qb)), nil
}
