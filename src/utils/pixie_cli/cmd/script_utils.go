package cmd

import (
	"errors"
	"io/ioutil"
	"os"
	"path/filepath"
	"sort"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-core.json"
const ossBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-oss.json"

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
	br, err := script.NewBundleManager([]string{bundleFile, ossBundleFile})
	if err != nil {
		return nil, err
	}
	return br, nil
}

func listBundleScripts(br *script.BundleManager, format string) {
	w := components.CreateStreamWriter(format, os.Stdout)
	defer w.Finish()
	w.SetHeader("script_list", []string{"Name", "Description"})
	scripts := br.GetScripts()

	// Sort show org scripts show up first.
	sort.Slice(scripts, func(i, j int) bool {
		if len(scripts[i].OrgName) != 0 || len(scripts[j].OrgName) != 0 {
			return scripts[i].OrgName > scripts[j].OrgName
		}
		return scripts[i].ScriptName < scripts[j].ScriptName
	})

	for _, script := range scripts {
		if script.Hidden {
			continue
		}
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
