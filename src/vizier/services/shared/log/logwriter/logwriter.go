package logwriter

import (
	"context"
	"fmt"
	"sync"
	"time"

	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/cloud_connectorpb"
)

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
func NewCloudLogWriter(client cloud_connectorpb.CloudConnectorServiceClient, addr, pod, svc string, maxQueued int, maxRetentionTime time.Duration) *CloudLogWriter {
	stream, err := client.TransferLog(context.Background())
	if err != nil {
		// Use fmt here, because logrus is using this writer component.
		fmt.Printf("Error opening TransferLog stream to CloudConnector %s: %s", addr, err.Error())
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
	return writer
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
					// Use fmt here, because logrus is using this writer component.
					fmt.Printf("Error forwarding logs to cloud connector at address %s: %s", w.cloudConnectorAddr, err.Error())
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
