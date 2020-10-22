package cmd

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"sort"

	"github.com/bmatcuk/doublestar"
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
		// Keep this as a log.Fatal() as opposed to using the cliLog, because it
		// is an unexpected error that Sentry should catch.
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

func fileExists(filename string) bool {
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return !info.IsDir()
}

func baseScript() *script.ExecutableScript {
	return &script.ExecutableScript{
		ShortDoc: "Script supplied by user",
		LongDoc:  "Script supplied by user",
	}
}

func loadScriptFromStdin() (*script.ExecutableScript, error) {
	s := baseScript()
	// Read from STDIN.
	query, err := ioutil.ReadAll(os.Stdin)
	if err != nil {
		return nil, err
	}
	if len(query) == 0 {
		return nil, errors.New("script string is empty")
	}
	s.ScriptName = "stdin_script"
	s.ScriptString = string(query)
	return s, nil
}

func isDir(scriptPath string) bool {
	r, err := os.Open(scriptPath)
	if err != nil {
		return false
	}
	stat, err := r.Stat()
	if err != nil {
		return false
	}
	return stat.Mode().IsDir()
}

func loadScriptFromDir(scriptPath string) (*script.ExecutableScript, error) {
	s := baseScript()

	pxlFiles, err := doublestar.Glob(path.Join(scriptPath, "*.pxl"))
	if len(pxlFiles) != 1 {
		return nil, fmt.Errorf("Expected 1 pxl file, got %d", len(pxlFiles))
	}
	query, err := ioutil.ReadFile(pxlFiles[0])
	if err != nil {
		return nil, err
	}
	s.ScriptString = string(query)
	visFile := path.Join(scriptPath, "vis.json")
	if fileExists(visFile) {
		vis, err := ioutil.ReadFile(visFile)
		if err != nil {
			return nil, err
		}
		s.Vis = script.ParseVisSpec(string(vis))
	}
	return s, nil
}

func loadScriptFromFile(scriptPath string) (*script.ExecutableScript, error) {
	if scriptPath == "-" {
		return loadScriptFromStdin()
	}

	if isDir(scriptPath) {
		return loadScriptFromDir(scriptPath)
	}

	r, err := os.Open(scriptPath)
	if err != nil {
		return nil, err
	}

	s := baseScript()
	query, err := ioutil.ReadAll(r)
	if err != nil {
		return nil, err
	}
	s.ScriptString = string(query)
	return s, nil
}
