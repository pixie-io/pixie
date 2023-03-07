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

package checks

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"text/template"
	"time"

	"github.com/cenkalti/backoff/v4"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/api/go/pxapi"
	"px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
)

type pxlHealthCheck struct {
	pxCtx *pixie.Context
	spec  *experimentpb.PxLHealthCheck

	script string

	scriptSuccess bool
}

var _ HealthCheck = &pxlHealthCheck{}

// NewPxLHealthCheck returns a new HealthCheck based on a 'success' column of a PxL script being true.
func NewPxLHealthCheck(pxCtx *pixie.Context, spec *experimentpb.PxLHealthCheck) HealthCheck {
	return &pxlHealthCheck{
		pxCtx: pxCtx,
		spec:  spec,
	}
}

// Name returns a printable name for this healthcheck.
func (hc *pxlHealthCheck) Name() string {
	return "PxL Healthcheck"
}

// Wait waits for the PxL script to successfully run, and for the 'success' column to be true.
func (hc *pxlHealthCheck) Wait(ctx context.Context, clusterCtx *cluster.Context, clusterSpec *experimentpb.ClusterSpec) error {
	if err := hc.prepareScript(clusterSpec); err != nil {
		return err
	}

	expBackoff := backoff.NewExponentialBackOff()
	expBackoff.InitialInterval = time.Second
	expBackoff.MaxElapsedTime = 10 * time.Minute
	bo := backoff.WithContext(expBackoff, ctx)

	op := func() error {
		hc.scriptSuccess = false
		return hc.runHealthCheck(ctx)
	}
	notify := func(err error, dur time.Duration) {
		log.WithError(err).Tracef("failed pxl healthcheck, retrying in %v", dur.Round(time.Second))
	}
	err := backoff.RetryNotify(op, bo, notify)
	if err != nil {
		return err
	}
	return nil
}

func (hc *pxlHealthCheck) runHealthCheck(ctx context.Context) error {
	ctx, cancel := context.WithTimeout(ctx, time.Minute)
	defer cancel()
	vz, err := hc.pxCtx.NewVizierClient()
	if err != nil {
		log.WithError(err).Trace("failed to create vizier client")
		return err
	}
	resultSet, err := vz.ExecuteScript(ctx, hc.script, hc)
	if err != nil {
		return err
	}
	if err := resultSet.Stream(); err != nil {
		return err
	}
	if !hc.scriptSuccess {
		return errors.New("healthcheck script executed successfully, but returned false (unhealthy)")
	}
	return nil
}

func (hc *pxlHealthCheck) prepareScript(clusterSpec *experimentpb.ClusterSpec) error {
	t, err := template.New("").Parse(hc.spec.Script)
	if err != nil {
		return err
	}
	buf := &strings.Builder{}
	if err := t.Execute(buf, clusterSpec); err != nil {
		return err
	}
	hc.script = buf.String()
	return nil
}

// AcceptTable implements pxapi.TableMuxer.
func (hc *pxlHealthCheck) AcceptTable(context.Context, types.TableMetadata) (pxapi.TableRecordHandler, error) {
	return hc, nil
}

// HandleInit implements pxapi.TableRecordHandler.
func (hc *pxlHealthCheck) HandleInit(context.Context, types.TableMetadata) error {
	return nil
}

// HandleRecord implements pxapi.TableRecordHandler.
func (hc *pxlHealthCheck) HandleRecord(ctx context.Context, r *types.Record) error {
	d := r.GetDatum(hc.spec.SuccessColumn)
	if d == nil || d.Type() != vizierpb.BOOLEAN {
		return backoff.Permanent(fmt.Errorf("success_column: '%s' is not a boolean column in the output", hc.spec.SuccessColumn))
	}
	success := d.(*types.BooleanValue).Value()
	hc.scriptSuccess = success
	return nil
}

// HandleDone implements pxapi.TableRecordHandler.
func (hc *pxlHealthCheck) HandleDone(context.Context) error {
	return nil
}
