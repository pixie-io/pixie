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

package pxconfig

import (
	"encoding/json"
	"os"
	"sync"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/pixie_cli/pkg/utils"
)

// ConfigInfo store the config about the CLI.
type ConfigInfo struct {
	// UniqueClientID is the ID assigned to this user on first startup when auth information is not know. This can be later associated with the UserID.
	UniqueClientID string `json:"uniqueClientID"`
}

var (
	config *ConfigInfo
	once   sync.Once
)

func writeDefaultConfig(path string) (*ConfigInfo, error) {
	f, err := os.OpenFile(path, os.O_RDWR|os.O_CREATE, 0600)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	clientID, err := uuid.NewV4()
	if err != nil {
		return nil, err
	}

	cfg := &ConfigInfo{UniqueClientID: clientID.String()}
	if err := json.NewEncoder(f).Encode(cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}

func readDefaultConfig(path string) (*ConfigInfo, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	cfg := &ConfigInfo{}
	if err := json.NewDecoder(f).Decode(cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}

// Cfg returns the default config.
func Cfg() *ConfigInfo {
	once.Do(func() {
		configPath, err := utils.EnsureDefaultConfigFilePath()
		if err != nil {
			utils.WithError(err).Fatal("Failed to load/create config file path")
		}
		_, err = os.Stat(configPath)
		if os.IsNotExist(err) {
			// Write the default config.
			if config, err = writeDefaultConfig(configPath); err != nil {
				utils.WithError(err).Fatal("Failed to create default config")
			}
			return
		}

		if config, err = readDefaultConfig(configPath); err != nil {
			utils.WithError(err).Fatal("Failed to read config file")
		}
	})
	return config
}
