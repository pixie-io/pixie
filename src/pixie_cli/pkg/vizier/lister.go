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

package vizier

import (
	"context"
	"crypto/tls"
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"time"

	"github.com/gofrs/uuid"
	"gopkg.in/segmentio/analytics-go.v3"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	cliUtils "px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/utils"
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
	vc cloudpb.VizierClusterInfoClient
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
func (l *Lister) GetViziersInfo() ([]*cloudpb.ClusterInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())

	c, err := l.vc.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}
	return c.Clusters, nil
}

// GetVizierInfo returns information about a connected vizier.
func (l *Lister) GetVizierInfo(id uuid.UUID) ([]*cloudpb.ClusterInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())
	clusterIDPb := utils.ProtoFromUUID(id)

	c, err := l.vc.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{ID: clusterIDPb})
	if err != nil {
		return nil, err
	}
	return c.Clusters, nil
}

// GetVizierConnection gets connection information for the specified Vizier.
func (l *Lister) GetVizierConnection(id uuid.UUID) (*ConnectionInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())

	ci, err := l.vc.GetClusterConnectionInfo(ctx, &cloudpb.GetClusterConnectionInfoRequest{
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

		cliUtils.Infof("Selected Vizier address addr=%s", addr)
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
func (l *Lister) UpdateVizierConfig(req *cloudpb.UpdateClusterVizierConfigRequest) error {
	ctx := auth.CtxWithCreds(context.Background())
	_, err := l.vc.UpdateClusterVizierConfig(ctx, req)
	return err
}
