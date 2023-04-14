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

package bridge

import (
	"context"
	"fmt"
	"strings"
	"time"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/vzconn/vzconnpb"
	"px.dev/pixie/src/shared/services"
)

func init() {
	pflag.String("cloud_addr", "vzconn-service.plc.svc:51600", "The Pixie Cloud service url (load balancer/list is ok)")
}

func getCloudAddrFromCRD(vzOperator VizierOperatorInfo) (string, error) {
	vz, err := vzOperator.GetVizierCRD()
	if err != nil {
		return "", err
	}

	// When cloudConn connects to dev cloud, it should communicate directly with VZConn.
	cloudAddr := vz.Spec.CloudAddr
	devCloudNamespace := vz.Spec.DevCloudNamespace
	if devCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNamespace)
	}

	return cloudAddr, nil
}

// NewVZConnClient creates a new vzconn RPC client stub.
func NewVZConnClient(vzOperator VizierOperatorInfo) (vzconnpb.VZConnServiceClient, error) {
	ctxBg := context.Background()

	// Get the cloud address - first try the CRD, if it exists.
	// If that fails, pull it from the environment for Viziers that are not
	// running the operator yet.
	cloudAddr, err := getCloudAddrFromCRD(vzOperator)
	if err != nil {
		cloudAddr = viper.GetString("cloud_addr")
	}

	isInternal := strings.Contains(cloudAddr, ".svc.cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}
	dialOpts = append(dialOpts, []grpc.DialOption{grpc.WithBlock()}...)

	ctx, cancel := context.WithTimeout(ctxBg, 10*time.Second)
	defer cancel()
	ccChannel, err := grpc.DialContext(ctx, cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return vzconnpb.NewVZConnServiceClient(ccChannel), nil
}
