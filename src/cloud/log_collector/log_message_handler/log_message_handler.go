package logmessagehandler

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"sync"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
)

// LogMessageHandler is the component that subscribes to the NATS log channel
// and writes log messages to file
type LogMessageHandler struct {
	ch      chan *nats.Msg
	nc      *nats.Conn
	es      *elastic.Client
	esCtx   context.Context
	wg      *sync.WaitGroup
	indexes map[string]struct{}
	sub     *nats.Subscription
}

// Note: we have to force the mapping to use "text" for the time field
// in log_processed, because some logs have time in date format and some
// have time in "%dms" format.
// TODO(james): Probably should put this into some sort of migration job eventually.
const indexMapping = `
{
	"mappings" : {
		"properties" : {
			"log_processed" : {
				"properties" : {
					"time" : {
						"type" : "text"
					}
				}
			}
		}
	}
}`

// IndexPrefix is the prefix for all log indices in elastic.
// The full index is of the format <IndexPrefix>-<VizierID>
const IndexPrefix = "vizier-logs"
const bufferSize = 5000

// NewLogMessageHandler creates a new handler for log messages.
func NewLogMessageHandler(esCtx context.Context, nc *nats.Conn, es *elastic.Client) *LogMessageHandler {
	h := &LogMessageHandler{
		ch:      make(chan *nats.Msg, bufferSize),
		nc:      nc,
		es:      es,
		esCtx:   esCtx,
		wg:      &sync.WaitGroup{},
		indexes: make(map[string]struct{}),
	}
	return h
}

func (h *LogMessageHandler) createIndexIfNeeded(vizID string) (string, error) {
	index := fmt.Sprintf("%s-%s", IndexPrefix, vizID)
	_, existsInCache := h.indexes[index]
	if !existsInCache {

		exists, err := h.es.IndexExists(index).Do(h.esCtx)
		if err != nil {
			return "", err
		}
		var empty struct{}
		if !exists {
			resp, err := h.es.CreateIndex(index).BodyString(indexMapping).Do(h.esCtx)
			if err != nil {
				return "", err
			}
			if !resp.Acknowledged {
				return "", fmt.Errorf("Elastic failed to create index: %s", index)
			}
			h.indexes[index] = empty
		} else {
			h.indexes[index] = empty
		}
		return index, nil
	}
	return index, nil
}

// SanitizeJSONForElastic replaces "." with "_" in json keys.
func (h *LogMessageHandler) SanitizeJSONForElastic(j map[string]interface{}) map[string]interface{} {
	newJSON := make(map[string]interface{})
	for k, v := range j {
		// Elastic considers "." in keys to refer to object attrs so replace any "." with "_"
		key := strings.ReplaceAll(k, ".", "_")
		switch v.(type) {
		case map[string]interface{}:
			newJSON[key] = h.SanitizeJSONForElastic(v.(map[string]interface{}))
		default:
			newJSON[key] = v
		}
	}
	return newJSON
}

func (h *LogMessageHandler) convertLogMsgToJSON(logMsg *cvmsgspb.VLogMessage) (map[string]interface{}, error) {
	j := make(map[string]interface{})
	if err := json.Unmarshal(logMsg.Data, &j); err != nil {
		return nil, err
	}
	sanitizedJSON := h.SanitizeJSONForElastic(j)
	return sanitizedJSON, nil
}

func unmarshalNATSMsg(natsMsg *nats.Msg) (string, *cvmsgspb.VLogMessage, error) {
	msg := &cvmsgspb.V2CMessage{}
	err := proto.Unmarshal(natsMsg.Data, msg)
	if err != nil {
		return "", nil, err
	}

	logMsg := &cvmsgspb.VLogMessage{}
	err = proto.Unmarshal(msg.GetMsg().Value, logMsg)
	if err != nil {
		return "", nil, err
	}
	return msg.VizierID, logMsg, nil
}

func (h *LogMessageHandler) handleNatsMessage(natsMsg *nats.Msg) {
	vizID, logMsg, err := unmarshalNATSMsg(natsMsg)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal NATS msg")
	}

	index, err := h.createIndexIfNeeded(vizID)
	if err != nil {
		log.WithError(err).Error("Failed to create index")
		return
	}

	JSON, err := h.convertLogMsgToJSON(logMsg)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal json.")
		return
	}

	_, err = h.es.Index().
		Index(index).
		BodyJson(JSON).
		Do(h.esCtx)

	if err != nil {
		log.WithError(err).WithFields(log.Fields{
			"VizierID": vizID,
			"Subject":  natsMsg.Subject,
		}).Error("Failed to add logmessage to index")
		return
	}

	marshaledJSON, err := json.Marshal(JSON)
	if err != nil {
		return
	}
	log.WithFields(log.Fields{
		"VizierID": vizID,
		"JSON":     marshaledJSON,
	}).Trace("Handled log message")
}

func (h *LogMessageHandler) run() {
	h.wg.Add(1)
	for msg := range h.ch {
		h.handleNatsMessage(msg)
	}

	h.wg.Done()
}

func (h *LogMessageHandler) subscribeToNatsChannels() {
	sub, err := h.nc.ChanSubscribe("v2c.*.*.log", h.ch)
	if err != nil {
		log.WithError(err).Fatal("Could not subscribe to v2c log NATS channels")
	}
	h.sub = sub

	h.nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithField("Sub", subscription.Subject).
			WithError(err).
			Error("NATS Error")
	})
}

// Start initializes the handler, starts the handler goroutine and then returns.
func (h *LogMessageHandler) Start() {
	h.subscribeToNatsChannels()
	go h.run()
}

// Stop unsubscribes from Nats and then closes the channel and waits for all messages to be handled.
func (h *LogMessageHandler) Stop() {
	if h.sub != nil {
		h.sub.Unsubscribe()
		h.sub = nil
	}
	close(h.ch)
	h.wg.Wait()
	indexNames := make([]string, len(h.indexes))
	var i int
	for index := range h.indexes {
		indexNames[i] = index
		i++
	}
	_, err := h.es.Refresh().Index(indexNames...).Do(h.esCtx)
	if err != nil {
		log.WithError(err).Error("Failed to refresh indices")
	}

}
