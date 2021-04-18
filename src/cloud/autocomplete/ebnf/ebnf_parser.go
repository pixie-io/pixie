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

package ebnf

import (
	"strings"

	"github.com/alecthomas/participle"
	"github.com/alecthomas/participle/lexer"
	"github.com/alecthomas/participle/lexer/ebnf"
)

var (
	grammar = `
        Ident = (alpha | digit | "/" | "_" | "$" | "-") { "/" | alpha | digit | "_" | "$" | "-"} .
        Whitespace = " " | "\t" | "\n" | "\r" .
        Punct = "!"…"/" | ":"…"@" | "["…` + "\"`\"" + ` | "{"…"~" .
        alpha = "a"…"z" | "A"…"Z" .
        digit = "0"…"9" .
    `
	cmdLexer = lexer.Must(ebnf.New(grammar))
	parser   = participle.MustBuild(&ParsedCmd{},
		participle.Lexer(cmdLexer),
	)
)

// ParsedCmd represents a command parsed through ebnf.
type ParsedCmd struct {
	Action *string      `parser:"{@ \"go\" Whitespace? | @ \"run\" Whitespace?}?"`
	Args   []*ParsedArg `parser:"@@*"`
}

// ParsedArg represents an arg parsed through ebnf.
type ParsedArg struct {
	Type *string `parser:"(@Ident \":\")?"`
	Name *string `parser:"(@Ident Whitespace?|Whitespace)?"`
}

// ParseInput parses the user input into a command using an ebnf parser.
func ParseInput(input string) (*ParsedCmd, error) {
	cmd := &ParsedCmd{}
	r := strings.NewReader(input)

	err := parser.Parse(r, cmd)
	if err != nil {
		return nil, err
	}

	return cmd, nil
}
