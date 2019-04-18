package main

import (
	"pixielabs.ai/pixielabs/src/vizier/components/compiler"
)

import "fmt"

func main() {
	// This is a test to make sure that the rest of the system will run.
	// TODO(philkuz) actually load the udf and make it work.
	udfProto := "todo"
	c := compiler.New()
	defer c.Free()
	// TODO(philkuz) properly pass in the schema and the query from somewhere else.
	// currently just prints out these strings.
	// outputs -> "todo    relation_map24  query7"
	fmt.Println(c.Compile("relation_map24", "query7", udfProto))
}
