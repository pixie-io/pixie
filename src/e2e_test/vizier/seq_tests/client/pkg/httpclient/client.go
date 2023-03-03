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

package httpclient

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"
	"golang.org/x/time/rate"

	"px.dev/pixie/src/e2e_test/util"
)

// HTTPSeqClient creates a new client that generate sequences of messages.
type HTTPSeqClient struct {
	addr        string
	startSeq    int
	numMessages int
	numConns    int
	reqSize     int
	respSize    int
	targetRPS   int

	rps        float64
	rpsLimiter *rate.Limiter
}

// New creates a new HTTP seq client.
func New(addr string, startSeq, numMessages, numConns, reqSize, respSize, targetRPS int) *HTTPSeqClient {
	return &HTTPSeqClient{
		addr:        addr,
		startSeq:    startSeq,
		numMessages: numMessages,
		numConns:    numConns,
		reqSize:     reqSize,
		respSize:    respSize,
		targetRPS:   targetRPS,
		rpsLimiter:  rate.NewLimiter(rate.Limit(targetRPS), targetRPS),
	}
}

// Run runs the sequence client.
func (c *HTTPSeqClient) Run() error {
	var wg sync.WaitGroup
	jobs := make(chan int, c.numConns)
	results := make(chan error, c.numConns)

	for i := 0; i < c.numConns; i++ {
		wg.Add(1)
		go c.worker(&wg, jobs, results)
	}

	var readerWg sync.WaitGroup
	readerWg.Add(1)
	go func() {
		defer readerWg.Done()
		count := 0
		for r := range results {
			count++
			if r != nil {
				log.WithError(r).
					Error("Failed to make request")
			}
			if count%1000 == 0 {
				log.WithField("count", count).Info("Requests checkpoint")
			}
		}
	}()

	timeStart := time.Now()
	for i := c.startSeq; i <= c.startSeq+c.numMessages; i++ {
		jobs <- i
	}
	close(jobs)

	wg.Wait()
	close(results)
	readerWg.Wait()
	timeDelta := time.Since(timeStart)

	rps := float64(c.numMessages) / timeDelta.Seconds()
	c.rps = rps

	return nil
}

// PrintStats writes request information to the screen.
func (c *HTTPSeqClient) PrintStats() error {
	log.WithField("rps", c.rps).Info("Done making requests")
	return nil
}

func (c *HTTPSeqClient) worker(wg *sync.WaitGroup, jobs <-chan int, results chan<- error) {
	defer wg.Done()

	d := net.Dialer{
		Timeout:   30 * time.Second,
		KeepAlive: 30 * time.Second,
	}
	u, err := url.Parse(c.addr)
	if err != nil {
		results <- err
		return
	}
	conn, err := d.DialContext(context.Background(), "tcp", u.Host)
	if err != nil {
		results <- err
		return
	}
	defer conn.Close()

	t := &http.Transport{
		DialContext: func(context.Context, string, string) (net.Conn, error) {
			return conn, nil
		},
	}
	client := &http.Client{
		Transport: t,
	}

	for j := range jobs {
		if err := c.rpsLimiter.Wait(context.Background()); err != nil {
			results <- err
			continue
		}
		results <- makeSingleRequest(client, c.addr, j, c.reqSize, c.respSize)
	}
}

func makeSingleRequest(client *http.Client, addr string, seqID, reqSize, respSize int) error {
	body := struct {
		SeqID          int    `json:"seq_id"`
		BodySize       int    `json:"body_size"`
		RequestPadData string `json:"request_pad_data"`
	}{
		SeqID:          seqID,
		BodySize:       respSize,
		RequestPadData: string(util.RandPrintable(reqSize)),
	}

	var buf bytes.Buffer
	err := json.NewEncoder(&buf).Encode(body)
	if err != nil {
		return err
	}

	req, err := http.NewRequest("POST", addr, &buf)
	if err != nil {
		return err
	}

	req.Header.Set("content-type", "application/json")
	req.Header.Add("x-px-seq-id", fmt.Sprintf("%d", seqID))

	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if len(respBody) < respSize {
		return fmt.Errorf("expected response body to be at least %d, it is %d", respSize, len(respBody))
	}
	return nil
}
