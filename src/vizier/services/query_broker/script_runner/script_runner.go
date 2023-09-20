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

package scriptrunner

import (
	"context"
	"fmt"
	"io"
	"sync"
	"time"

	"google.golang.org/grpc/status"

	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/common/base/statuspb"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"gopkg.in/yaml.v2"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/shared/cvmsgs"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/scripts"
	svcutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/utils/messagebus"
)

var (
	// CronScriptChecksumRequestChannel is the NATS channel to make checksum requests to.
	CronScriptChecksumRequestChannel = messagebus.V2CTopic(cvmsgs.CronScriptChecksumRequestChannel)
	// CronScriptChecksumResponseChannel is the NATS channel that checksum responses are published to.
	CronScriptChecksumResponseChannel = messagebus.C2VTopic(cvmsgs.CronScriptChecksumResponseChannel)
	// GetCronScriptsRequestChannel is the NATS channel script requests are sent to.
	GetCronScriptsRequestChannel = messagebus.V2CTopic(cvmsgs.GetCronScriptsRequestChannel)
	// GetCronScriptsResponseChannel is the NATS channel that script responses are published to.
	GetCronScriptsResponseChannel = messagebus.C2VTopic(cvmsgs.GetCronScriptsResponseChannel)
	// CronScriptUpdatesChannel is the NATS channel that any cron script updates are published to.
	CronScriptUpdatesChannel = messagebus.C2VTopic(cvmsgs.CronScriptUpdatesChannel)
	// CronScriptUpdatesResponseChannel is the NATS channel that script updates are published to.
	CronScriptUpdatesResponseChannel = messagebus.V2CTopic(cvmsgs.CronScriptUpdatesResponseChannel)
	natsWaitTimeout                  = 2 * time.Minute
	defaultOTelTimeoutS              = int64(5)
)

// ScriptRunner tracks registered cron scripts and runs them according to schedule.
type ScriptRunner struct {
	csClient   metadatapb.CronScriptStoreServiceClient
	vzClient   vizierpb.VizierServiceClient
	signingKey string

	runnerMap   map[uuid.UUID]*runner
	runnerMapMu sync.Mutex

	cancelFunc func()
	once       sync.Once
	updatesCh  chan *cvmsgspb.CronScriptUpdate
	baseCtx    context.Context
	sources    []Source
}

// New creates a new script runner.
func New(
	csClient metadatapb.CronScriptStoreServiceClient,
	vzClient vizierpb.VizierServiceClient,
	signingKey string,
	scriptSources ...Source,
) *ScriptRunner {
	baseCtx, cancel := context.WithCancel(context.Background())
	return &ScriptRunner{
		csClient:   csClient,
		vzClient:   vzClient,
		signingKey: signingKey,

		runnerMap: make(map[uuid.UUID]*runner),

		cancelFunc: cancel,
		once:       sync.Once{},
		updatesCh:  make(chan *cvmsgspb.CronScriptUpdate, 4096),
		baseCtx:    baseCtx,
		sources:    scriptSources,
	}
}

// Stop performs any necessary cleanup before shutdown.
func (s *ScriptRunner) Stop() {
	s.once.Do(func() {
		s.cancelFunc()
		for _, source := range s.sources {
			source.Stop()
		}
		s.runnerMapMu.Lock()
		for _, runner := range s.runnerMap {
			runner.stop()
		}
		s.runnerMapMu.Unlock()
	})
}

// SyncScripts syncs the known set of scripts in Vizier with scripts in Cloud.
func (s *ScriptRunner) SyncScripts() error {
	for _, source := range s.sources {
		initialScripts, err := source.Start(s.baseCtx, s.updatesCh)
		if err != nil {
			return err
		}
		for id, script := range initialScripts {
			s.upsertScript(uuid.FromStringOrNil(id), script)
		}
	}
	s.processUpdates()
	return nil
}

func (s *ScriptRunner) processUpdates() {
	for {
		select {
		case <-s.baseCtx.Done():
			return
		case update := <-s.updatesCh:
			switch update.Msg.(type) {
			case *cvmsgspb.CronScriptUpdate_UpsertReq:
				req := update.GetUpsertReq()
				s.upsertScript(utils.UUIDFromProtoOrNil(req.Script.ID), req.Script)
			case *cvmsgspb.CronScriptUpdate_DeleteReq:
				req := update.GetDeleteReq()
				s.deleteScript(utils.UUIDFromProtoOrNil(req.ScriptID))
			default:
				log.Error("Received unknown message for cronScriptUpdate")
			}
		}
	}
}

func (s *ScriptRunner) upsertScript(id uuid.UUID, script *cvmsgspb.CronScript) {
	s.runnerMapMu.Lock()
	defer s.runnerMapMu.Unlock()
	if v, ok := s.runnerMap[id]; ok {
		v.stop()
		delete(s.runnerMap, id)
	}
	r := newRunner(script, s.vzClient, s.signingKey, id, s.csClient)
	s.runnerMap[id] = r
	go r.start()
}

func (s *ScriptRunner) deleteScript(id uuid.UUID) {
	s.runnerMapMu.Lock()
	defer s.runnerMapMu.Unlock()
	v, ok := s.runnerMap[id]
	if !ok {
		return
	}
	v.stop()
	delete(s.runnerMap, id)
}

// Logic for "runners" which handle the script execution.
type runner struct {
	cronScript *cvmsgspb.CronScript
	config     *scripts.Config

	lastRun time.Time

	csClient   metadatapb.CronScriptStoreServiceClient
	vzClient   vizierpb.VizierServiceClient
	signingKey string

	done chan struct{}
	once sync.Once

	scriptID uuid.UUID
}

func newRunner(script *cvmsgspb.CronScript, vzClient vizierpb.VizierServiceClient, signingKey string, id uuid.UUID, csClient metadatapb.CronScriptStoreServiceClient) *runner {
	// Parse config YAML into struct.
	var config scripts.Config
	err := yaml.Unmarshal([]byte(script.Configs), &config)
	if err != nil {
		log.WithError(err).Error("Failed to parse config YAML")
	}

	return &runner{
		cronScript: script,
		done:       make(chan struct{}),
		once:       sync.Once{},
		csClient:   csClient,
		vzClient:   vzClient,
		signingKey: signingKey,
		config:     &config,
		scriptID:   id,
	}
}

// VizierStatusToStatus converts the Vizier status to the internal storable version statuspb.Status
func VizierStatusToStatus(s *vizierpb.Status) (*statuspb.Status, error) {
	var ctxAny *types.Any
	var err error
	if len(s.ErrorDetails) > 0 {
		errorPb := &compilerpb.CompilerErrorGroup{
			Errors: make([]*compilerpb.CompilerError, len(s.ErrorDetails)),
		}
		for i, ed := range s.ErrorDetails {
			e := ed.GetCompilerError()
			errorPb.Errors[i] = &compilerpb.CompilerError{
				Error: &compilerpb.CompilerError_LineColError{
					LineColError: &compilerpb.LineColError{
						Line:    e.Line,
						Column:  e.Column,
						Message: e.Message,
					},
				},
			}
		}
		ctxAny, err = types.MarshalAny(errorPb)
		if err != nil {
			return nil, err
		}
	}
	return &statuspb.Status{
		ErrCode: statuspb.Code(s.Code),
		Msg:     s.Message,
		Context: ctxAny,
	}, nil
}

func (r *runner) runScript(scriptPeriod time.Duration) {
	claims := svcutils.GenerateJWTForService("query_broker", "vizier")
	token, _ := svcutils.SignJWTClaims(claims, r.signingKey)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", token))

	var otelEndpoint *vizierpb.Configs_OTelEndpointConfig
	if r.config != nil && r.config.OtelEndpointConfig != nil {
		otelEndpoint = &vizierpb.Configs_OTelEndpointConfig{
			URL:      r.config.OtelEndpointConfig.URL,
			Headers:  r.config.OtelEndpointConfig.Headers,
			Insecure: r.config.OtelEndpointConfig.Insecure,
			Timeout:  defaultOTelTimeoutS,
		}
	}

	// We set the time 1 second in the past to cover colletor latency and request latencies
	// which can cause data overlaps or cause data to be missed.
	startTime := r.lastRun.Add(-time.Second)
	endTime := startTime.Add(scriptPeriod)
	r.lastRun = time.Now()
	execScriptClient, err := r.vzClient.ExecuteScript(ctx, &vizierpb.ExecuteScriptRequest{
		QueryStr: r.cronScript.Script,
		Configs: &vizierpb.Configs{
			OTelEndpointConfig: otelEndpoint,
			PluginConfig: &vizierpb.Configs_PluginConfig{
				StartTimeNs: startTime.UnixNano(),
				EndTimeNs:   endTime.UnixNano(),
			},
		},
		QueryName: "cron_" + r.scriptID.String(),
	})
	if err != nil {
		log.WithError(err).Error("Failed to execute cronscript")
		return
	}
	for {
		resp, err := execScriptClient.Recv()
		if err == io.EOF {
			break
		}
		if err != nil {
			grpcStatus, _ := status.FromError(err)

			tsPb, err := types.TimestampProto(startTime)
			if err != nil {
				log.WithError(err).Error("Error while creating timestamp proto")
			}

			_, err = r.csClient.RecordExecutionResult(ctx, &metadatapb.RecordExecutionResultRequest{
				ScriptID:  utils.ProtoFromUUID(r.scriptID),
				Timestamp: tsPb,
				Result: &metadatapb.RecordExecutionResultRequest_Error{
					Error: &statuspb.Status{
						ErrCode: statuspb.Code(grpcStatus.Code()),
						Msg:     grpcStatus.Message(),
					},
				},
			})
			if err != nil {
				grpcStatus, ok := status.FromError(err)
				if !ok || grpcStatus.Code() != codes.Unavailable {
					log.WithError(err).Error("Error while recording cron script execution error")
				}
			}
			break
		}

		if vzStatus := resp.GetStatus(); vzStatus != nil {
			tsPb, err := types.TimestampProto(startTime)
			if err != nil {
				log.WithError(err).Error("Error while creating timestamp proto")
			}
			st, err := VizierStatusToStatus(vzStatus)
			if err != nil {
				log.WithError(err).Error("Error converting status")
			}

			_, err = r.csClient.RecordExecutionResult(ctx, &metadatapb.RecordExecutionResultRequest{
				ScriptID:  utils.ProtoFromUUID(r.scriptID),
				Timestamp: tsPb,
				Result: &metadatapb.RecordExecutionResultRequest_Error{
					Error: st,
				},
			})
			if err != nil {
				grpcStatus, ok := status.FromError(err)
				if !ok || grpcStatus.Code() != codes.Unavailable {
					log.WithError(err).Error("Error while recording cron script execution error")
				}
			}
			break
		}
		if data := resp.GetData(); data != nil {
			tsPb, err := types.TimestampProto(startTime)
			if err != nil {
				log.WithError(err).Error("Error while creating timestamp proto")
			}
			stats := data.GetExecutionStats()
			if stats == nil {
				continue
			}
			_, err = r.csClient.RecordExecutionResult(ctx, &metadatapb.RecordExecutionResultRequest{
				ScriptID:  utils.ProtoFromUUID(r.scriptID),
				Timestamp: tsPb,
				Result: &metadatapb.RecordExecutionResultRequest_ExecutionStats{
					ExecutionStats: &metadatapb.ExecutionStats{
						ExecutionTimeNs:   stats.Timing.ExecutionTimeNs,
						CompilationTimeNs: stats.Timing.CompilationTimeNs,
						BytesProcessed:    stats.BytesProcessed,
						RecordsProcessed:  stats.RecordsProcessed,
					},
				},
			})
			if err != nil {
				grpcStatus, ok := status.FromError(err)
				if !ok || grpcStatus.Code() != codes.Unavailable {
					log.WithError(err).Error("Error recording execution stats")
				}
			}
			break
		}
	}
}

func (r *runner) start() {
	if r.cronScript.FrequencyS <= 0 {
		return
	}
	scriptPeriod := time.Duration(r.cronScript.FrequencyS) * time.Second
	ticker := time.NewTicker(scriptPeriod)
	r.lastRun = time.Now()

	go func() {
		defer ticker.Stop()
		for {
			select {
			case <-r.done:
				return
			case <-ticker.C:
				r.runScript(scriptPeriod)
			}
		}
	}()
}

func (r *runner) stop() {
	r.once.Do(func() {
		close(r.done)
	})
}
