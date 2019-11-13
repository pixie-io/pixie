package logwriter

import (
	"context"
	log "github.com/sirupsen/logrus"
	"io"
	"sync"
	"time"

	"google.golang.org/grpc"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/cloud_connectorpb"
)

const (
	maxQueueSize     int           = 20               // Max queue size before invoking a flush
	maxRetentionTime time.Duration = 10 * time.Second // Max time interval between flushes
)

// SetupLogWriter Used to set up a logger that writes to both standard out and the cloud connector
// so that logs can be forwarded to Pixie cloud.
func SetupLogWriter(cloudConnAddr, pod, svc string) (io.Writer, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	conn, err := grpc.Dial(cloudConnAddr, dialOpts...)
	if err != nil {
		return nil, err
	}
	client := cloud_connectorpb.NewCloudConnectorServiceClient(conn)
	return NewCloudLogWriter(client, cloudConnAddr, pod, svc, maxQueueSize, maxRetentionTime)
}

// CloudLogWriter implements io.Writer, and is used by Vizier services to forward their logs to Pixie Cloud.
type CloudLogWriter struct {
	cloudConnectorAddr string
	podName            string
	serviceName        string
	stream             cloud_connectorpb.CloudConnectorService_TransferLogClient
	msgChannel         chan *cloud_connectorpb.LogMessage
	maxQueued          int
	maxRetentionTime   time.Duration
	closeWg            sync.WaitGroup
}

// NewCloudLogWriter constructs a CloudLogWriter and starts the log-forwarding goroutine.
func NewCloudLogWriter(client cloud_connectorpb.CloudConnectorServiceClient, addr, pod, svc string, maxQueued int, maxRetentionTime time.Duration) (*CloudLogWriter, error) {
	stream, err := client.TransferLog(context.Background())
	if err != nil {
		log.WithError(err).Errorf("Error opening TransferLog stream to CloudConnector %s", addr)
		return nil, err
	}

	var wg sync.WaitGroup
	writer := &CloudLogWriter{
		cloudConnectorAddr: addr,
		podName:            pod,
		serviceName:        svc,
		stream:             stream,
		msgChannel:         make(chan *cloud_connectorpb.LogMessage),
		maxQueued:          maxQueued,
		maxRetentionTime:   maxRetentionTime,
		closeWg:            wg,
	}

	go writer.forwardLogs()
	return writer, nil
}

// Close When called, all queued log messages will be flushed.
func (w *CloudLogWriter) Close() {
	w.msgChannel <- nil
	w.closeWg.Wait()
}

func (w *CloudLogWriter) forwardLogs() {
	w.closeWg.Add(1)

	var msgs []*cloud_connectorpb.LogMessage
	lastSent := time.Now()

	for {
		nextMsg := <-w.msgChannel
		closeChannel := nextMsg == nil

		if !closeChannel {
			msgs = append(msgs, nextMsg)
		}
		currentTime := time.Now()

		if closeChannel || len(msgs) > w.maxQueued || currentTime.Sub(lastSent) > w.maxRetentionTime {
			if len(msgs) > 0 {
				err := w.stream.Send(&cloud_connectorpb.TransferLogRequest{
					BatchedLogs: msgs,
				})
				if err != nil {
					log.WithError(err).Errorf("Error forwarding logs to cloud connector at address %s", w.cloudConnectorAddr)
				}
			}

			msgs = nil
			lastSent = currentTime
		}
		if closeChannel {
			w.closeWg.Done()
			return
		}
	}
}

// Write implements io.Writer so that CloudWriter can be a destination for logrus.
func (w *CloudLogWriter) Write(p []byte) (n int, err error) {
	w.msgChannel <- &cloud_connectorpb.LogMessage{
		Pod: w.podName,
		Svc: w.serviceName,
		Log: string(p),
	}

	return len(p), nil
}
