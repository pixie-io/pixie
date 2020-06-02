package logmessagehandler

import (
	"context"
	"encoding/json"
	"strings"
	"sync"
	"time"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
)

// LogMessageHandler is the component that subscribes to the NATS log channel
// and writes log messages to file
type LogMessageHandler struct {
	ch        chan *nats.Msg
	jsonCh    chan []byte
	quitCh    chan bool
	nc        *nats.Conn
	es        *elastic.Client
	esCtx     context.Context
	wg        *sync.WaitGroup
	sub       *nats.Subscription
	indexName string
	stats     *logMessageHandlerStats
}

const bufferSize = 5000

// Number of log messages to group into single bulk index request.
const indexBlockSize = 1000

// NewLogMessageHandler creates a new handler for log messages.
func NewLogMessageHandler(esCtx context.Context, nc *nats.Conn, es *elastic.Client, indexName string) *LogMessageHandler {
	h := &LogMessageHandler{
		ch:        make(chan *nats.Msg, bufferSize),
		jsonCh:    make(chan []byte, bufferSize),
		quitCh:    make(chan bool),
		nc:        nc,
		es:        es,
		esCtx:     esCtx,
		wg:        &sync.WaitGroup{},
		indexName: indexName,
		stats:     newStats(),
	}
	return h
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
	start := time.Now()
	vizID, logMsg, err := unmarshalNATSMsg(natsMsg)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal NATS msg")
	}

	JSON, err := h.convertLogMsgToJSON(logMsg)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal json.")
		return
	}
	JSON["cluster_id"] = vizID

	jsonBytes, err := json.Marshal(JSON)
	if err != nil {
		log.WithError(err).Error("Failed to marshal json.")
		return
	}

	select {
	case h.jsonCh <- jsonBytes:
		h.stats.Prepped(time.Since(start))
	default:
		h.stats.Dropped()
	}
}

func (h *LogMessageHandler) runNATSHandler() {
	h.wg.Add(1)
	defer h.wg.Done()

	for msg := range h.ch {
		h.handleNatsMessage(msg)
	}
	close(h.jsonCh)
}

func (h *LogMessageHandler) doBulkIndex(b *elastic.BulkService) {
	numActions := b.NumberOfActions()
	start := time.Now()
	resp, err := b.Do(h.esCtx)
	if err != nil {
		log.WithError(err).
			WithField("index", h.indexName).
			Error("Failed to bulk index log messages.")
		return
	}
	if len(resp.Failed()) > 0 {
		numActions -= len(resp.Failed())
		log.WithField("index", h.indexName).
			WithField("failed items", resp.Failed()).
			Error("Some items of the bulk index failed.")
	}
	if numActions > 0 {
		h.stats.Indexed(numActions, time.Since(start))
	}
}

func (h *LogMessageHandler) runElasticBulkIndexer() {
	h.wg.Add(1)
	defer h.wg.Done()

	timeout := time.NewTicker(5 * time.Second)
	currentBulkReq := h.es.Bulk()

forLoop:
	for {
		doReq := false
		select {
		case <-h.quitCh:
			break forLoop
		case jsonBytes := <-h.jsonCh:
			currentBulkReq.Add(elastic.NewBulkIndexRequest().
				Doc(string(jsonBytes)).
				Index(h.indexName))
		case <-timeout.C:
			if currentBulkReq.NumberOfActions() > 0 {
				doReq = true
			}
		}

		if currentBulkReq.NumberOfActions() >= indexBlockSize {
			doReq = true
		}

		if doReq {
			h.doBulkIndex(currentBulkReq)
			currentBulkReq = h.es.Bulk()
		}
	}

	// If quitCh receives a message, then jsonCh will be closed, so we can loop over the remaining
	// jsonBytes in the channel.
	for jsonBytes := range h.jsonCh {
		currentBulkReq.Add(elastic.NewBulkIndexRequest().
			Doc(string(jsonBytes)).
			Index(h.indexName))
	}

	if currentBulkReq.NumberOfActions() > 0 {
		h.doBulkIndex(currentBulkReq)
	}
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
	go h.runNATSHandler()
	go h.runElasticBulkIndexer()
	go h.stats.Run()
}

// Stop unsubscribes from Nats and then closes the channel and waits for all messages to be handled.
func (h *LogMessageHandler) Stop() {
	if h.sub != nil {
		h.sub.Unsubscribe()
		h.sub = nil
	}
	close(h.ch)
	h.quitCh <- true
	h.wg.Wait()
	_, err := h.es.Refresh().Index(h.indexName).Do(h.esCtx)
	if err != nil {
		log.WithError(err).Error("Failed to refresh indices")
	}
}

type logMessageHandlerStats struct {
	numIndexCalls int
	numIndexed    int
	numPrepped    int
	numDropped    int
	timePrepping  time.Duration
	timeIndexing  time.Duration
	mux           sync.Mutex
}

func newStats() *logMessageHandlerStats {
	s := &logMessageHandlerStats{}
	return s.reset()
}

func (s *logMessageHandlerStats) reset() *logMessageHandlerStats {
	s.mux.Lock()
	defer s.mux.Unlock()
	s.numIndexCalls = 0
	s.numIndexed = 0
	s.numPrepped = 0
	s.numDropped = 0
	s.timePrepping = time.Duration(0)
	s.timeIndexing = time.Duration(0)
	return s
}

func (s *logMessageHandlerStats) Indexed(numIndexed int, dur time.Duration) {
	s.mux.Lock()
	defer s.mux.Unlock()
	s.numIndexCalls++
	s.numIndexed += numIndexed
	s.timeIndexing += dur
}

func (s *logMessageHandlerStats) Dropped() {
	s.mux.Lock()
	defer s.mux.Unlock()
	s.numDropped++
}

func (s *logMessageHandlerStats) Prepped(dur time.Duration) {
	s.mux.Lock()
	defer s.mux.Unlock()
	s.numPrepped++
	s.timePrepping += dur
}

func (s *logMessageHandlerStats) PrintStats() {
	s.mux.Lock()
	defer s.mux.Unlock()

	var avgPrepTime time.Duration
	if s.numPrepped == 0 {
		avgPrepTime = time.Duration(0)
	} else {
		avgPrepTime = s.timePrepping / time.Duration(s.numPrepped)
	}
	var avgIndexTime time.Duration
	if s.numIndexCalls == 0 {
		avgIndexTime = time.Duration(0)
	} else {
		avgIndexTime = s.timeIndexing / time.Duration(s.numIndexCalls)
	}

	log.WithField("avg_prep_time_ms", avgPrepTime.Milliseconds()).
		WithField("avg_index_time_ms", avgIndexTime.Milliseconds()).
		WithField("prepped_msgs", s.numPrepped).
		WithField("indexed_msgs", s.numIndexed).
		WithField("dropped_msgs", s.numDropped).
		Info("log_message_handler stats")
}

func (s *logMessageHandlerStats) Run() {
	ticker := time.NewTicker(30 * time.Second)

	for range ticker.C {
		s.PrintStats()
		s.reset()
	}
}
