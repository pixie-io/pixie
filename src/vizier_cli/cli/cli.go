package cli

import (
	"strings"

	"github.com/abiosoft/ishell"
)

// Executor is an interface that the CLI uses to process commands.
type Executor interface {
	ExecuteQuery(in string) error
	PrintAgentInfo() error
	Connect(in string) error
}

// CLI defines the pixie CLI state.
type CLI struct {
	ExecutorInst Executor
	shell        *ishell.Shell
}

// New creates and returns a new CLI.
func New(exec Executor) *CLI {
	cli := &CLI{
		ExecutorInst: exec,
		shell:        ishell.New(),
	}

	cli.shell.AddCmd(&ishell.Cmd{
		Name: "query",
		Help: "Execute a Query",
		Func: func(c *ishell.Context) {
			c.SetPrompt("... ")
			defer c.SetPrompt(">>> ")
			queryStr := c.ReadMultiLines(";")
			queryStr = strings.Replace(queryStr, ";", "", -1)
			err := cli.ExecutorInst.ExecuteQuery(queryStr)
			if err != nil {
				c.Println("Failed to execute query: " + err.Error())
			}
		},
	})

	cli.shell.AddCmd(&ishell.Cmd{
		Name: "agents",
		Help: "Get agent information",
		Func: func(c *ishell.Context) {
			err := cli.ExecutorInst.PrintAgentInfo()
			if err != nil {
				c.Println("Failed to get agent information " + err.Error())
			}
		},
	})

	cli.shell.AddCmd(&ishell.Cmd{
		Name: "connect",
		Help: "Connect to Vizier",
		Func: func(c *ishell.Context) {
			hostnameAddr := strings.Join(c.Args, "")
			err := cli.ExecutorInst.Connect(hostnameAddr)
			if err != nil {
				c.Println("Failed to connect " + err.Error())
			}
		},
	})

	return cli
}

// Run the CLI. This function blocks until the user asks exit.
func (c *CLI) Run() {
	c.shell.Run()
}
