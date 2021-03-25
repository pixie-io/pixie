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
