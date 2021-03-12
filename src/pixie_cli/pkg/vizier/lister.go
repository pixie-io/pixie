package vizier

import (
	"context"
	"crypto/tls"
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"time"

	uuid "github.com/satori/go.uuid"
	"gopkg.in/segmentio/analytics-go.v3"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/auth"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/pxconfig"
	cliLog "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/utils"
)

const proxyIPAddr = "https://127.0.0.1:31068"

func selectVizierOrProxy(vizierAddr string) (string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
	defer cancel()

	selectedAddr := make(chan string)

	checkAddr := func(addr string) {
		tr := &http.Transport{
			TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		}
		client := &http.Client{Transport: tr}
		res, _ := client.Get(fmt.Sprintf("%s/%s", addr, "healthz"))
		if res != nil && res.StatusCode == http.StatusOK {
			selectedAddr <- addr
		}
	}
	go checkAddr(proxyIPAddr)
	go checkAddr(vizierAddr)

	select {
	case <-ctx.Done():
		return "", errors.New("Cannot contact vizier")
	case a := <-selectedAddr:
		cancel()
		return a, nil
	}
}

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
	vc, err := newVizierClusterInfoClient(cloudAddr)
	if err != nil {
		return nil, err
	}
	return &Lister{vc: vc}, nil
}

// GetViziersInfo returns information about connected viziers.
func (l *Lister) GetViziersInfo() ([]*cloudapipb.ClusterInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())

	c, err := l.vc.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}
	return c.Clusters, nil
}

// GetVizierInfo returns information about a connected vizier.
func (l *Lister) GetVizierInfo(id uuid.UUID) ([]*cloudapipb.ClusterInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())
	clusterIDPb := utils.ProtoFromUUID(id)

	c, err := l.vc.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{ID: clusterIDPb})
	if err != nil {
		return nil, err
	}
	return c.Clusters, nil
}

// GetVizierConnection gets connection information for the specified Vizier.
func (l *Lister) GetVizierConnection(id uuid.UUID) (*ConnectionInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())

	ci, err := l.vc.GetClusterConnectionInfo(ctx, &cloudapipb.GetClusterConnectionInfoRequest{
		ID: utils.ProtoFromUUID(id),
	})
	if err != nil {
		return nil, err
	}

	if len(ci.Token) == 0 {
		return nil, errors.New("invalid token received")
	}

	var u *url.URL
	if len(ci.IPAddress) > 0 {
		addr, err := selectVizierOrProxy(ci.IPAddress)
		if err != nil {
			return nil, err
		}

		cliLog.Infof("Selected Vizier address addr=%s", addr)
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Selected Vizier Address",
			Properties: analytics.NewProperties().
				Set("addr", addr),
		})

		u, err = url.Parse(addr)
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

// UpdateVizierConfig updates the config for the given Vizier.
func (l *Lister) UpdateVizierConfig(req *cloudapipb.UpdateClusterVizierConfigRequest) error {
	ctx := auth.CtxWithCreds(context.Background())
	_, err := l.vc.UpdateClusterVizierConfig(ctx, req)
	return err
}
