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

package script

import (
	"fmt"
	"strings"
)

const (
	scriptPrefix = "ch-"
)

type ScriptConfig struct {
	ClusterName     string
	ClusterId       string
	CollectInterval int64
}

type Script struct {
	ScriptDefinition
	ScriptId   string
	ClusterIds string
}

type ScriptDefinition struct {
	Name        string `yaml:"name"`
	Description string `yaml:"description"`
	FrequencyS  int64  `yaml:"frequencyS"`
	Script      string `yaml:"script"`
	IsPreset    bool   `yaml:"-"`
}

type ScriptActions struct {
	ToDelete []*Script
	ToUpdate []*Script
	ToCreate []*Script
}

func IsClickHouseScript(scriptName string) bool {
	return strings.HasPrefix(scriptName, scriptPrefix)
}

func IsScriptForCluster(scriptName, clusterName string) bool {
	return IsClickHouseScript(scriptName) && strings.HasSuffix(scriptName, "-"+clusterName)
}

func GetActions(scriptDefinitions []*ScriptDefinition, currentScripts []*Script, config ScriptConfig) ScriptActions {
	definitions := make(map[string]ScriptDefinition)
	for _, definition := range scriptDefinitions {
		scriptName := getScriptName(definition.Name, config.ClusterName)
		frequencyS := getInterval(definition, config)
		if frequencyS > 0 {
			definitions[scriptName] = ScriptDefinition{
				Name:        scriptName,
				Description: definition.Description,
				FrequencyS:  frequencyS,
				Script:      templateScript(definition, config),
			}
		}
	}
	actions := ScriptActions{}
	for _, current := range currentScripts {
		if definition, present := definitions[current.Name]; present {
			if definition.Script != current.Script || definition.FrequencyS != current.FrequencyS || config.ClusterId != current.ClusterIds {
				actions.ToUpdate = append(actions.ToUpdate, &Script{
					ScriptDefinition: definition,
					ScriptId:         current.ScriptId,
					ClusterIds:       config.ClusterId,
				})
			}
			delete(definitions, current.Name)
		} else if IsClickHouseScript(current.Name) {
			actions.ToDelete = append(actions.ToDelete, current)
		}
	}
	for _, definition := range definitions {
		actions.ToCreate = append(actions.ToCreate, &Script{
			ScriptDefinition: definition,
			ClusterIds:       config.ClusterId,
		})
	}
	return actions
}

func getScriptName(scriptName string, clusterName string) string {
	return fmt.Sprintf("%s%s-%s", scriptPrefix, scriptName, clusterName)
}

func getInterval(definition *ScriptDefinition, config ScriptConfig) int64 {
	if definition.FrequencyS == 0 {
		return config.CollectInterval
	}
	return definition.FrequencyS
}

func templateScript(definition *ScriptDefinition, config ScriptConfig) string {
	// Return script as-is without any processing
	return definition.Script
}
