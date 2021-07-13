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

package main

import (
	"context"
	"fmt"

	"github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/utils"
)

func init() {
	pflag.String("profile_service", "profile-service.plc.svc.cluster.local:51500", "The profile service url (load balancer/list is ok)")
	pflag.String("auth_service", "auth-service.plc.svc.cluster.local:50100", "The auth service url (load balancer/list is ok)")
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
}

// NewProfileServiceClient creates a new profile RPC client stub.
func NewProfileServiceClient() (profilepb.ProfileServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	authChannel, err := grpc.Dial(viper.GetString("profile_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return profilepb.NewProfileServiceClient(authChannel), nil
}

// NewAuthClient creates a new auth RPC client stub.
func NewAuthClient() (authpb.AuthServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	authChannel, err := grpc.Dial(viper.GetString("auth_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return authpb.NewAuthServiceClient(authChannel), nil
}

func main() {
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()

	pc, err := NewProfileServiceClient()
	if err != nil {
		logrus.WithError(err).Fatal("Unable to connect to Profile Service")
	}

	ac, err := NewAuthClient()
	if err != nil {
		logrus.WithError(err).Fatal("Unable to connect to Auth Service")
	}

	// Setup credentials.
	claims := utils.GenerateJWTForService("API Service", viper.GetString("domain_name"))
	serviceAuthToken, err := utils.SignJWTClaims(claims, viper.GetString("jwt_signing_key"))
	if err != nil {
		logrus.WithError(err).Fatal("Unable to sign JWT claims")
	}
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	// Create the default organization.
	orgInfo := &authpb.CreateOrgAndInviteUserRequest_Org{
		DomainName: "default.com",
		OrgName:    "default",
	}

	// Ignore error, just try the org.
	org, _ := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{
		DomainName: orgInfo.DomainName,
	})
	if org != nil {
		logrus.Fatalf("Org '%s' with domain '%s' already exists. Remove the org from the database or change the org name.", orgInfo.OrgName, orgInfo.DomainName)
	}

	email := "admin@default.com"
	userInfo := &authpb.CreateOrgAndInviteUserRequest_User{
		Username:  email,
		FirstName: "admin",
		LastName:  "admin",
		Email:     email,
	}
	inviteLink, err := ac.CreateOrgAndInviteUser(ctx, &authpb.CreateOrgAndInviteUserRequest{
		Org:  orgInfo,
		User: userInfo,
	})
	if err != nil {
		logrus.WithError(err).Fatal("Failed to create org and invite link.")
	}

	// Log the InviteLink for the admin user.
	logrus.Infof("Please go to '%s' to set password for '%s'", inviteLink.InviteLink, userInfo.Email)
}
