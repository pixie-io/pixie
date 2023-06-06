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

package pxapi

import (
	"context"
	"crypto/tls"
	"fmt"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/metadata"
        "google.golang.org/grpc/credentials/insecure"

	"px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/api/go/pxapi/utils"
	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vizierpb"
)

const (
	defaultCloudAddr = "work.withpixie.ai:443"
)

// TableRecordHandler is an interface that processes a table record-wise.
type TableRecordHandler interface {
	// HandleInit is called to initialize the table handler interface.
	HandleInit(ctx context.Context, metadata types.TableMetadata) error
	// HandleRecord is called whenever a new row of the data is available.
	HandleRecord(ctx context.Context, record *types.Record) error
	// HandleDone is called when the table streaming has been completed.
	HandleDone(ctx context.Context) error
}

// TableMuxer is an interface to route tables to the correct handler.
type TableMuxer interface {
	// AcceptTable is passed the table information, if nil is returned then the table stream is ignored.
	AcceptTable(ctx context.Context, metadata types.TableMetadata) (TableRecordHandler, error)
}

// Client is the base client to use pixie cloud + vizier.
type Client struct {
	apiKey     string
	bearerAuth string

	cloudAddr string

	useEncryption          bool
	disableTLSVerification bool

	grpcConn *grpc.ClientConn
	cmClient cloudpb.VizierClusterInfoClient
	vizier   vizierpb.VizierServiceClient
}

// NewClient creates a new Pixie API Client.
func NewClient(ctx context.Context, opts ...ClientOption) (*Client, error) {
	c := &Client{
		cloudAddr:     defaultCloudAddr,
		useEncryption: true,
	}

	for _, opt := range opts {
		opt(c)
	}

	if (strings.Contains(c.cloudAddr, "0.0.0.0") == true) {
		if err := c.initLocalClient(ctx); err != nil {
			return nil, err
		}
		return c, nil
	}

	if err := c.init(ctx); err != nil {
		return nil, err
	}
	return c, nil
}


// initLocalClient is for establishing gRPC connection to standalonePEM
func (c *Client) initLocalClient(ctx context.Context) error {
        conn, err := grpc.Dial(c.cloudAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
        if err != nil {
                return err
        }

        c.grpcConn = conn
        c.cmClient = cloudpb.NewVizierClusterInfoClient(conn)

        c.vizier = vizierpb.NewVizierServiceClient(conn)
        return nil
}

func (c *Client) init(ctx context.Context) error {
	tlsConfig := &tls.Config{InsecureSkipVerify: c.disableTLSVerification}
	creds := credentials.NewTLS(tlsConfig)

	conn, err := grpc.Dial(c.cloudAddr, grpc.WithTransportCredentials(creds))
	if err != nil {
		return err
	}

	c.grpcConn = conn
	c.cmClient = cloudpb.NewVizierClusterInfoClient(conn)

	c.vizier = vizierpb.NewVizierServiceClient(conn)
	return nil
}

func (c *Client) cloudCtxWithMD(ctx context.Context) context.Context {
	ctx = metadata.AppendToOutgoingContext(ctx,
		"pixie-api-client", "go")

	if len(c.apiKey) > 0 {
		ctx = metadata.AppendToOutgoingContext(ctx,
			"pixie-api-key", c.apiKey)
	}

	if len(c.bearerAuth) > 0 {
		ctx = metadata.AppendToOutgoingContext(ctx,
			"authorization", fmt.Sprintf("bearer %s", c.bearerAuth))
	}
	return ctx
}

// NewVizierClient creates a new vizier client, for the passed in vizierID.
func (c *Client) NewVizierClient(ctx context.Context, vizierID string) (*VizierClient, error) {
	var err error
	vzConn := c.grpcConn

	var encOpts, decOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions
	if c.useEncryption {
		encOpts, decOpts, err = utils.CreateEncryptionOptions()
		if err != nil {
			return nil, err
		}
	}

	// Now create the actual client.
	vzClient := &VizierClient{
		cloud:    c,
		encOpts:  encOpts,
		decOpts:  decOpts,
		vizierID: vizierID,
		vzClient: vizierpb.NewVizierServiceClient(vzConn),
	}

	return vzClient, nil
}
