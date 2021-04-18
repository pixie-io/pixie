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

// A general set of utils that are useful throughout the project
import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"strings"
)

// FindBazelWorkspaceRoot Finds the Workspace directory as specified by bazel.
func FindBazelWorkspaceRoot() (string, error) {
	workspaceDir, exists := os.LookupEnv("BUILD_WORKSPACE_DIRECTORY")
	if exists {
		return workspaceDir, nil
	}
	return workspaceDir, errors.New("WORKSPACE Directory could not be found")
}

// GetStdinInput Gets the stdin from the terminal.
func GetStdinInput(message string) (string, error) {
	reader := bufio.NewReader(os.Stdin)
	fmt.Print(message)
	text, err := reader.ReadString('\n')
	text = strings.Trim(text, "\n")
	return text, err
}

// FileExists checks whether a string path exists.
func FileExists(filePath string) bool {
	_, err := os.Stat(filePath)
	return !os.IsNotExist(err)
}
