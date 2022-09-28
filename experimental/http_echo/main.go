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
	"crypto/md5"
	crand "crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"strconv"
	"sync/atomic"
	"time"
)

var numProcessingExpesive int64

type handleParams struct {
	w    http.ResponseWriter
	r    *http.Request
	done chan struct{}
}

func simpleHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "Hello!\n")
	_, _ = io.Copy(w, r.Body)
}

func rsaHandler(w http.ResponseWriter, r *http.Request) {
	numBytes, _ := strconv.Atoi(r.URL.Query().Get("num_rsa_bytes"))
	if numBytes == 0 {
		numBytes = 2048
	}

	privKey, _ := rsa.GenerateKey(crand.Reader, numBytes)
	pubKey, _ := x509.MarshalPKIXPublicKey(&privKey.PublicKey)

	fmt.Fprintf(w, "Hello! (%x)\n", md5.Sum(pubKey))
}

func main() {
	http.HandleFunc("/", simpleHandler)

	http.HandleFunc("/expensive", rsaHandler)

	http.HandleFunc("/expensive-limit-concurrent", func(w http.ResponseWriter, r *http.Request) {
		if atomic.LoadInt64(&numProcessingExpesive) > 10 {
			http.Error(w, "Too many requests", http.StatusTooManyRequests)
			return
		}
		v := atomic.AddInt64(&numProcessingExpesive, 1)
		fmt.Fprintf(w, "numProcessingExpesive[%d]:\n", v)
		rsaHandler(w, r)
		atomic.AddInt64(&numProcessingExpesive, -1)
	})

	slowReqs := make(chan handleParams)
	for i := 0; i < 10; i++ {
		go func() {
			for {
				p := <-slowReqs
				time.Sleep(time.Duration(rand.Intn(10)) * 100 * time.Millisecond)
				simpleHandler(p.w, p.r)
				close(p.done)
			}
		}()
	}
	http.HandleFunc("/slow-contention", func(w http.ResponseWriter, r *http.Request) {
		done := make(chan struct{})
		slowReqs <- handleParams{w, r, done}
		<-done
	})

	log.Fatal(http.ListenAndServe(":8080", nil))
}
