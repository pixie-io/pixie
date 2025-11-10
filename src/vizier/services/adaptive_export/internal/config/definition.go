// Copyright 2018- The Pixie Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

package config

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

	"gopkg.in/yaml.v2"

	"px.dev/pixie/src/vizier/services/adaptive_export/internal/script"
)

const scriptExtension = ".yaml"

// ReadScriptDefinitions reads the script definition from the given directory path.
// Only .yaml files are read and subdirectories are not traversed.
func ReadScriptDefinitions(dir string) ([]*script.ScriptDefinition, error) {
	if _, err := os.Stat(dir); os.IsNotExist(err) {
		return nil, nil
	}
	files, err := ioutil.ReadDir(dir)
	if err != nil {
		return nil, err
	}
	var l []*script.ScriptDefinition
	for _, file := range files {
		if strings.HasSuffix(file.Name(), scriptExtension) {
			description, err := readScriptDefinition(filepath.Join(dir, file.Name()))
			if err != nil {
				return nil, err
			}
			l = append(l, description)
		}
	}
	return l, nil
}

func readScriptDefinition(path string) (*script.ScriptDefinition, error) {
	content, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var definition script.ScriptDefinition
	err = yaml.Unmarshal(content, &definition)
	if err != nil {
		return nil, err
	}
	return &definition, nil
}
