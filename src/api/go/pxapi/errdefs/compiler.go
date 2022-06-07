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

package errdefs

import (
	"fmt"

	"px.dev/pixie/src/api/proto/vizierpb"
)

// CompilerMultiError is an implementation of a multi-error for compiler messages.
type CompilerMultiError struct {
	errors []error
}

// Error returns the string representation of the error.
func (c CompilerMultiError) Error() string {
	s := "Compilation failed: "
	for _, e := range c.errors {
		s += "\n"
		s += e.Error()
	}
	return s
}

// Errors returns the list of underlying errors.
func (c CompilerMultiError) Errors() []error {
	return c.errors
}

// Unwrap makes this error wrap a generic compilation error.
func (c CompilerMultiError) Unwrap() error {
	return ErrCompilation
}

func newCompilerMultiError(errs ...error) error {
	e := CompilerMultiError{
		errors: make([]error, len(errs)),
	}
	copy(e.errors, errs)
	return e
}

// CompilerErrorDetails is an interface to access error details from the compiler.
type CompilerErrorDetails interface {
	Line() int64
	Column() int64
	Message() int64
}

type compilerErrorWithDetails struct {
	line    int64
	column  int64
	message string
}

func (e compilerErrorWithDetails) UnWrap() error {
	return ErrCompilation
}

func (e compilerErrorWithDetails) Error() string {
	return fmt.Sprintf("%d:%d %s", e.line, e.column, e.message)
}

func (e compilerErrorWithDetails) Line() int64 {
	return e.line
}

func (e compilerErrorWithDetails) Column() int64 {
	return e.line
}

func (e compilerErrorWithDetails) Message() string {
	return e.message
}

func newCompilerErrorWithDetails(e *vizierpb.CompilerError) compilerErrorWithDetails {
	return compilerErrorWithDetails{
		line:    int64(e.Line),
		column:  int64(e.Column),
		message: e.Message,
	}
}
