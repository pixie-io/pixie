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

package components

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/spf13/viper"
)

// This file has components that interact with the user via prompts.

// Prompter proves a user input dialog.
type Prompter struct {
	message string
	choices []string
	dv      string
}

// NewPrompter creates a Prompter with the given message, choices and defaults.
func NewPrompter(message string, choices []string, defaultValue string) *Prompter {
	return &Prompter{
		message: message,
		choices: choices,
		dv:      defaultValue,
	}
}

// Prompt prompts the user and return the value. If the config parameter "y" is set we will return the default.
func (p *Prompter) Prompt() string {
	if p.skip() {
		return p.dv
	}
	fmt.Print(p.msg())
	input := ""
	s := bufio.NewScanner(os.Stdin)
	ok := s.Scan()
	if ok {
		input = strings.TrimRight(s.Text(), "\r\n")
	}
	if input == "" {
		return p.dv
	}
	if !p.validInput(input) {
		fmt.Println(p.errorMsg())
		return p.Prompt()
	}
	return input
}

func (p *Prompter) validInput(s string) bool {
	// Compare values ignoring case.
	ls := strings.ToLower(s)
	for i := range p.choices {
		if ls == strings.ToLower(p.choices[i]) {
			return true
		}
	}
	return false
}

func (p *Prompter) msg() string {
	defaultValue := ""
	if p.dv != "" {
		defaultValue = fmt.Sprintf("[%s] ", p.dv)
	}
	return fmt.Sprintf("%s (%s) %s: ", p.message, strings.Join(p.choices, "/"), defaultValue)
}

func (p *Prompter) errorMsg() string {
	return fmt.Sprintf("Invalid input, must be one of: [%s]", strings.Join(p.choices, ", "))
}

func (p *Prompter) skip() bool {
	return viper.GetBool("y")
}

// YNPrompt is a helper function that prompts the user for a Y/N response.
func YNPrompt(message string, defaultValue bool) bool {
	defaultChoice := "n"
	if defaultValue {
		defaultChoice = "y"
	}
	return strings.ToLower(NewPrompter(message, []string{"y", "n"}, defaultChoice).Prompt()) == "y"
}
