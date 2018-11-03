package controller

import (
	"context"
	"net/http"

	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/relay"
	"pixielabs.ai/pixielabs/services/api/apienv"
	"pixielabs.ai/pixielabs/services/api/controller/schema"
	"pixielabs.ai/pixielabs/services/common/sessioncontext"
)

type query struct {
	env apienv.APIEnv
}

func (*query) User(ctx context.Context) (*UserInfoResolver, error) {
	sCtx, err := sessioncontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	return &UserInfoResolver{sCtx}, nil
}

// GraphQLHandler is the hTTP handler used for handling GraphQL requests.
func GraphQLHandler(env apienv.APIEnv) http.Handler {
	schemaData := schema.MustLoadSchema()
	gqlSchema := graphql.MustParseSchema(schemaData, &query{env})
	return &relay.Handler{Schema: gqlSchema}
}
