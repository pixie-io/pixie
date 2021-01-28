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
 */
package pxapi

import (
	"context"
	"crypto/tls"
	"fmt"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/metadata"
	"go.withpixie.dev/pixie/src/api/go/pxapi/errdefs"
	"go.withpixie.dev/pixie/src/api/go/pxapi/types"
	cloudapipb "go.withpixie.dev/pixie/src/api/public/cloudapipb"
	vizierapipb "go.withpixie.dev/pixie/src/api/public/vizierapipb"
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

	grpcConn *grpc.ClientConn
	cmClient cloudapipb.ClusterManagerClient
	vizier   vizierapipb.VizierServiceClient
}

// NewClient creates a new Pixie API Client.
func NewClient(ctx context.Context, opts ...ClientOption) (*Client, error) {
	c := &Client{
		cloudAddr: defaultCloudAddr,
	}

	for _, opt := range opts {
		opt(c)
	}

	if err := c.init(ctx); err != nil {
		return nil, err
	}
	return c, nil
}

func (c *Client) init(ctx context.Context) error {
	isInternal := strings.ContainsAny(c.cloudAddr, "cluster.local")

	tlsConfig := &tls.Config{InsecureSkipVerify: isInternal}
	creds := credentials.NewTLS(tlsConfig)

	conn, err := grpc.Dial(c.cloudAddr, grpc.WithTransportCredentials(creds))
	if err != nil {
		return err
	}

	c.grpcConn = conn
	c.cmClient = cloudapipb.NewClusterManagerClient(conn)

	c.vizier = vizierapipb.NewVizierServiceClient(conn)
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
	// We need to make a request to find out if vizier is direct mode or not.
	// TODO(zasgar): We can use the most restrictive API to request data for just one vizier.
	viziers, err := c.ListViziers(ctx)
	if err != nil {
		return nil, err
	}

	found := false
	directAccess := false
	for _, v := range viziers {
		if v.ID == vizierID && v.DirectAccess == false {
			found = true
			directAccess = v.DirectAccess
		}
	}

	if !found {
		return nil, errdefs.ErrUnImplemented
	}

	// TODO(zasgar): make call to grab the token if we are using direct mode access.
	if directAccess {
	}

	// Now create the actual client.
	vzClient := &VizierClient{
		cloud:        c,
		vizierID:     vizierID,
		directAccess: directAccess,
		accessToken:  "",
		vzClient:     vizierapipb.NewVizierServiceClient(c.grpcConn),
	}

	return vzClient, nil
}
