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

package utils

import (
	"os"
	"path/filepath"
)

const (
	pixieDotPath    = ".pixie"
	pixieConfigFile = "config.json"
	pixieAuthFile   = "auth.json"
)

// ensureDotFolderPath returns and creates the dot folder for cli config/auth.
func ensureDotFolderPath() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}

	pixieDirPath := filepath.Join(home, pixieDotPath)
	if _, err := os.Stat(pixieDirPath); os.IsNotExist(err) {
		err = os.Mkdir(pixieDirPath, 0744)
		if err != nil {
			return "", err
		}
	}

	return pixieDirPath, nil
}

// EnsureDefaultConfigFilePath returns the file path for the config file.
func EnsureDefaultConfigFilePath() (string, error) {
	pixieDirPath, err := ensureDotFolderPath()
	if err != nil {
		return "", err
	}

	pixieConfigFilePath := filepath.Join(pixieDirPath, pixieConfigFile)
	return pixieConfigFilePath, nil
}

// EnsureDefaultAuthFilePath returns the file path for the auth file.
func EnsureDefaultAuthFilePath() (string, error) {
	pixieDirPath, err := ensureDotFolderPath()
	if err != nil {
		return "", err
	}

	pixieAuthFilePath := filepath.Join(pixieDirPath, pixieAuthFile)
	return pixieAuthFilePath, nil
}
