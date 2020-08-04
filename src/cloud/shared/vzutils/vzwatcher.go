package vzutils

import (
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"

	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
)

// VizierHandlerFn is the function signature for the function that is run for active and newly connected Viziers.
type VizierHandlerFn func(id uuid.UUID, orgID uuid.UUID, uid string, resourceVersion string) error

// ErrorHandlerFn is the function signature for the function that should be called when the VizierHandlerFn returns
// an error.
type ErrorHandlerFn func(id uuid.UUID, orgID uuid.UUID, uid string, resourceVersion string, err error)

// Watcher tracks active Viziers and executes the registered task for each Vizier.
type Watcher struct {
	nc          *nats.Conn
	vzmgrClient vzmgrpb.VZMgrServiceClient

	vizierHandlerFn VizierHandlerFn
	errorHandlerFn  ErrorHandlerFn

	quitCh chan bool
}

// NewWatcher creates a new vizier watcher.
func NewWatcher(nc *nats.Conn, vzmgrClient vzmgrpb.VZMgrServiceClient) *Watcher {
	vw := &Watcher{
		nc:          nc,
		vzmgrClient: vzmgrClient,
		quitCh:      make(chan bool),
	}

	go vw.runWatch()

	return vw
}

// runWatch subscribes to the NATS channel for any newly connected viziers, and executes the registered task for
// each.
func (w *Watcher) runWatch() {
}

// RegisterVizierHandler registers the function that should be called on all currently active Viziers, and any newly
// connected Viziers.
func (w *Watcher) RegisterVizierHandler(fn VizierHandlerFn) error {
	// Make an RPC call to Vzmgr to get all active Viziers. Call the VizierHandlerFn on all those viziers.

	// Set w.registeredFn = fn, so that runWatch calls fn for each newly connected vizier.
	return nil
}

// RegisterErrorHandler registers the function that should be called when the VizierHandler returns an error.
func (w *Watcher) RegisterErrorHandler(fn ErrorHandlerFn) {

}

// Stop stops the watcher.
func (w *Watcher) Stop() {
	close(w.quitCh)
}
