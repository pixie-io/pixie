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
	"fmt"
	"strings"
)

func indent(i string) string {
	return "\t" + strings.Join(strings.Split(i, "\n"), "\n\t")
}

// ErrorAccumulator is the struct that stores accumulated errors.
type ErrorAccumulator struct {
	errorStrs []string
}

// AddError accumulates the passed in error if its not nil.
func (ea *ErrorAccumulator) AddError(e error) {
	if e == nil {
		return
	}
	ea.errorStrs = append(ea.errorStrs, e.Error())
}

// Merge returns a merged representation of the accumulated errors.
func (ea *ErrorAccumulator) Merge() error {
	if len(ea.errorStrs) == 0 {
		return nil
	}
	return fmt.Errorf(indent(strings.Join(ea.errorStrs, "\n")))
}

// MakeErrorAccumulator constructs the ErrorAccumulator.
func MakeErrorAccumulator() *ErrorAccumulator {
	return &ErrorAccumulator{
		errorStrs: make([]string, 0),
	}
}
