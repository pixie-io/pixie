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

package controllers

import (
	"encoding/json"
	"fmt"
	"regexp"
	"strconv"
	"strings"
)

// visSpecObj represents a call to a visualization spec function.
type visSpecObj struct {
	Function string                 `json:"func"`
	Args     map[string]interface{} `json:"args"`
}

// placementObj represents the entire placement definition.
type placementObj struct {
	Name       string                `json:"name"`
	GlobalArgs map[string]string     `json:"args"`
	Tables     map[string]visSpecObj `json:"tables"`
}

// Object to hold the context of the compiler to simplify function calls.
// Can be used to store the available functions to the pxl script so we can error
// out in the placement compiler if someone specifies a function that doesn't exist.
type compilerState struct {
	globalArgMap *map[string]string
}

// PlacementCompilerConfig is the config for the PlacementCompiler. This doesn't hold state
// because we may reuse the placement compiler for multiple compilations.
type PlacementCompilerConfig struct {
	Spacing      string
	MainFuncName string
}

// PlacementCompiler is the object used to compile placement.json into valid pxl scripts.
type PlacementCompiler struct {
	funcName string
	spacing  string
}

// lookupVarOrStrLiteral converts a string into a variable if one exists.
func (pc *PlacementCompiler) lookupVarOrStrLiteral(state *compilerState, s string) (string, error) {
	if len(s) == 0 {
		return s, nil
	}

	// Everything that is not prefixed with '@' should be a string
	if s[0] != '@' {
		return fmt.Sprintf("'%s'", s), nil
	}
	_, ok := (*state.globalArgMap)[s[1:]]
	if !ok {
		return "", fmt.Errorf("'%s' is not a valid global arg", s[1:])
	}
	return s[1:], nil
}

// prepareVisFuncArg interprets one argumment to a function that is called in the placement spec.
func (pc *PlacementCompiler) prepareVisFuncArg(state *compilerState, argName string, argVal interface{}) (string, error) {
	if s, ok := argVal.(string); ok {
		return pc.lookupVarOrStrLiteral(state, s)
	} else if i, ok := argVal.(int); ok {
		return fmt.Sprintf("%d", i), nil
	} else if fl, ok := argVal.(float64); ok {
		return strconv.FormatFloat(fl, 'f', -1, 64), nil
	}
	return "", fmt.Errorf("%T type not found", argVal)
}

// createVisSpecCalls creates the vizier spec call.
func (pc *PlacementCompiler) createVisSpecCalls(state *compilerState, tableName string, table *visSpecObj) ([]string, error) {
	argVals := make([]string, len(table.Args))
	i := 0
	for argName, argVal := range table.Args {
		if err := pc.validArgNameOrError(argName); err != nil {
			return nil, err
		}
		a, err := pc.prepareVisFuncArg(state, argName, argVal)
		if err != nil {
			return nil, err
		}
		argVals[i] = fmt.Sprintf("%s=%s", argName, a)
		i++
	}
	lines := make([]string, 2)
	lines[0] = fmt.Sprintf("%sdf = %s(%s)", pc.spacing, table.Function, strings.Join(argVals, ", "))
	lines[1] = fmt.Sprintf("%spx.display(df, '%s')", pc.spacing, tableName)
	return lines, nil
}

// createSignature makes the signature of the main function
func (pc *PlacementCompiler) createSignature(state *compilerState) (string, error) {
	var argsFmt []string
	for argName, argType := range *state.globalArgMap {
		argsFmt = append(argsFmt, fmt.Sprintf("%s: %s", argName, argType))
	}
	return fmt.Sprintf("def %s(%s):", pc.funcName, strings.Join(argsFmt, ", ")), nil
}

// Verifies whether the argName is valid Python.
func (pc *PlacementCompiler) isValidArgName(argName string) bool {
	// Variables can only be made of the underscore char, letters, and numbers.
	// First character cannot be a number.
	re := regexp.MustCompile("^[A-z_][A-z0-9_]*$")
	return re.Match([]byte(argName))
}

func (pc *PlacementCompiler) validArgNameOrError(argName string) error {
	if pc.isValidArgName(argName) {
		return nil
	}
	return fmt.Errorf("'%s' is an invalid argname. must match regex ^[A-z_][A-z0-9_]*$", argName)
}

func newPlacementCompiler(config PlacementCompilerConfig) *PlacementCompiler {
	return &PlacementCompiler{
		spacing:  config.Spacing,
		funcName: config.MainFuncName,
	}
}

func newDefaultPlacementCompilerConfig() PlacementCompilerConfig {
	return PlacementCompilerConfig{
		Spacing:      "    ",
		MainFuncName: "main",
	}
}

func (pc *PlacementCompiler) prepareCompilerState(cs *compilerState, placement *placementObj) error {
	// Verify that the global args are valid.
	for argName := range placement.GlobalArgs {
		if err := pc.validArgNameOrError(argName); err != nil {
			return err
		}
	}
	cs.globalArgMap = &placement.GlobalArgs
	return nil
}

// Internal function that might support specifying state outside of the call to PlacementToPxl.
func (pc *PlacementCompiler) placementToPxl(state *compilerState, jsonStr string) (string, error) {
	// First parse the placement JSON.
	var placement placementObj
	err := json.Unmarshal([]byte(jsonStr), &placement)
	if err != nil {
		return "", err
	}

	if len(placement.Tables) == 0 {
		return "", fmt.Errorf("you must specify tables in the placement spec")
	}

	err = pc.prepareCompilerState(state, &placement)
	if err != nil {
		return "", err
	}

	var lines []string
	// Create the signature of the compiled pxl function.
	signature, err := pc.createSignature(state)
	if err != nil {
		return "", err
	}
	lines = append(lines, signature)
	for tableName, tableObj := range placement.Tables {
		// Create the calls to the functions backing the visualization specs.
		curLines, err := pc.createVisSpecCalls(state, tableName, &tableObj)
		if err != nil {
			return "", err
		}
		lines = append(lines, curLines...)
	}

	return strings.Join(lines, "\n"), nil
}

// NewPlacementCompiler creates a new compiler object.
func NewPlacementCompiler() *PlacementCompiler {
	return newPlacementCompiler(newDefaultPlacementCompilerConfig())
}

// PlacementToPxl converts a JSON placement to a pxl script.
func (pc *PlacementCompiler) PlacementToPxl(jsonStr string) (string, error) {
	state := &compilerState{}
	return pc.placementToPxl(state, jsonStr)
}
