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

package controllers

import (
	"context"
	"net/http"

	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/relay"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers/schema/complete"
	"px.dev/pixie/src/cloud/api/controllers/schema/noauth"
)

// GraphQLEnv holds the GRPC API servers so the GraphQL server can call out to them.
type GraphQLEnv struct {
	ArtifactTrackerServer cloudpb.ArtifactTrackerServer
	VizierClusterInfo     cloudpb.VizierClusterInfoServer
	VizierDeployKeyMgr    cloudpb.VizierDeploymentKeyManagerServer
	APIKeyMgr             cloudpb.APIKeyManagerServer
	ScriptMgrServer       cloudpb.ScriptMgrServer
	AutocompleteServer    cloudpb.AutocompleteServiceServer
	OrgServer             cloudpb.OrganizationServiceServer
	UserServer            cloudpb.UserServiceServer
	PluginServer          cloudpb.PluginServiceServer
}

// QueryResolver resolves queries for GQL.
type QueryResolver struct {
	Env GraphQLEnv
}

type resolverError struct {
	status *status.Status
}

func (re resolverError) Error() string {
	return re.status.Message()
}

func (re resolverError) Extensions() map[string]interface{} {
	return map[string]interface{}{
		"code":    re.status.Code(),
		"message": re.status.Message(),
	}
}

func rpcErrorHelper(err error) resolverError {
	return resolverError{status: status.Convert(err)}
}

// Noop is added to the query resolver to handle the fact that we can't define
// empty typesin graphql. :(
// There shouldn't be any consumers of Noop.
func (q *QueryResolver) Noop(ctx context.Context) (bool, error) {
	return true, nil
}

// NewGraphQLHandler is the HTTP handler used for handling GraphQL requests.
func NewGraphQLHandler(graphqlEnv GraphQLEnv) http.Handler {
	schemaData := complete.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	gqlSchema := graphql.MustParseSchema(schemaData, &QueryResolver{graphqlEnv}, opts...)
	return &relay.Handler{Schema: gqlSchema}
}

// NewUnauthenticatedGraphQLHandler is the HTTP handler used for handling unauthenticated GraphQL requests.
func NewUnauthenticatedGraphQLHandler(graphqlEnv GraphQLEnv) http.Handler {
	schemaData := noauth.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	gqlSchema := graphql.MustParseSchema(schemaData, &QueryResolver{graphqlEnv}, opts...)
	return &relay.Handler{Schema: gqlSchema}
}
