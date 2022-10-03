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

package script

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"strings"
)

// ErrMissingRequiredArgument specifies that a required script flag has not been provided.
var ErrMissingRequiredArgument = errors.New("missing required argument")

// FlagSet is a wrapper around flag.FlagSet, because the latter
// does not support required args without a default value.
type FlagSet struct {
	baseFlagSet *flag.FlagSet
	// Keeps track of which args have values (whether it is a default value or a passed in value)
	// Used to differentiate between an unset arg and an arg that has an empty default value.
	argHasValue map[string]bool
}

// NewFlagSet creates a new FlagSet.
func NewFlagSet(scriptName string) *FlagSet {
	return &FlagSet{
		baseFlagSet: flag.NewFlagSet(scriptName, flag.ContinueOnError),
		argHasValue: make(map[string]bool),
	}
}

// String is a wrapper around flag.FlagSet's String function.
// It declares the presence of an argument.
// It differs from FlagSet's string in that defaultValue is allowed to be nil.
func (f *FlagSet) String(name string, defaultValue *string, usage string) {
	f.argHasValue[name] = defaultValue != nil
	if defaultValue != nil {
		f.baseFlagSet.String(name, *defaultValue, usage)
	} else {
		f.baseFlagSet.String(name, "", fmt.Sprintf("(required) %s", usage))
	}
}

// Parse wraps flag.FlagSet's Parse function to parse args.
func (f *FlagSet) Parse(arguments []string) error {
	// Get the flag values defined, so we can mark which ones are actually set.
	for _, arg := range arguments {
		// Not a flag
		if len(arg) == 0 || arg[0] != '-' {
			continue
		}
		splits := strings.SplitN(strings.Trim(arg, "-"), "=", 2)
		if len(splits) < 1 {
			return fmt.Errorf("Error parsing argument string: %s", arg)
		}
		f.argHasValue[splits[0]] = true
	}
	return f.baseFlagSet.Parse(arguments)
}

// Set wraps flag.FlagSet's Set function.
func (f *FlagSet) Set(name, value string) error {
	f.argHasValue[name] = true
	return f.baseFlagSet.Set(name, value)
}

// Lookup wraps flag.FlagSet's Lookup function, returning an error if we
// look up a required arg (without a default value) that hasn't been set.
func (f *FlagSet) Lookup(name string) (string, error) {
	if f.argHasValue[name] {
		return f.baseFlagSet.Lookup(name).Value.String(), nil
	}
	return "", fmt.Errorf("%w : '%s'", ErrMissingRequiredArgument, name)
}

// SetOutput wraps flag.FlagSet's SetOutput function.
func (f *FlagSet) SetOutput(output io.Writer) {
	f.baseFlagSet.SetOutput(output)
}

// Usage wraps flag.FlagSet's Usage function.
func (f *FlagSet) Usage() {
	f.baseFlagSet.Usage()
}
