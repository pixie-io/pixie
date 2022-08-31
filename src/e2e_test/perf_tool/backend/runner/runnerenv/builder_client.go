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

package runnerenv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/e2e_test/perf_tool/backend/builder/builderpb"
	"px.dev/pixie/src/shared/services"
)

func init() {
	pflag.String("builder_service", "builder-service.px-perf.svc.cluster.local:50100", "The builder service url (load balancer/list is ok)")
}

// NewBuilderServiceClient creates a new gRPC client to the builder service.
func NewBuilderServiceClient() (builderpb.BuilderServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	authChannel, err := grpc.Dial(viper.GetString("builder_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return builderpb.NewBuilderServiceClient(authChannel), nil
}
