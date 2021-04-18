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

package vizier

import (
	"strings"

	"github.com/fatih/color"
)

// ErrorCode is the base type for vizier error codes.
type ErrorCode int

const (
	// CodeUnknown is placeholder for unknown errors.
	CodeUnknown ErrorCode = iota
	// CodeTimeout is for execution timeouts.
	CodeTimeout
	// CodeBadData occurs when bad data/format is received from vizier.
	CodeBadData
	// CodeGRPCError is used for GRPC errors.
	CodeGRPCError
	// CodeCompilerError is used for compilations errors.
	CodeCompilerError
	// CodeCanceled is used for script cancellation.
	CodeCanceled
)

// ScriptExecutionError occurs for errors during script execution on vizier.
type ScriptExecutionError struct {
	code           ErrorCode
	s              string
	compilerErrors []string
}

// Error returns the errors message.
func (s *ScriptExecutionError) Error() string {
	return s.s
}

// Code returns the error code.
func (s *ScriptExecutionError) Code() ErrorCode {
	return s.code
}

// CompilerErrors returns compiler errors as strings if any.
func (s *ScriptExecutionError) CompilerErrors() []string {
	return s.compilerErrors
}

// GetErrorCode gets the error code for vizier errors.
func GetErrorCode(err error) ErrorCode {
	if e, ok := err.(*ScriptExecutionError); ok {
		return e.Code()
	}
	return CodeUnknown
}

func newScriptExecutionError(c ErrorCode, m string) *ScriptExecutionError {
	return &ScriptExecutionError{
		code: c,
		s:    m,
	}
}

// FormatErrorMessage converts Vizier error messages into stylized strings.
func FormatErrorMessage(err error) string {
	if err == nil {
		return ""
	}
	sb := strings.Builder{}
	switch e := err.(type) {
	case *ScriptExecutionError:
		if e.Code() == CodeCompilerError {
			sb.WriteString(color.RedString("Script Compilation Failed:"))
			sb.WriteString("\n")
			for _, s := range e.CompilerErrors() {
				sb.WriteString("  ")
				sb.WriteString(s)
			}
			break
		}
		sb.WriteString(color.RedString("Script Execution Error:"))
		sb.WriteString(e.Error())
	default:
		sb.WriteString(color.RedString("Error:"))
		sb.WriteString("\n")
		sb.WriteString(e.Error())
	}

	sb.WriteString("\nType '?' for help or ctrl-k to select another script.")
	return sb.String()
}
