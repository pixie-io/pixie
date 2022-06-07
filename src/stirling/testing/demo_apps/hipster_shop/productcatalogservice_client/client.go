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

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	pb "px.dev/pixie/src/stirling/testing/demo_apps/hipster_shop/proto"
)

func main() {
	ctx := context.Background()
	addr := "localhost:3550"
	conn, err := grpc.Dial(addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return
	}
	defer conn.Close()
	client := pb.NewProductCatalogServiceClient(conn)

	_, err = client.ListProducts(ctx, &pb.Empty{})
	if err != nil {
		return
	}

	_, err = client.GetProduct(ctx, &pb.GetProductRequest{Id: "OLJCESPC7Z"})
	if err != nil {
		return
	}

	_, err = client.SearchProducts(ctx, &pb.SearchProductsRequest{Query: "typewriter"})
	if err != nil {
		return
	}

	log.Info("Client terminated")
}
