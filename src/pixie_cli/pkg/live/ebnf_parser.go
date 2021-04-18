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

package live

import (
	"strings"

	"github.com/alecthomas/participle"
	"github.com/alecthomas/participle/lexer"
	"github.com/alecthomas/participle/lexer/ebnf"
)

var (
	grammar = `
        Ident = (alpha | Digit | "/" | "_" | "$" | "-") { "/" | alpha | Digit | "_" | "$" | "-"} .
        Whitespace = " " | "\t" | "\n" | "\r" .
        Punct = "!"…"/" | ":"…"@" | "["…` + "\"`\"" + ` | "{"…"~" | "$" .
        alpha = "a"…"z" | "A"…"Z" .
        Digit = "0"…"9" .
    `

	cmdLexer = lexer.Must(ebnf.New(grammar))
	parser   = participle.MustBuild(&Cmd{},
		participle.Lexer(cmdLexer),
		participle.Elide("Whitespace"),
	)
)

// ParseInput parses the user input into a command using an ebnf parser.
func ParseInput(input string) (*Cmd, error) {
	cmd := &Cmd{}
	r := strings.NewReader(input)

	err := parser.Parse(r, cmd)
	if err != nil {
		return nil, err
	}

	return cmd, nil
}

// Cmd represents the formatted cmd parsed into tabstops.
type Cmd struct {
	TabStops []*TabStop `parser:"(\"$\" @@)*"`
}

// TabStop represents the fields where the user's should go when tabbing.
type TabStop struct {
	Index    *int    `parser:"(\"{\" @Ident )"`
	Label    *string `parser:"(\":\" @Ident )?"`
	HasLabel bool    `parser:"@\":\"?"`
	Value    *string `parser:"((@Ident)? \"}\")?"`
}
