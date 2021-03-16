package pxapi

import (
	"context"
	"crypto/tls"
	"fmt"
	"net/url"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/metadata"

	"pixielabs.ai/pixielabs/src/api/go/pxapi/types"
	cloudapipb "pixielabs.ai/pixielabs/src/api/public/cloudapipb"
	vizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
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
	vizier, err := c.GetVizierInfo(ctx, vizierID)
	if err != nil {
		return nil, err
	}

	vzConn := c.grpcConn
	if vizier.DirectAccess {
		connInfo, err := c.getConnectionInfo(ctx, vizierID)
		if err != nil {
			return nil, err
		}

		c.bearerAuth = connInfo.Token
		parsedURL, err := url.Parse(connInfo.IPAddress)
		if err != nil {
			return nil, err
		}

		tlsConfig := &tls.Config{InsecureSkipVerify: false}
		creds := credentials.NewTLS(tlsConfig)

		conn, err := grpc.Dial(parsedURL.Host, grpc.WithTransportCredentials(creds))
		if err != nil {
			return nil, err
		}
		vzConn = conn
	}

	// Now create the actual client.
	vzClient := &VizierClient{
		cloud:    c,
		vizierID: vizierID,
		vzClient: vizierapipb.NewVizierServiceClient(vzConn),
	}

	return vzClient, nil
}
