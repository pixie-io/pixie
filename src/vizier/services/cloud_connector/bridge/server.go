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

package bridge

import (
	"context"
	"errors"
	"fmt"
	"io"
	"math"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	batchv1 "k8s.io/api/batch/v1"
	k8sErrors "k8s.io/apimachinery/pkg/api/errors"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/cloud/vzconn/vzconnpb"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/shared/cvmsgs"
	"px.dev/pixie/src/shared/cvmsgspb"
	vzstatus "px.dev/pixie/src/shared/status"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/cloud_connector/vzmetrics"
	"px.dev/pixie/src/vizier/utils/messagebus"
)

const (
	// NATSBackoffInitialInterval is the initial interval at which to start the backoff retry.
	NATSBackoffInitialInterval = 30 * time.Second
	// NATSBackoffMultipler is the multiplier for the backoff interval.
	NATSBackoffMultipler = 2
	// NATSBackoffMaxElapsedTime is the maximum elapsed time that we should retry.
	NATSBackoffMaxElapsedTime = 10 * time.Minute
	logChunkSize              = 500
	operatorMessage           = "You are running the non-operator version of Pixie. The operator version of Pixie has additional features that help you manage Pixie and keep it up-to-date. To update to the operator-controlled version of Pixie, follow instructions on docs.px.dev to redeploy Pixie with the CLI or Helm Chart. Using the operator version of Pixie is optional and will not affect functionality or data. Note that the operator version of Pixie is not available for air gapped systems."
)

// UpdaterJobYAML is the YAML that should be applied for the updater job.
const UpdaterJobYAML string = `---
apiVersion: batch/v1
kind: Job
metadata:
  name: vizier-upgrade-job
spec:
  template:
    metadata:
      name: vizier-upgrade-job
      labels:
        plane: control
    spec:
      serviceAccountName: pl-updater-service-account
      containers:
      - name: updater
        image: gcr.io/pixie-oss/pixie-prod/vizier-vizier_updater_image
        envFrom:
        - configMapRef:
            name: pl-cloud-config
        - configMapRef:
            name: pl-cluster-config
            optional: true
        env:
        - name: PL_CLOUD_TOKEN
          valueFrom:
            secretKeyRef:
              name: pl-update-job-secrets
              key: cloud-token
        - name: PL_NAMESPACE
          valueFrom:
            fieldRef:
              fieldPath: metadata.namespace
        - name: PL_VIZIER_VERSION
          value: __PL_VIZIER_VERSION__
        - name: PL_REDEPLOY_ETCD
          value: __PL_REDEPLOY_ETCD__
        - name: PL_CLIENT_TLS_CERT
          value: /certs/client.crt
        - name: PL_CLIENT_TLS_KEY
          value: /certs/client.key
        - name: PL_SERVER_TLS_CERT
          value: /certs/server.crt
        - name: PL_SERVER_TLS_KEY
          value: /certs/server.key
        - name: PL_TLS_CA_CERT
          value: /certs/ca.crt
        volumeMounts:
        - name: certs
          mountPath: /certs
      volumes:
      - name: certs
        secret:
          secretName: service-tls-certs
      restartPolicy: "Never"
  backoffLimit: 1
  parallelism: 1
  completions: 1`

const (
	heartbeatIntervalS = 5 * time.Second
	// HeartbeatTopic is the topic that heartbeats are written to.
	HeartbeatTopic                = "heartbeat"
	registrationTimeout           = 30 * time.Second
	passthroughReplySubjectPrefix = "v2c.reply-"
	vizStatusCheckFailInterval    = 10 * time.Second
)

// ErrRegistrationTimeout is the registration timeout error.
var ErrRegistrationTimeout = errors.New("Registration timeout")

const upgradeJobName = "vizier-upgrade-job"

// VizierInfo fetches information about Vizier.
type VizierInfo interface {
	GetVizierClusterInfo() (*cvmsgspb.VizierClusterInfo, error)
	GetK8sState() *K8sState
	ParseJobYAML(yamlStr string, imageTag map[string]string, envSubtitutions map[string]string) (*batchv1.Job, error)
	LaunchJob(j *batchv1.Job) (*batchv1.Job, error)
	CreateSecret(string, map[string]string) error
	WaitForJobCompletion(string) (bool, error)
	DeleteJob(string) error
	GetJob(string) (*batchv1.Job, error)
	GetClusterUID() (string, error)
	UpdateClusterID(string) error
	UpdateClusterName(string) error
	UpdateClusterIDAnnotation(string) error
	GetVizierPodLogs(string, bool, string) (string, error)
	GetVizierPods() ([]*vizierpb.VizierPodStatus, []*vizierpb.VizierPodStatus, error)
}

// VizierOperatorInfo updates and fetches info about the Vizier CRD.
type VizierOperatorInfo interface {
	UpdateCRDVizierVersion(string) (bool, error)
	GetVizierCRD() (*v1alpha1.Vizier, error)
}

// VizierHealthChecker is the interface that gets information on health of a Vizier.
type VizierHealthChecker interface {
	GetStatus() (time.Time, error)
}

// Bridge is the NATS<->GRPC bridge.
type Bridge struct {
	vizierID            uuid.UUID
	assignedClusterName string
	jwtSigningKey       string
	sessionID           int64
	deployKey           string

	vzConnClient vzconnpb.VZConnServiceClient
	vzInfo       VizierInfo
	vzOperator   VizierOperatorInfo
	vizChecker   VizierHealthChecker

	hbSeqNum int64

	nc     *nats.Conn
	natsCh chan *nats.Msg
	// There are a two sets of streams that we manage for the GRPC side. The incoming
	// data and the outgoing data. GRPC does not natively provide a channel based interface
	// so we wrap the Send/Recv calls with goroutines that are responsible for
	// performing the read/write operations.
	//
	// If the GRPC connection gets disrupted, we close all the readers and writer routines, but we leave the
	// channels in place so that data does not get lost. The data will simply be resent
	// once the connection comes back alive. If data is lost due to a crash, the rest of the system
	// is resilient to this loss, but reducing it is optimal to prevent a lot of replay traffic.

	grpcOutCh chan *vzconnpb.V2CBridgeMessage
	grpcInCh  chan *vzconnpb.C2VBridgeMessage
	// Explicitly prioritize passthrough traffic to prevent script failure under load.
	ptOutCh chan *vzconnpb.V2CBridgeMessage
	// This tracks the message we are trying to send, but has not been sent yet.
	pendingGRPCOutMsg *vzconnpb.V2CBridgeMessage

	quitCh chan bool      // Channel is used to signal that things should shutdown.
	wg     sync.WaitGroup // Tracks all the active goroutines.
	wdWg   sync.WaitGroup // Tracks all the active goroutines.

	updateRunning atomic.Value // True if an update is running
	updateFailed  bool         // True if an update has failed (sticky).

	droppedMessagesBeforeResume int64 // Number of messages dropped before successful resume.

	natsMetricsCh chan *nats.Msg
	metricsCh     <-chan *messagespb.MetricsMessage // Channel is used to pass metrics from the scraper to the bridge.
}

// New creates a cloud connector to cloud bridge.
func New(vizierID uuid.UUID, assignedClusterName string, jwtSigningKey string, deployKey string, sessionID int64, vzClient vzconnpb.VZConnServiceClient, vzInfo VizierInfo, vzOperator VizierOperatorInfo, nc *nats.Conn, checker VizierHealthChecker, metricsCh <-chan *messagespb.MetricsMessage) *Bridge {
	return &Bridge{
		vizierID:            vizierID,
		assignedClusterName: assignedClusterName,
		jwtSigningKey:       jwtSigningKey,
		deployKey:           deployKey,
		sessionID:           sessionID,
		vzConnClient:        vzClient,
		vizChecker:          checker,
		vzInfo:              vzInfo,
		vzOperator:          vzOperator,
		hbSeqNum:            0,
		nc:                  nc,
		// Buffer NATS channels to make sure we don't back-pressure NATS
		natsCh:            make(chan *nats.Msg, 5000),
		ptOutCh:           make(chan *vzconnpb.V2CBridgeMessage, 5000),
		grpcOutCh:         make(chan *vzconnpb.V2CBridgeMessage, 5000),
		grpcInCh:          make(chan *vzconnpb.C2VBridgeMessage, 5000),
		pendingGRPCOutMsg: nil,
		quitCh:            make(chan bool),
		wg:                sync.WaitGroup{},
		wdWg:              sync.WaitGroup{},
		natsMetricsCh:     make(chan *nats.Msg, 5000),
		metricsCh:         metricsCh,
	}
}

// WatchDog watches and make sure the bridge is functioning. If not commits suicide to try to self-heal.
func (s *Bridge) WatchDog() {
	defer s.wdWg.Done()
	t := time.NewTicker(10 * time.Minute)

	for {
		lastHbSeq := atomic.LoadInt64(&s.hbSeqNum)
		select {
		case <-s.quitCh:
			log.Trace("Quitting watchdog")
			return
		case <-t.C:
			currentHbSeqNum := atomic.LoadInt64(&s.hbSeqNum)
			if currentHbSeqNum == lastHbSeq {
				log.Fatal("Heartbeat messages failed, assuming stream is dead. Killing self to restart...")
			}
		}
	}
}

// WaitForUpdater waits for the update job to complete, if any.
func (s *Bridge) WaitForUpdater() {
	defer func() {
		s.updateRunning.Store(false)
	}()
	ok, err := s.vzInfo.WaitForJobCompletion(upgradeJobName)
	if err != nil {
		log.WithError(err).Error("Error while trying to watch vizier-upgrade-job")
		return
	}
	s.updateFailed = !ok
	err = s.vzInfo.DeleteJob(upgradeJobName)
	if err != nil {
		log.WithError(err).Error("Error deleting upgrade job")
	}
}

// RegisterDeployment registers the vizier cluster using the deployment key.
func (s *Bridge) RegisterDeployment() error {
	ctx := context.Background()
	ctx = metadata.AppendToOutgoingContext(ctx, "X-API-KEY", s.deployKey)
	clusterInfo, err := s.vzInfo.GetVizierClusterInfo()
	if err != nil {
		return err
	}
	resp, err := s.vzConnClient.RegisterVizierDeployment(ctx, &vzconnpb.RegisterVizierDeploymentRequest{
		K8sClusterUID:  clusterInfo.ClusterUID,
		K8sClusterName: clusterInfo.ClusterName,
	})
	if err != nil {
		return err
	}

	// Get cluster ID and assign to secrets.
	s.vizierID = utils.UUIDFromProtoOrNil(resp.VizierID)
	s.assignedClusterName = resp.VizierName

	err = s.vzInfo.UpdateClusterID(s.vizierID.String())
	if err != nil {
		return err
	}
	return s.vzInfo.UpdateClusterName(resp.VizierName)
}

// RunStream manages starting and restarting the stream to VZConn.
func (s *Bridge) RunStream() {
	s.updateRunning.Store(false)

	if s.vzConnClient == nil {
		var vzClient vzconnpb.VZConnServiceClient
		var err error

		connect := func() error {
			log.Info("Connecting to VZConn in Pixie Cloud...")
			vzClient, err = NewVZConnClient(s.vzOperator)
			if err != nil {
				log.WithError(err).Error(fmt.Sprintf("Failed to connect to Pixie Cloud. Please check your firewall settings and confirm that %s is correct and accessible from your cluster.", viper.GetString("cloud_addr")))
			}
			return err
		}

		backOffOpts := backoff.NewExponentialBackOff()
		backOffOpts.InitialInterval = 30 * time.Second
		backOffOpts.Multiplier = 2
		backOffOpts.MaxElapsedTime = 30 * time.Minute
		err = backoff.Retry(connect, backOffOpts)
		if err != nil {
			log.WithError(err).Fatal(fmt.Sprintf("Failed to connect to Pixie Cloud. Please check your firewall settings and confirm that %s is correct and accessible from your cluster.", viper.GetString("cloud_addr")))
		}
		log.Info("Successfully connected to Pixie Cloud via VZConn")
		s.vzConnClient = vzClient
	}

	if s.nc == nil {
		var nc *nats.Conn
		var err error

		connectNats := func() error {
			log.Info("Connecting to NATS...")
			nc, err = nats.Connect(viper.GetString("nats_url"),
				nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
				nats.RootCAs(viper.GetString("tls_ca_cert")))
			return err
		}

		backOffOpts := backoff.NewExponentialBackOff()
		backOffOpts.InitialInterval = NATSBackoffInitialInterval
		backOffOpts.Multiplier = NATSBackoffMultipler
		backOffOpts.MaxElapsedTime = 10 * time.Minute
		err = backoff.Retry(connectNats, backOffOpts)
		if err != nil {
			log.WithError(err).Fatal("Could not connect to NATS. Please check for the `pl-nats` pods in the namespace to confirm they are healthy and running.")
		}
		log.Info("Successfully connected to NATS")
		s.nc = nc
	}

	s.nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithField("Sub", subscription.Subject).
			WithError(err).
			Error("Error with NATS handler")
	})

	natsTopic := messagebus.V2CTopic("*")
	log.WithField("topic", natsTopic).Trace("Subscribing to NATS")
	natsSub, err := s.nc.ChanSubscribe(natsTopic, s.natsCh)
	if err != nil {
		log.WithError(err).Fatal("Could not subscribe to NATS. Please check for the `pl-nats` pods in the namespace to confirm they are healthy and running.")
	}
	defer func() {
		err := natsSub.Unsubscribe()
		if err != nil {
			log.WithError(err).Error("Failed to unsubscribe from NATS")
		}
	}()

	log.WithField("topic", messagebus.MetricsTopic).Trace("Subscribing to Metrics topic on NATS")
	natsMetricsSub, err := s.nc.ChanSubscribe(messagebus.MetricsTopic, s.natsMetricsCh)
	if err != nil {
		log.WithError(err).Fatal("Could not subscribe to Metrics topic on NATS. Please check for the `pl-nats` pods in the namespace to confirm they are healthy and running.")
	}
	defer func() {
		err := natsMetricsSub.Unsubscribe()
		if err != nil {
			log.WithError(err).Error("Failed to unsubscribe from NATS metrics topic.")
		}
	}()

	// Check if there is an existing update job. If so, then set the status to "UPDATING".
	_, err = s.vzInfo.GetJob(upgradeJobName)
	if err != nil && !k8sErrors.IsNotFound(err) {
		log.WithError(err).Fatal("Could not check for upgrade job")
	}
	if err == nil { // There is an upgrade job running.
		s.updateRunning.Store(true)
		go s.WaitForUpdater()
	}

	// Get the cluster ID, if not already specified.
	if s.vizierID == uuid.Nil {
		err = s.RegisterDeployment()
		if err != nil {
			log.WithError(err).Fatal("Failed to register vizier deployment. If deploying via Helm or Manifest, please check your deployment key. Otherwise, ensure you have deployed with an authorized account.")
		}
	}

	err = s.vzInfo.UpdateClusterIDAnnotation(s.vizierID.String())
	if err != nil {
		log.WithError(err).Error("Error updating cluster ID annotation")
	}

	s.wdWg.Add(1)
	go s.WatchDog()

	for {
		select {
		case <-s.quitCh:
			return
		default:
			log.Trace("Starting stream")
			err := s.StartStream()
			if err == nil {
				log.Trace("Stream ending")
			} else {
				log.WithError(err).Error("Stream errored. Restarting stream")
			}
		}
	}
}

func (s *Bridge) handleUpdateMessage(msg *types.Any) error {
	pb := &cvmsgspb.UpdateOrInstallVizierRequest{}
	err := types.UnmarshalAny(msg, pb)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal update req message")
		return err
	}

	// If cluster is using operator-deployed version of Vizier, we should
	// trigger the update through the CRD. Otherwise, we fallback to the
	// update job.
	updating, err := s.vzOperator.UpdateCRDVizierVersion(pb.Version)
	if err == nil {
		if updating {
			s.updateRunning.Store(true)
		}
	} else {
		job, err := s.vzInfo.ParseJobYAML(UpdaterJobYAML, map[string]string{"updater": pb.Version}, map[string]string{
			"PL_VIZIER_VERSION": pb.Version,
			"PL_REDEPLOY_ETCD":  fmt.Sprintf("%v", pb.RedeployEtcd),
		})
		if err != nil {
			log.WithError(err).Error("Could not parse job")
			return err
		}

		err = s.vzInfo.CreateSecret("pl-update-job-secrets", map[string]string{
			"cloud-token": pb.Token,
		})
		if err != nil {
			log.WithError(err).Error("Failed to create job secrets")
			return err
		}

		_, err = s.vzInfo.LaunchJob(job)
		if err != nil {
			log.WithError(err).Error("Could not launch job")
			return err
		}
		// Set the update status to true while the update job is running.
		s.updateRunning.Store(true)
		// This goroutine waits for the update job to complete. When it does, it sets
		// the updateRunning boolean to false. Normally, if the update has successfully completed,
		// this goroutine won't actually complete because this cloudconnector instance should be
		// replaced by a new cloudconnector instance. This case is here to handle when the
		// update job has failed.
		go s.WaitForUpdater()
	}

	// Send response message to indicate update job has started.
	m := cvmsgspb.UpdateOrInstallVizierResponse{
		UpdateStarted: true,
	}
	reqAnyMsg, err := types.MarshalAny(&m)
	if err != nil {
		return err
	}

	v2cMsg := cvmsgspb.V2CMessage{
		Msg: reqAnyMsg,
	}
	b, err := v2cMsg.Marshal()
	if err != nil {
		return err
	}
	err = s.nc.Publish(messagebus.V2CTopic("VizierUpdateResponse"), b)
	if err != nil {
		log.WithError(err).Error("Failed to publish VizierUpdateResponse")
		return err
	}

	return nil
}

func (s *Bridge) sendPTStatusMessage(reqID string, code codes.Code, message string) {
	topic := fmt.Sprintf("v2c.reply-%s", reqID)

	resp := &cvmsgspb.V2CAPIStreamResponse{
		RequestID: reqID,
		Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
			Status: &vizierpb.Status{
				Code:    int32(code),
				Message: message,
			},
		},
	}
	// Wrap message in V2C message.
	reqAnyMsg, err := types.MarshalAny(resp)
	if err != nil {
		log.WithError(err).Info("Failed to marshal any")
		return
	}
	v2cMsg := cvmsgspb.V2CMessage{
		Msg: reqAnyMsg,
	}
	b, err := v2cMsg.Marshal()
	if err != nil {
		log.WithError(err).Info("Failed to marshal to bytes")
		return
	}

	err = s.nc.Publish(topic, b)
	if err != nil {
		log.WithError(err).Error("Failed to publish PTStatus Message")
	}
}

func (s *Bridge) sendDebugStreamResponse(reqID string, resps []*cvmsgspb.V2CAPIStreamResponse) error {
	topic := fmt.Sprintf("v2c.reply-%s", reqID)

	for _, resp := range resps {
		// Wrap message in V2C message.
		reqAnyMsg, err := types.MarshalAny(resp)
		if err != nil {
			log.WithError(err).Info("Failed to marshal any")
			return err
		}
		v2cMsg := cvmsgspb.V2CMessage{
			Msg: reqAnyMsg,
		}
		b, err := v2cMsg.Marshal()
		if err != nil {
			log.WithError(err).Info("Failed to marshal to bytes")
			return err
		}

		err = s.nc.Publish(topic, b)
		if err != nil {
			return err
		}
	}

	s.sendPTStatusMessage(reqID, codes.OK, "")
	return nil
}

func (s *Bridge) handleDebugLogRequest(reqID string, req *vizierpb.DebugLogRequest) error {
	if req == nil {
		err := status.Errorf(codes.Internal, "DebugLogRequest is unexpectedly nil")
		s.sendPTStatusMessage(reqID, codes.Internal, err.Error())
		return err
	}

	logs, err := s.vzInfo.GetVizierPodLogs(req.PodName, req.Previous, req.Container)
	if err != nil {
		s.sendPTStatusMessage(reqID, codes.NotFound, err.Error())
		return err
	}

	var resps []*cvmsgspb.V2CAPIStreamResponse
	i := 0
	for i*logChunkSize <= len(logs) {
		resps = append(resps, &cvmsgspb.V2CAPIStreamResponse{
			RequestID: reqID,
			Msg: &cvmsgspb.V2CAPIStreamResponse_DebugLogResp{
				DebugLogResp: &vizierpb.DebugLogResponse{
					Data: logs[i*logChunkSize : int(math.Min(float64(len(logs)), float64((i+1)*logChunkSize)))],
				},
			},
		})
		i++
	}

	return s.sendDebugStreamResponse(reqID, resps)
}

func (s *Bridge) handleDebugPodsRequest(reqID string, req *vizierpb.DebugPodsRequest) error {
	if req == nil {
		err := status.Errorf(codes.Internal, "DebugPodsRequest is unexpectedly nil")
		s.sendPTStatusMessage(reqID, codes.Internal, err.Error())
		return err
	}

	ctrlPods, dataPods, err := s.vzInfo.GetVizierPods()
	if err != nil {
		return err
	}

	resps := []*cvmsgspb.V2CAPIStreamResponse{
		{
			RequestID: reqID,
			Msg: &cvmsgspb.V2CAPIStreamResponse_DebugPodsResp{
				DebugPodsResp: &vizierpb.DebugPodsResponse{
					ControlPlanePods: ctrlPods,
					DataPlanePods:    dataPods,
				},
			},
		},
	}
	return s.sendDebugStreamResponse(reqID, resps)
}

func (s *Bridge) handleMetricsMessage(msg *messagespb.MetricsMessage) error {
	promWriteReq, err := vzmetrics.ParsePrometheusTextToWriteReq(msg.PromMetricsText, s.vizierID.String(), msg.PodName)
	if err != nil {
		return err
	}
	anyMsg, err := types.MarshalAny(promWriteReq)
	if err != nil {
		return err
	}
	return s.publishBridgeCh(cvmsgs.VizierMetricsChannel, anyMsg)
}

func (s *Bridge) doRegistrationHandshake(stream vzconnpb.VZConnService_NATSBridgeClient) error {
	clusterInfo, err := s.vzInfo.GetVizierClusterInfo()
	if err != nil {
		log.WithError(err).Error("Unable to get k8s cluster info")
	}
	// Send over a registration request and wait for ACK.
	regReq := &cvmsgspb.RegisterVizierRequest{
		VizierID:    utils.ProtoFromUUID(s.vizierID),
		JwtKey:      s.jwtSigningKey,
		ClusterInfo: clusterInfo,
	}

	err = s.publishBridgeSync(stream, "register", regReq)
	if err != nil {
		return err
	}

	for {
		select {
		case <-stream.Context().Done():
			return errors.New("registration unsuccessful: stream closed before complete")
		case <-time.After(registrationTimeout):
			log.Info("Timeout with registration terminating stream")
			return ErrRegistrationTimeout
		case resp := <-s.grpcInCh:
			// Try to receive the registerAck.
			if resp.Topic != "registerAck" {
				log.Error("Unexpected message type while waiting for ACK")
			}
			registerAck := &cvmsgspb.RegisterVizierAck{}
			err = types.UnmarshalAny(resp.Msg, registerAck)
			if err != nil {
				return err
			}
			switch registerAck.Status {
			case cvmsgspb.ST_FAILED_NOT_FOUND:
				return errors.New("registration not found, cluster unknown in pixie-cloud")
			case cvmsgspb.ST_OK:
				break
			default:
				return errors.New("registration unsuccessful: " + err.Error())
			}

			if s.assignedClusterName == "" {
				// Deliberately not returning the error. We don't want to kill a cluster
				// in case something goes wrong in the update process.
				err = s.vzInfo.UpdateClusterName(registerAck.VizierName)
				if err != nil {
					log.WithError(err).Error("Failed to set the UpdateClusterName")
				}
				s.assignedClusterName = registerAck.VizierName
			}
			return nil
		}
	}
}

// StartStream starts the stream between the cloud connector and Vizier connector.
func (s *Bridge) StartStream() error {
	var stream vzconnpb.VZConnService_NATSBridgeClient
	cancel := func() {}
	// Wait for  all goroutines to terminate.
	defer func() {
		s.wg.Wait()
	}()
	done := make(chan bool)
	defer close(done)

	// We backoff-retry the registration logic but immediately fail the core-logic.
	backOffOpts := backoff.NewExponentialBackOff()
	backOffOpts.InitialInterval = 30 * time.Second
	backOffOpts.Multiplier = 2
	backOffOpts.MaxElapsedTime = 30 * time.Minute
	err := backoff.Retry(func() error {
		select {
		case <-s.quitCh:
			return nil
		default:
		}

		log.Trace("Start Vizier registration")
		var err error
		var ctx context.Context
		ctx, cancel = context.WithCancel(context.Background())
		stream, err = s.vzConnClient.NATSBridge(ctx)
		if err != nil {
			log.WithError(err).Error("Error starting grpc stream")
			cancel()
			return err
		}
		s.wg.Add(1)
		go s.startStreamGRPCReader(stream, done)
		s.wg.Add(1)
		go s.startStreamGRPCWriter(stream, done)

		// Need to do registration handshake before we allow any cvmsgs.
		err = s.doRegistrationHandshake(stream)
		if err != nil {
			log.WithError(err).Error("Error doing registration handshake")
			cancel()
			return err
		}
		log.Trace("Complete Vizier registration")
		return nil
	}, backOffOpts)

	// Defer is placed after backoff because we re-assign the cancel inside the backoff retry.
	defer cancel()
	if err != nil {
		return err
	}

	// Check to see if Stop was called while we waited for the
	// registrationHandshake and if so, skip setting up NATS
	// bridging.
	select {
	case <-s.quitCh:
		return nil
	default:
	}

	s.wg.Add(1)
	err = s.HandleNATSBridging(stream, done)
	if err != nil {
		log.WithError(err).Error("Error inside NATS bridge")
		return err
	}
	return nil
}

func (s *Bridge) startStreamGRPCReader(stream vzconnpb.VZConnService_NATSBridgeClient, done chan bool) {
	defer s.wg.Done()
	log.Trace("Starting GRPC reader stream")
	defer log.Trace("Closing GRPC read stream")
	for {
		select {
		case <-s.quitCh:
			return
		case <-stream.Context().Done():
			return
		case <-done:
			log.Info("Closing GRPC reader because of <-done")
			return
		default:
			log.Trace("Waiting for next message")
			msg, err := stream.Recv()
			if err != nil && err == io.EOF {
				log.Trace("Stream has closed(Read)")
				// stream closed.
				return
			}
			if err != nil && errors.Is(err, context.Canceled) {
				log.Trace("Stream has been cancelled")
				return
			}
			if err != nil {
				log.WithError(err).Trace("Got a stream read error")
				return
			}
			s.grpcInCh <- msg
		}
	}
}

func (s *Bridge) startStreamGRPCWriter(stream vzconnpb.VZConnService_NATSBridgeClient, done chan bool) {
	defer s.wg.Done()
	log.Trace("Starting GRPC writer stream")
	defer log.Trace("Closing GRPC writer stream")

	sendMsg := func(m *vzconnpb.V2CBridgeMessage) {
		// Pending message try to send it first.
		if s.pendingGRPCOutMsg != nil {
			err := stream.Send(s.pendingGRPCOutMsg)
			if err != nil {
				log.WithError(err).Error("Error sending GRPC message")
				return
			}
			s.pendingGRPCOutMsg = nil
		}

		if m != nil {
			// Write message to GRPC if it exists.
			err := stream.Send(m)
			if err != nil {
				// Need to resend this message.
				log.WithError(err).Error("Error sending GRPC message")
				s.pendingGRPCOutMsg = m
				return
			}
		}
	}

	for {
		// If there's a pending message, send it.
		sendMsg(nil)

		// Try to send PT traffic first.
		select {
		case <-s.quitCh:
			return
		case <-stream.Context().Done():
			log.Trace("Write stream has closed")
			return
		case <-done:
			log.Trace("Closing GRPC writer because of <-done")
			err := stream.CloseSend()
			if err != nil {
				log.WithError(err).Error("Failed to CloseSend stream")
			}
			// Quit called.
			return
		case m := <-s.ptOutCh:
			sendMsg(m)
			continue
		default:
		}

		select {
		case <-s.quitCh:
			return
		case <-stream.Context().Done():
			log.Trace("Write stream has closed")
			return
		case <-done:
			log.Trace("Closing GRPC writer because of <-done")
			err := stream.CloseSend()
			if err != nil {
				log.WithError(err).Error("Failed to CloseSend stream")
			}
			// Quit called.
			return
		case m := <-s.ptOutCh:
			sendMsg(m)
		case m := <-s.grpcOutCh:
			sendMsg(m)
		}
	}
}

func (s *Bridge) parseV2CNatsMsg(data *nats.Msg) (*cvmsgspb.V2CMessage, string, error) {
	v2cPrefix := messagebus.V2CTopic("")
	topic := strings.TrimPrefix(data.Subject, v2cPrefix)

	// Message over nats should be wrapped in a V2CMessage.
	v2cMsg := &cvmsgspb.V2CMessage{}
	err := proto.Unmarshal(data.Data, v2cMsg)
	if err != nil {
		return nil, "", err
	}
	return v2cMsg, topic, nil
}

// HandleNATSBridging routes message to and from cloud NATS.
func (s *Bridge) HandleNATSBridging(stream vzconnpb.VZConnService_NATSBridgeClient, done chan bool) error {
	defer s.wg.Done()
	defer log.Info("Closing NATS Bridge. This is expected behavior which happens periodically")
	// Vizier -> Cloud side:
	// 1. Listen to NATS on v2c.<topic>.
	// 2. Extract Topic from the stream name above.
	// 3. Wrap the message and throw it over the wire.
	// 4. Additionally, listen on NATS for messages on the metrics topic, and bridge those to cloud.

	// Cloud -> Vizier side:
	// 1. Read the stream.
	// 2. For cvmsgs of type: C2VBridgeMessage, read the topic
	//    and throw it onto nats under c2v.topic

	log.Info("Starting NATS bridge.")
	hbChan := s.generateHeartbeats(done)

	for {
		select {
		case <-s.quitCh:
			return nil
		case <-done:
			return nil
		case data := <-s.natsCh:
			v2cPrefix := messagebus.V2CTopic("")
			if !strings.HasPrefix(data.Subject, v2cPrefix) {
				err := errors.New("invalid subject: " + data.Subject)
				log.WithError(err).Error("Invalid subject sent to nats channel")
				return err
			}

			v2cMsg, topic, err := s.parseV2CNatsMsg(data)
			if err != nil {
				log.WithError(err).Error("Failed to parse message")
				return err
			}

			if strings.HasPrefix(data.Subject, passthroughReplySubjectPrefix) {
				// Passthrough message.
				err = s.publishPTBridgeCh(topic, v2cMsg.Msg)
				if err != nil {
					return err
				}
			} else {
				err = s.publishBridgeCh(topic, v2cMsg.Msg)
				if err != nil {
					return err
				}
			}
		case bridgeMsg := <-s.grpcInCh:
			if bridgeMsg == nil {
				return nil
			}

			if bridgeMsg.Topic == "VizierUpdate" {
				err := s.handleUpdateMessage(bridgeMsg.Msg)
				if err != nil && !k8sErrors.IsAlreadyExists(err) {
					log.WithError(err).Error("Failed to launch vizier update job")
				}
				continue
			}

			if bridgeMsg.Topic == "VizierPassthroughRequest" {
				pb := &cvmsgspb.C2VAPIStreamRequest{}
				err := types.UnmarshalAny(bridgeMsg.Msg, pb)
				if err != nil {
					log.WithError(err).Error("Could not unmarshal c2v stream req message")
					return err
				}
				switch pb.Msg.(type) {
				case *cvmsgspb.C2VAPIStreamRequest_DebugLogReq:
					err := s.handleDebugLogRequest(pb.RequestID, pb.GetDebugLogReq())
					if err != nil {
						log.WithError(err).Error("Could not handle debug log request")
					}
					continue
				case *cvmsgspb.C2VAPIStreamRequest_DebugPodsReq:
					err := s.handleDebugPodsRequest(pb.RequestID, pb.GetDebugPodsReq())
					if err != nil {
						log.WithError(err).Error("Could not handle debug pods request")
					}
					continue
				default:
				}
			}

			topic := messagebus.C2VTopic(bridgeMsg.Topic)

			natsMsg := &cvmsgspb.C2VMessage{
				VizierID: s.vizierID.String(),
				Msg:      bridgeMsg.Msg,
			}
			b, err := natsMsg.Marshal()
			if err != nil {
				log.WithError(err).Error("Failed to marshal")
				return err
			}

			err = s.nc.Publish(topic, b)
			if err != nil {
				log.WithError(err).Error("Failed to publish")
				return err
			}
		case hbMsg := <-hbChan:
			err := s.publishProtoToBridgeCh(HeartbeatTopic, hbMsg)
			if err != nil {
				return err
			}

		case natsMetricsMsg := <-s.natsMetricsCh:
			metricsMsg := &messagespb.MetricsMessage{}
			err := proto.Unmarshal(natsMetricsMsg.Data, metricsMsg)
			if err != nil {
				log.WithError(err).Error("failed to unmarshal metrics message")
				continue
			}
			err = s.handleMetricsMessage(metricsMsg)
			if err != nil {
				log.WithError(err).Error("failed to bridge metrics message to cloud")
				continue
			}
		case metricsMsg := <-s.metricsCh:
			err := s.handleMetricsMessage(metricsMsg)
			if err != nil {
				log.WithError(err).Error("failed to bridge metrics message to cloud")
				continue
			}

		case <-stream.Context().Done():
			log.Info("Stream has been closed, shutting down grpc readers")
			return nil
		}
	}
}

// Stop terminates the server. Don't reuse this server object after stop has been called.
func (s *Bridge) Stop() {
	close(s.quitCh)
	// Wait fo all goroutines to stop.
	s.wg.Wait()
	s.wdWg.Wait()
}

func (s *Bridge) publishBridgeCh(topic string, msg *types.Any) error {
	wrappedReq := &vzconnpb.V2CBridgeMessage{
		Topic:     topic,
		SessionId: s.sessionID,
		Msg:       msg,
	}

	// Don't stall the queue for regular message.
	select {
	case s.grpcOutCh <- wrappedReq:
		if s.droppedMessagesBeforeResume > 0 {
			log.WithField("Topic", wrappedReq.Topic).
				WithField("droppedCount", s.droppedMessagesBeforeResume).
				Info("Resuming messages again...")
		}
		s.droppedMessagesBeforeResume = 0
	default:
		if (s.droppedMessagesBeforeResume % 100) == 0 {
			log.WithField("Topic", wrappedReq.Topic).
				WithField("droppedCount", s.droppedMessagesBeforeResume).
				Warn("Dropping message because of queue backoff")
		}
		s.droppedMessagesBeforeResume++
	}
	return nil
}

func (s *Bridge) publishPTBridgeCh(topic string, msg *types.Any) error {
	wrappedReq := &vzconnpb.V2CBridgeMessage{
		Topic:     topic,
		SessionId: s.sessionID,
		Msg:       msg,
	}
	s.ptOutCh <- wrappedReq
	return nil
}

func (s *Bridge) publishProtoToBridgeCh(topic string, msg proto.Message) error {
	anyMsg, err := types.MarshalAny(msg)
	if err != nil {
		return err
	}

	return s.publishBridgeCh(topic, anyMsg)
}

func (s *Bridge) publishBridgeSync(stream vzconnpb.VZConnService_NATSBridgeClient, topic string, msg proto.Message) error {
	anyMsg, err := types.MarshalAny(msg)
	if err != nil {
		return err
	}

	wrappedReq := &vzconnpb.V2CBridgeMessage{
		Topic:     topic,
		SessionId: s.sessionID,
		Msg:       anyMsg,
	}

	if err := stream.Send(wrappedReq); err != nil {
		return err
	}
	return nil
}

func crdPhaseToHeartbeatStatus(phase v1alpha1.VizierPhase) cvmsgspb.VizierStatus {
	switch phase {
	case v1alpha1.VizierPhaseDisconnected:
		return cvmsgspb.VZ_ST_DISCONNECTED
	case v1alpha1.VizierPhaseHealthy:
		return cvmsgspb.VZ_ST_HEALTHY
	case v1alpha1.VizierPhaseUpdating:
		return cvmsgspb.VZ_ST_UPDATING
	case v1alpha1.VizierPhaseUnhealthy:
		return cvmsgspb.VZ_ST_UNHEALTHY
	case v1alpha1.VizierPhaseDegraded:
		return cvmsgspb.VZ_ST_DEGRADED
	default:
		return cvmsgspb.VZ_ST_UNKNOWN
	}
}

func (s *Bridge) generateHeartbeats(done <-chan bool) chan *cvmsgspb.VizierHeartbeat {
	hbCh := make(chan *cvmsgspb.VizierHeartbeat)
	crdSeen := false

	sendHeartbeat := func() {
		state := s.vzInfo.GetK8sState()

		// Try to get the status from the Vizier CRD.
		vz, err := s.vzOperator.GetVizierCRD()
		if err != nil && atomic.LoadInt64(&s.hbSeqNum)%128 == 0 {
			log.WithError(err).Warn("Failed to get CRD")
		}

		var msg, operatorVersion string
		status := s.currentStatus()

		if vz != nil {
			crdSeen = true
			msg = vz.Status.Message
			vzStatus := crdPhaseToHeartbeatStatus(vz.Status.VizierPhase)
			if vzStatus != cvmsgspb.VZ_ST_UNKNOWN {
				status = vzStatus
			}
			// We always override our status if the Reconciler is still in the updating phase.
			if vz.Status.ReconciliationPhase == v1alpha1.ReconciliationPhaseUpdating {
				status = cvmsgspb.VZ_ST_UPDATING
			}
			operatorVersion = vz.Status.OperatorVersion
		} else if status == cvmsgspb.VZ_ST_HEALTHY && !crdSeen {
			// If running on non-operator, and is healthy, consider the vizier in a degraded state.
			status = cvmsgspb.VZ_ST_DEGRADED
			msg = operatorMessage
		}

		hbMsg := &cvmsgspb.VizierHeartbeat{
			VizierID:                      utils.ProtoFromUUID(s.vizierID),
			Time:                          time.Now().UnixNano(),
			SequenceNumber:                atomic.LoadInt64(&s.hbSeqNum),
			NumNodes:                      state.NumNodes,
			NumInstrumentedNodes:          state.NumInstrumentedNodes,
			UnhealthyDataPlanePodStatuses: state.UnhealthyDataPlanePodStatuses,
			K8sClusterVersion:             state.K8sClusterVersion,
			PodStatusesLastUpdated:        state.LastUpdated.UnixNano(),
			Status:                        status,
			StatusMessage:                 msg,
			DisableAutoUpdate:             viper.GetBool("disable_auto_update"),
			OperatorVersion:               operatorVersion,
		}

		// Only send the control plane pod statuses every 1 min.
		if atomic.LoadInt64(&s.hbSeqNum)%12 == 0 {
			hbMsg.PodStatuses = state.ControlPlanePodStatuses
		}

		select {
		case <-s.quitCh:
			return
		case <-done:
			return
		case hbCh <- hbMsg:
			atomic.AddInt64(&s.hbSeqNum, 1)
		}
	}

	s.wg.Add(1)
	go func() {
		defer s.wg.Done()
		ticker := time.NewTicker(heartbeatIntervalS)
		defer ticker.Stop()

		// Send first heartbeat.
		sendHeartbeat()

		for {
			select {
			case <-s.quitCh:
				log.Info("Stopping heartbeat routine")
				return
			case <-done:
				log.Info("Stopping heartbeat routine")
				return
			case <-ticker.C:
				sendHeartbeat()
			}
		}
	}()
	return hbCh
}

func (s *Bridge) currentStatus() cvmsgspb.VizierStatus {
	if s.updateRunning.Load().(bool) && !s.updateFailed {
		return cvmsgspb.VZ_ST_UPDATING
	} else if s.updateFailed {
		return cvmsgspb.VZ_ST_UPDATE_FAILED
	}

	t, status := s.vizChecker.GetStatus()
	if time.Since(t) > vizStatusCheckFailInterval {
		return cvmsgspb.VZ_ST_UNKNOWN
	}
	if status != nil {
		return cvmsgspb.VZ_ST_UNHEALTHY
	}
	return cvmsgspb.VZ_ST_HEALTHY
}

// DebugLog is the GRPC stream method to fetch debug logs from vizier.
func (s *Bridge) DebugLog(req *vizierpb.DebugLogRequest, srv vizierpb.VizierDebugService_DebugLogServer) error {
	logs, err := s.vzInfo.GetVizierPodLogs(req.PodName, req.Previous, req.Container)
	if err != nil {
		return err
	}
	i := 0
	// Repeated logic from handleDebugLogRequest.
	for i*logChunkSize <= len(logs) {
		err := srv.Send(&vizierpb.DebugLogResponse{
			Data: logs[i*logChunkSize : int(math.Min(float64(len(logs)), float64((i+1)*logChunkSize)))],
		})
		if err != nil {
			return err
		}
	}
	return nil
}

// DebugPods is the GRPC method to fetch the list of Vizier pods (and statuses) from a cluster.
func (s *Bridge) DebugPods(req *vizierpb.DebugPodsRequest, srv vizierpb.VizierDebugService_DebugPodsServer) error {
	ctrlPods, dataPods, err := s.vzInfo.GetVizierPods()
	if err != nil {
		return err
	}
	err = srv.Send(&vizierpb.DebugPodsResponse{
		ControlPlanePods: ctrlPods,
		DataPlanePods:    dataPods,
	})
	if err != nil {
		return err
	}
	return nil
}

// GetStatus returns a reason for the current state of the cloud bridge.
// If an empty string is returned, assume healthy.
func (s *Bridge) GetStatus() vzstatus.VizierReason {
	if s.vzConnClient == nil {
		return vzstatus.CloudConnectorFailedToConnect
	}

	if s.vizierID == uuid.Nil {
		return vzstatus.CloudConnectorRegistering
	}
	// TODO(michellenguyen): Add status reasons for whether the bridge stream has started/stopped successfully.
	return ""
}
