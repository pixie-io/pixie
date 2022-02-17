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
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"

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

	rps float64
}

// New creates a new HTTP seq client.
func New(addr string, startSeq, numMessages, numConns, reqSize, respSize int) *HTTPSeqClient {
	return &HTTPSeqClient{
		addr:        addr,
		startSeq:    startSeq,
		numMessages: numMessages,
		numConns:    numConns,
		reqSize:     reqSize,
		respSize:    respSize,
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

	for j := range jobs {
		results <- makeSingleRequest(c.addr, j, c.reqSize, c.respSize)
	}
}

func makeSingleRequest(addr string, seqID, reqSize, respSize int) error {
	body := struct {
		SeqID          int    `json:"seq_id"`
		RespSize       int    `json:"resp_size"`
		RequestPadData string `json:"request_pad_data"`
	}{
		SeqID:          seqID,
		RespSize:       respSize,
		RequestPadData: string(util.RandPrintable(reqSize)),
	}

	var buf bytes.Buffer
	err := json.NewEncoder(&buf).Encode(body)
	if err != nil {
		return err
	}

	client := &http.Client{}
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

	respBody, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if len(respBody) < respSize {
		return fmt.Errorf("expected response body to be at least %d, it is %d", respSize, len(respBody))
	}
	return nil
}
