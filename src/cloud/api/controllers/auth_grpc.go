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
	"errors"
	"fmt"

	"github.com/spf13/viper"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/auth/authpb"
)

// AuthServer logs users
type AuthServer struct {
	AuthClient authpb.AuthServiceClient
}

// Login logs the user in by taking an access token from the auth provider, or using an API key.
func (a *AuthServer) Login(ctx context.Context, req *cloudpb.LoginRequest) (*cloudpb.LoginReply, error) {
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, err
	}
	aCtx := metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	var token string
	var expiresAt int64

	// Use access token to login, if specified.
	if req.AccessToken != "" {
		resp, err := a.AuthClient.Login(aCtx, &authpb.LoginRequest{
			AccessToken: req.AccessToken,
		})
		if err == nil {
			token = resp.Token
			expiresAt = resp.ExpiresAt
		}
	}

	md, mdOK := metadata.FromIncomingContext(ctx)
	apiKey := md.Get("pixie-api-key")
	// If API key is in headers, try to login with API key.
	if token == "" && mdOK && len(apiKey) == 1 {
		apiKeyResp, err := a.AuthClient.GetAugmentedTokenForAPIKey(aCtx, &authpb.GetAugmentedTokenForAPIKeyRequest{
			APIKey: apiKey[0],
		})
		if err == nil {
			token = apiKeyResp.Token
			expiresAt = apiKeyResp.ExpiresAt
		}
	}

	if token == "" {
		return nil, errors.New("Unauthorized")
	}

	return &cloudpb.LoginReply{
		Token:     token,
		ExpiresAt: expiresAt,
	}, nil
}
