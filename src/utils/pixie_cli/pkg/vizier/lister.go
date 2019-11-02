package vizier

import (
	"context"
	"errors"
	"net/url"

	uuid "github.com/satori/go.uuid"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/utils"
)

// Lister allows fetching information about Viziers from the cloud.
type Lister struct {
	vc cloudapipb.VizierClusterInfoClient
}

// ConnectionInfo has connection info about a Vizier.
type ConnectionInfo struct {
	ID    uuid.UUID
	URL   *url.URL
	Token string
}

// NewLister returns a Lister.
func NewLister(cloudAddr string) (*Lister, error) {
	vc, err := newVizierInfoClient(cloudAddr)
	if err != nil {
		return nil, err
	}
	return &Lister{vc: vc}, nil
}

// GetViziersInfo returns information about connected viziers.
func (l *Lister) GetViziersInfo() ([]*cloudapipb.ClusterInfo, error) {
	ctx, err := ctxWithCreds(context.Background())
	if err != nil {
		return nil, err
	}

	c, err := l.vc.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}
	return c.Clusters, nil
}

// GetVizierConnection gets connection information for the specified Vizier.
func (l *Lister) GetVizierConnection(id uuid.UUID) (*ConnectionInfo, error) {
	ctx, err := ctxWithCreds(context.Background())
	if err != nil {
		return nil, err
	}

	ci, err := l.vc.GetClusterConnectionInfo(ctx, &cloudapipb.GetClusterConnectionInfoRequest{
		ID: utils.ProtoFromUUID(&id),
	})
	if err != nil {
		return nil, err
	}

	if len(ci.Token) == 0 {
		return nil, errors.New("invalid token received")
	}

	var u *url.URL
	if len(ci.IPAddress) > 0 {
		u, err = url.Parse(ci.IPAddress)
		if err != nil {
			return nil, err
		}
	}

	return &ConnectionInfo{
		ID:    id,
		URL:   u,
		Token: ci.Token,
	}, nil
}
