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

package apienv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	"px.dev/pixie/src/shared/services"
)

func init() {
	pflag.String("project_manager_service", "kubernetes:///project-manager-service.plc:50300", "The projectmanager service url (load balancer/list is ok)")
}

// NewProjectManagerServiceClient creates a new auth RPC client stub.
func NewProjectManagerServiceClient() (projectmanagerpb.ProjectManagerServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	projectManagerChannel, err := grpc.Dial(viper.GetString("project_manager_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return projectmanagerpb.NewProjectManagerServiceClient(projectManagerChannel), nil
}
