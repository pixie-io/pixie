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

package metrics

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"sync"
	"text/template"
	"time"

	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/go/pxapi"
	pxtypes "px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
)

type pxlScriptRecorderImpl struct {
	pxCtx    *pixie.Context
	spec     *experimentpb.PxLScriptSpec
	resultCh chan<- *ResultRow
	eg       *errgroup.Group

	wg     sync.WaitGroup
	cancel context.CancelFunc
	vz     *pxapi.VizierClient
	script string
}

// Start the pxlScriptRecorder.
func (r *pxlScriptRecorderImpl) Start(ctx context.Context) error {
	vz, err := r.pxCtx.NewVizierClient()
	if err != nil {
		return err
	}
	r.vz = vz

	ctx, cancel := context.WithCancel(ctx)
	r.cancel = cancel

	err = r.prepareScript()
	if err != nil {
		return err
	}

	if r.spec.Streaming {
		r.wg.Add(1)
		r.eg.Go(func() error {
			defer r.wg.Done()
			err := r.runStreamingScript(ctx)
			if err != nil {
				err = fmt.Errorf("error running streaming PxL script: %w", err)
			}
			return err
		})
	} else {
		r.wg.Add(1)
		r.eg.Go(func() error {
			defer r.wg.Done()
			err := r.runPeriodicScript(ctx)
			if err != nil {
				err = fmt.Errorf("error running periodic PxL script: %w", err)
			}
			return err
		})
	}
	return nil
}

// Close stops the recorder.
func (r *pxlScriptRecorderImpl) Close() {
	r.cancel()
	r.wg.Wait()
}

func (r *pxlScriptRecorderImpl) prepareScript() error {
	t, err := template.New("").Parse(r.spec.Script)
	if err != nil {
		return err
	}
	var buf strings.Builder
	err = t.Execute(&buf, r.spec)
	if err != nil {
		return err
	}
	r.script = buf.String()
	return nil
}

func (r *pxlScriptRecorderImpl) runStreamingScript(ctx context.Context) error {
	err := r.executeScript(ctx)
	if err != nil {
		return err
	}
	return nil
}

func errIsCanceled(err error) bool {
	if err == nil {
		return false
	}
	if s, ok := status.FromError(err); ok && (s.Code() == codes.Canceled) {
		return true
	}
	return false
}

func (r *pxlScriptRecorderImpl) runPeriodicScript(ctx context.Context) error {
	d, err := types.DurationFromProto(r.spec.CollectionPeriod)
	if err != nil {
		return err
	}
	t := time.NewTicker(d)

	for {
		select {
		case <-ctx.Done():
			return nil
		case <-t.C:
			err := r.executeScript(ctx)
			if err != nil {
				return err
			}
		}
	}
}

func (r *pxlScriptRecorderImpl) executeScript(ctx context.Context) error {
	rs, err := r.vz.ExecuteScript(ctx, r.script, r)
	if err != nil {
		log.WithError(err).Error("failed to ExecuteScript for pxl script recorder")
		return err
	}
	defer rs.Close()
	if err := rs.Stream(); err != nil && !errIsCanceled(err) {
		log.WithError(err).Error("pxl script recorder script execution error")
		return err
	}
	return nil
}

// AcceptTable implements the pxapi.TableMuxer interface.
func (r *pxlScriptRecorderImpl) AcceptTable(ctx context.Context, metadata pxtypes.TableMetadata) (pxapi.TableRecordHandler, error) {
	specs := make([]*experimentpb.PxLScriptOutputSpec, 0)
	if specsForAll, ok := r.spec.TableOutputs["*"]; ok {
		specs = append(specs, specsForAll.Outputs...)
	}
	tblSpecList, ok := r.spec.TableOutputs[metadata.Name]
	if ok {
		specs = append(specs, tblSpecList.Outputs...)
	}
	handlers := make([]pxapi.TableRecordHandler, 0)
	for _, outputSpec := range specs {
		h, err := newHandlerFromSpec(outputSpec, r.resultCh)
		if err != nil {
			return nil, err
		}
		handlers = append(handlers, h)
	}
	return &recordHandlerProxy{
		handlers: handlers,
	}, nil
}

type recordHandlerProxy struct {
	handlers []pxapi.TableRecordHandler
}

// HandleInit implements the pxapi.TableRecordHandler interface.
func (p *recordHandlerProxy) HandleInit(ctx context.Context, metadata pxtypes.TableMetadata) error {
	for _, h := range p.handlers {
		if err := h.HandleInit(ctx, metadata); err != nil {
			return err
		}
	}
	return nil
}

// HandleRecord implements the pxapi.TableRecordHandler interface.
func (p *recordHandlerProxy) HandleRecord(ctx context.Context, r *pxtypes.Record) error {
	for _, h := range p.handlers {
		if err := h.HandleRecord(ctx, r); err != nil {
			return err
		}
	}
	return nil
}

// HandleDone implements the pxapi.TableRecordHandler interface.
func (p *recordHandlerProxy) HandleDone(ctx context.Context) error {
	for _, h := range p.handlers {
		if err := h.HandleDone(ctx); err != nil {
			return err
		}
	}
	return nil
}

func newHandlerFromSpec(spec *experimentpb.PxLScriptOutputSpec, resultCh chan<- *ResultRow) (pxapi.TableRecordHandler, error) {
	switch spec.OutputSpec.(type) {
	case *experimentpb.PxLScriptOutputSpec_SingleMetric:
		return &singleMetricHandler{
			resultCh: resultCh,
			spec:     spec.GetSingleMetric(),
		}, nil
	case *experimentpb.PxLScriptOutputSpec_DataLossCounter:
		return &dataLossHandler{
			resultCh: resultCh,
			spec:     spec.GetDataLossCounter(),
		}, nil
	}
	return nil, errors.New("invalid pxl script output spec type")
}
