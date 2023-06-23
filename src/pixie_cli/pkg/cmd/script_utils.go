/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package cmd

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path"

	"github.com/bmatcuk/doublestar"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/utils/script"
)

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-core.json"
const ossBundleFile = "https://artifacts.px.dev/pxl_scripts/bundle.json"

func mustCreateBundleReader() *script.BundleManager {
	br, err := createBundleReader()
	if err != nil {
		// Keep this as a log.Fatal() as opposed to using the CLI logger, because it
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
	direct := viper.GetString("direct_vizier_addr")

	var orgID string
	var orgName string
	if direct == "" {
		authInfo := auth.MustLoadDefaultCredentials()
		orgID = authInfo.OrgID
		orgName = authInfo.OrgName
	}

	br, err := script.NewBundleManagerWithOrg([]string{bundleFile, ossBundleFile}, orgID, orgName)
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

	for _, script := range scripts {
		if script.Hidden {
			continue
		}
		err := w.Write([]interface{}{script.ScriptName, script.ShortDoc})
		if err != nil {
			log.WithError(err).Error("Failed to write to stream")
		}
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
	s.IsLocal = true
	// Read from STDIN.
	query, err := io.ReadAll(os.Stdin)
	if err != nil {
		return nil, err
	}
	if len(query) == 0 {
		return nil, errors.New("script string is empty")
	}
	s.ScriptName = "<stdin_script>"
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
	pxlFiles, err := doublestar.Glob(path.Join(scriptPath, "*.pxl"))
	if err != nil {
		return nil, err
	}
	if len(pxlFiles) != 1 {
		return nil, fmt.Errorf("Expected 1 pxl file, got %d", len(pxlFiles))
	}
	s, err := loadScriptFromFile(pxlFiles[0])
	if err != nil {
		return nil, err
	}
	visFile := path.Join(scriptPath, "vis.json")
	if fileExists(visFile) {
		vis, err := os.ReadFile(visFile)
		if err != nil {
			return nil, err
		}
		if s.Vis, err = script.ParseVisSpec(string(vis)); err != nil {
			return nil, err
		}
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
	s.IsLocal = true
	query, err := io.ReadAll(r)
	if err != nil {
		return nil, err
	}
	s.ScriptString = string(query)
	s.ScriptName = fmt.Sprintf("%s<local>", scriptPath)

	return s, nil
}
