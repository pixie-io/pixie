package controller

import (
	"context"
	"net/http"

	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/relay"
	"pixielabs.ai/pixielabs/src/services/api/apienv"
	"pixielabs.ai/pixielabs/src/services/api/controller/schema"
	"pixielabs.ai/pixielabs/src/services/common/sessioncontext"
)

// QueryResolver resolves queries for GQL.
type QueryResolver struct {
	Env apienv.APIEnv
}

// MutationResolver resolves mutations for GQL.
type MutationResolver struct {
	Env apienv.APIEnv
}

// User resolves user information.
func (*QueryResolver) User(ctx context.Context) (*UserInfoResolver, error) {
	sCtx, err := sessioncontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	return &UserInfoResolver{sCtx}, nil
}

// NewGraphQLHandler is the hTTP handler used for handling GraphQL requests.
func NewGraphQLHandler(env apienv.APIEnv) http.Handler {
	schemaData := schema.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	gqlSchema := graphql.MustParseSchema(schemaData, &QueryResolver{env}, opts...)
	return &relay.Handler{Schema: gqlSchema}
}
