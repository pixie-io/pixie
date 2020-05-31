package vizhealth

import (
	"context"
	"fmt"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/metadata"
	utils2 "pixielabs.ai/pixielabs/src/shared/services/utils"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

// Checker runs a remote health check to make sure that Vizier is in a queryable state.
type Checker struct {
	quitCh     chan bool
	signingKey string
	vzClient   pl_api_vizierpb.VizierServiceClient
	wg         sync.WaitGroup

	hcRestMu         sync.Mutex
	latestHCResp     *pl_api_vizierpb.HealthCheckResponse
	latestHCRespTime time.Time
}

// NewChecker creates and starts a Vizier Checker. Stop must be called when done to prevent leaking
// goroutines making requests to Vizier.
func NewChecker(signingKey string, vzClient pl_api_vizierpb.VizierServiceClient) *Checker {
	c := &Checker{
		quitCh:     make(chan bool),
		signingKey: signingKey,
		vzClient:   vzClient,
	}
	c.wg.Add(1)
	go c.run()
	return c
}

func (c *Checker) run() {
	defer c.wg.Done()
	for {
		select {
		case <-c.quitCh:
			log.Trace("Quitting health monitor")
			return
		default:
		}

		claims := utils2.GenerateJWTForService("cloud_conn")
		token, _ := utils2.SignJWTClaims(claims, c.signingKey)

		ctx := context.Background()
		ctx = metadata.AppendToOutgoingContext(context.Background(), "authorization",
			fmt.Sprintf("bearer %s", token))
		ctx, cancel := context.WithCancel(ctx)
		defer cancel()

		resp, err := c.vzClient.HealthCheck(ctx, &pl_api_vizierpb.HealthCheckRequest{})
		if err != nil {
			c.updateHCResp(nil)
			// Try again a bit later.
			log.Error("Failed to run health check, will retry in a bit")
			time.Sleep(5 * time.Second)
			continue
		}

		go func() {
			for {
				hc, err := resp.Recv()
				if hc == nil {
					// Channel closed.
					c.updateHCResp(nil)
					return
				}
				if err != nil {
					// Got an error. Return and wait for restart logic.
					log.WithError(err).Error("Got error during Vizier Health Check")
					c.updateHCResp(nil)
					return
				}
				c.updateHCResp(hc)
			}
		}()

		select {
		case <-c.quitCh:
			log.Trace("Quitting health monitor")
			cancel()
			return
		case <-resp.Context().Done():
			if resp.Context().Err() != nil {
				log.WithError(resp.Context().Err()).Error("Got context close, will try to resume ...")
			} else {
				log.Info("Got context close, will try to resume ...")
			}
			continue
		}
	}
}

// Stop the checker.
func (c *Checker) Stop() {
	close(c.quitCh)
	c.wg.Wait()
	c.updateHCResp(nil)
}

func (c *Checker) updateHCResp(resp *pl_api_vizierpb.HealthCheckResponse) {
	c.hcRestMu.Lock()
	defer c.hcRestMu.Unlock()

	c.latestHCResp = resp
	c.latestHCRespTime = time.Now()
}

// GetStatus returns the current status and the last check time.
func (c *Checker) GetStatus() (time.Time, error) {
	c.hcRestMu.Lock()
	defer c.hcRestMu.Unlock()
	checkTime := c.latestHCRespTime

	if c.latestHCResp == nil {
		return checkTime, fmt.Errorf("missing vizier healthcheck response")
	}

	if c.latestHCResp.Status.Code == 0 {
		return checkTime, nil
	}
	return checkTime, fmt.Errorf("vizier healthcheck is failing: %s", c.latestHCResp.Status.Message)
}
