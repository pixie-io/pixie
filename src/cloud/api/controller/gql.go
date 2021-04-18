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

package controller

import (
	"net/http"

	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/relay"

	public_cloudapipb "px.dev/pixie/src/api/public/cloudapipb"
	"px.dev/pixie/src/cloud/api/controller/schema"
	unauthenticatedschema "px.dev/pixie/src/cloud/api/controller/unauthenticated_schema"
	"px.dev/pixie/src/cloud/cloudapipb"
	profilepb "px.dev/pixie/src/cloud/profile/profilepb"
)

// GraphQLEnv holds the GRPC API servers so the GraphQL server can call out to them.
type GraphQLEnv struct {
	ArtifactTrackerServer cloudapipb.ArtifactTrackerServer
	VizierClusterInfo     cloudapipb.VizierClusterInfoServer
	VizierDeployKeyMgr    cloudapipb.VizierDeploymentKeyManagerServer
	APIKeyMgr             cloudapipb.APIKeyManagerServer
	ScriptMgrServer       cloudapipb.ScriptMgrServer
	AutocompleteServer    cloudapipb.AutocompleteServiceServer
	OrgServer             public_cloudapipb.OrganizationServiceServer

	ProfileServiceClient profilepb.ProfileServiceClient
}

// QueryResolver resolves queries for GQL.
type QueryResolver struct {
	Env GraphQLEnv
}

// NewGraphQLHandler is the hTTP handler used for handling GraphQL requests.
func NewGraphQLHandler(graphqlEnv GraphQLEnv) http.Handler {
	schemaData := schema.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	gqlSchema := graphql.MustParseSchema(schemaData, &QueryResolver{graphqlEnv}, opts...)
	return &relay.Handler{Schema: gqlSchema}
}

// NewUnauthenticatedGraphQLHandler is the hTTP handler used for handling unauthenticated GraphQL requests.
func NewUnauthenticatedGraphQLHandler(graphqlEnv GraphQLEnv) http.Handler {
	schemaData := unauthenticatedschema.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	gqlSchema := graphql.MustParseSchema(schemaData, &QueryResolver{graphqlEnv}, opts...)
	return &relay.Handler{Schema: gqlSchema}
}
