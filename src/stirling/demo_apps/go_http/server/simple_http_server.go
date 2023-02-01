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

package main

import (
	"fmt"
	"log"
	"math/rand"
	"net/http"
	"strconv"
	"time"
)

var letterRunes = []rune("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")
var cachedRespData []string
var mIterationsPerMs float64

func init() {
	// Compute the cached response data, generating randoms strings is slow
	// and we don't want to add to critical path of the benchmark.
	for i := 0; i < 4096; i++ {
		cachedRespData = append(cachedRespData, randStringRunes(i))
	}

	// Compute and print out iterations/ms of the burn function.
	startTime := time.Now()
	cyclesToTest := int64(1000000)
	res := burnCyclesE(cyclesToTest)
	d := time.Since(startTime)
	iterationsPerMs := float64(cyclesToTest) / (float64(d.Nanoseconds()) / 1.0e6)
	mIterationsPerMs = iterationsPerMs
	fmt.Printf("MIterations/Ms: %.2f, e=%.2f\n", mIterationsPerMs/1.0e6, res)
}

// burnCyclesE computes the approximation of e by running a fixed number of iterations.
func burnCyclesE(iterations int64) float64 {
	res := 2.0
	fact := 1.0

	for i := int64(2); i < iterations; i++ {
		fact *= float64(i)
		res += 1 / fact
	}
	return res
}

func randStringRunes(n int) string {
	b := make([]rune, n)
	for i := range b {
		b[i] = letterRunes[rand.Intn(len(letterRunes))]
	}
	return string(b)
}

func min(x, y int64) int64 {
	if x < y {
		return x
	}
	return y
}

func fakeLoad(w *http.ResponseWriter, latency float64, mIters, respSize int64) float64 {
	// add some jitters
	// latency = rand.NormFloat64()*latency/10 + latency
	if latency > 0 {
		_ = burnCyclesE(int64(mIterationsPerMs * latency))
	}
	_ = burnCyclesE(mIters)

	bytesSent := int64(0)
	maxCachedSize := int64(len(cachedRespData))
	for bytesSent < respSize {
		bytesToWrite := min(respSize, maxCachedSize)
		fmt.Fprint(*w, cachedRespData[bytesToWrite])
		bytesSent += bytesToWrite
	}
	return latency
}

func main() {
	count := 0
	// Returns a response for /ping. Responses with 200 (pong) 70% of the tim.
	// Status 500 5% and status 401 25% of the time.
	http.HandleFunc("/ping", func(w http.ResponseWriter, r *http.Request) {
		count++
		// Random number from 0 - 100.
		randInt := rand.Intn(100)
		if randInt < 5 {
			// 5 % probability.
			w.WriteHeader(http.StatusInternalServerError)
			fmt.Fprintf(w, "badness-internal")
			return
		}
		if randInt < 30 {
			// 25 % probability.
			w.WriteHeader(http.StatusForbidden)
			fmt.Fprintf(w, "badness-forbid")
			return
		}
		// (70%) Good.
		fmt.Fprintf(w, "pong")
	})

	// Simple function used for benchmarking. It accepts 3 HTTP parameters:
	// latency: Attempt to compute for specified number of milliseconds.
	// miters: Run millions of iterations of compute for computing e. Mutually exclusive with latency.
	// response_size: The response size in bytes (must be less than 4096).
	http.HandleFunc("/bm", func(w http.ResponseWriter, r *http.Request) {
		count++
		// The default latency in ms.
		latency := 0.0
		var err error

		latencyArg, ok := r.URL.Query()["latency"]
		if ok {
			latency, err = strconv.ParseFloat(latencyArg[0], 64)
			if err != nil {
				fmt.Println("Got a latency arg, but failed to parse")
			}
		}

		mItersArg, ok := r.URL.Query()["miters"]
		var mIters int64 = 0
		if ok {
			mItersMultipler, err := strconv.ParseFloat(mItersArg[0], 64)
			if err != nil {
				fmt.Print("Got a latency arg, but failed to parse")
			}
			mIters = int64(mItersMultipler * 1000000)
		}

		respSizeArg, ok := r.URL.Query()["response_size"]
		// The default response size in bytes.
		var respSize int64 = 10
		if ok {
			respSize, err = strconv.ParseInt(respSizeArg[0], 10, 64)
			if err != nil {
				fmt.Print("Got a response_size arg, but failed to parse")
			}
		}

		fakeLoad(&w, latency, mIters, respSize)
	})
	http.HandleFunc("/count", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "count %d", count)
		fmt.Println("asked for count", count)
	})

	log.Fatal(http.ListenAndServe(":9090", nil))
}
