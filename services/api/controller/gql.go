package controller

import (
	"net/http"

	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/relay"
	"pixielabs.ai/pixielabs/services/api/controller/schema"
)

type query struct{}

func (*query) Dummy() string { return "Hello, world!" }

// GraphQLHandler is the hTTP handler used for handling GraphQL requests.
func GraphQLHandler() http.Handler {
	schemaData := schema.MustLoadSchema()
	gqlSchema := graphql.MustParseSchema(schemaData, &query{})
	return &relay.Handler{Schema: gqlSchema}
}
