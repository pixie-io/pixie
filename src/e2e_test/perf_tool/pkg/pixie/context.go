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

package pixie

import (
	"bytes"
	"errors"
	"fmt"
	"os/exec"
	"strings"

	"github.com/gofrs/uuid"
	"golang.org/x/net/context"

	"px.dev/pixie/src/api/go/pxapi"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

// Context stores the information necessary to connect to Pixie Cloud, and optionally a particular vizier.
type Context struct {
	apiKey    string
	cloudAddr string

	clusterID uuid.UUID

	pxClient *pxapi.Client
}

// NewContext creates a new Context.
func NewContext(apiKey string, cloudAddr string) *Context {
	ctx := &Context{
		apiKey:    apiKey,
		cloudAddr: cloudAddr,
	}
	return ctx
}

// SetClusterID sets the cluster ID associated with this Context.
// This is called by the pxDeploy step that deploys Pixie to the cluster.
func (ctx *Context) SetClusterID(id uuid.UUID) {
	ctx.clusterID = id
}

// AddEnv adds the necessary environment variables to a exec.Cmd,
// such that the command will connect to the Pixie cloud specified by this context.
func (ctx *Context) AddEnv(cmd *exec.Cmd) {
	cmd.Env = append(cmd.Environ(),
		fmt.Sprintf("PX_CLOUD_ADDR=%s", ctx.cloudAddr),
		fmt.Sprintf("PX_API_KEY=%s", ctx.apiKey),
	)
}

// NewVizierClient creates a new pxapi VizierClient to the cluster associated with this context.
// If WithClusterID was not specified, this will error.
func (ctx *Context) NewVizierClient() (*pxapi.VizierClient, error) {
	if (ctx.clusterID == uuid.UUID{}) {
		return nil, errors.New("must call SetClusterID before calling NewVizierClient on Context")
	}
	if ctx.pxClient == nil {
		c, err := pxapi.NewClient(context.Background(), pxapi.WithCloudAddr(ctx.cloudAddr), pxapi.WithAPIKey(ctx.apiKey))
		if err != nil {
			return nil, err
		}
		ctx.pxClient = c
	}

	vz, err := ctx.pxClient.NewVizierClient(context.Background(), ctx.clusterID.String())
	if err != nil {
		return nil, err
	}
	return vz, nil
}

// RunPXCmd calls out to the `px` cli binary to run the given command, using the context's api key and cloud addr.
func (ctx *Context) RunPXCmd(clusterCtx *cluster.Context, args ...string) ([]byte, error) {
	cmd := exec.Command("px", args...)
	clusterCtx.AddEnv(cmd)
	ctx.AddEnv(cmd)
	var stderr bytes.Buffer
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	err := cmd.Run()
	if err != nil {
		sanitizedStderr := removeAPIKey(stderr.String())
		return nil, fmt.Errorf("failed to run `px %s`: %w, stderr: %s", strings.Join(args, " "), err, sanitizedStderr)
	}
	return stdout.Bytes(), nil
}

func removeAPIKey(stderr string) string {
	lines := strings.Split(stderr, "\n")
	sanitized := []string{}
	for _, l := range lines {
		if !strings.Contains(l, "PX_API_KEY") {
			sanitized = append(sanitized, l)
		}
	}
	return strings.Join(sanitized, "\n")
}
