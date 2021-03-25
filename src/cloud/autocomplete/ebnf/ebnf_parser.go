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
