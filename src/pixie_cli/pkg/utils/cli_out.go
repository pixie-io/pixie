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
	"io"
	"os"

	"github.com/fatih/color"
)

// CLIOutputEntry represents an output log entry.
type CLIOutputEntry struct {
	textColor *color.Color
	err       error
}

var defaultCLIOutput = &CLIOutputEntry{
	textColor: nil,
	err:       nil,
}

// WithColor returns a struct that can be used to log text to the CLI
// in a specific color.
func WithColor(c *color.Color) *CLIOutputEntry {
	return &CLIOutputEntry{
		textColor: c,
	}
}

// WithError returns a struct that can be used to log text to the CLI
// with a specific error.
func WithError(err error) *CLIOutputEntry {
	return &CLIOutputEntry{
		err: err,
	}
}

// Infof prints the input string to stdout formatted with the input args.
func Infof(format string, args ...interface{}) {
	defaultCLIOutput.Infof(format, args...)
}

// Info prints the input string to stdout.
func Info(str string) {
	defaultCLIOutput.Info(str)
}

// Errorf prints the input string to stderr formatted with the input args.
func Errorf(format string, args ...interface{}) {
	defaultCLIOutput.Errorf(format, args...)
}

// Error prints the input string to stderr.
func Error(str string) {
	defaultCLIOutput.Error(str)
}

// Fatalf prints the input string to stderr formatted with the input args.
func Fatalf(format string, args ...interface{}) {
	defaultCLIOutput.Fatalf(format, args...)
}

// Fatal prints the input string to stderr.
func Fatal(str string) {
	defaultCLIOutput.Fatal(str)
}

// WithColor returns a struct that can be used to log text to the CLI
// in a specific color.
func (c *CLIOutputEntry) WithColor(textColor *color.Color) *CLIOutputEntry {
	return &CLIOutputEntry{
		err:       c.err,
		textColor: textColor,
	}
}

// WithError returns a struct that can be used to log text to the CLI
// with a specific error.
func (c *CLIOutputEntry) WithError(err error) *CLIOutputEntry {
	return &CLIOutputEntry{
		err:       err,
		textColor: c.textColor,
	}
}

func (c *CLIOutputEntry) write(w io.Writer, format string, args ...interface{}) {
	text := fmt.Sprintf(format, args...)
	if c.err != nil {
		text += fmt.Sprintf(" error=%s", c.err.Error())
	}
	text += "\n"
	if c.textColor == nil {
		fmt.Fprint(w, text)
	} else {
		c.textColor.Fprint(w, text)
	}
}

// Infof prints the input string to stdout formatted with the input args.
func (c *CLIOutputEntry) Infof(format string, args ...interface{}) {
	c.write(os.Stderr, format, args...)
}

// Info prints the input string to stdout.
func (c *CLIOutputEntry) Info(str string) {
	c.Infof(str)
}

// Errorf prints the input string to stderr formatted with the input args.
func (c *CLIOutputEntry) Errorf(format string, args ...interface{}) {
	c.write(os.Stderr, format, args...)
}

// Error prints the input string to stderr.
func (c *CLIOutputEntry) Error(str string) {
	c.write(os.Stderr, str)
}

// Fatalf prints the input string to stderr formatted with the input args.
func (c *CLIOutputEntry) Fatalf(format string, args ...interface{}) {
	c.write(os.Stderr, format, args...)
	os.Exit(1)
}

// Fatal prints the input string to stderr.
func (c *CLIOutputEntry) Fatal(str string) {
	c.write(os.Stderr, str)
	os.Exit(1)
}
