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

package http

import (
	"bytes"
	"compress/gzip"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"io"
	"net/http"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/collectors"
	"github.com/prometheus/client_golang/prometheus/promhttp"

	"px.dev/pixie/src/e2e_test/util"
)

const (
	defaultNumHeaders = 8
	defaultHeaderSize = 128
	defaultBodySize   = 1024
	defaultChunkSize  = 128
)

// LoadParams defines the post params accepted by the server.
type LoadParams struct {
	HeaderSize      int  `json:"header_size,omitempty"`
	NumHeaders      int  `json:"num_headers,omitempty"`
	BodySize        int  `json:"body_size,omitempty"`
	ChunkSize       int  `json:"chunk_size,omitempty"`
	DisableChunking bool `json:"disable_chunking,omitempty"`
}

func getDefaultParams() *LoadParams {
	return &LoadParams{
		NumHeaders:      defaultNumHeaders,
		HeaderSize:      defaultHeaderSize,
		BodySize:        defaultBodySize,
		ChunkSize:       defaultChunkSize,
		DisableChunking: false,
	}
}

func checkExists(haystack []string, needle string) bool {
	for _, it := range haystack {
		if needle == it {
			return true
		}
	}
	return false
}

func duplicateReadCloser(r io.ReadCloser) (io.ReadCloser, io.ReadCloser, error) {
	data, err := io.ReadAll(r)
	if err != nil {
		return nil, nil, err
	}
	return io.NopCloser(bytes.NewReader(data)), io.NopCloser(bytes.NewReader(data)), nil
}

// Gzip handling adapted from https://gist.github.com/the42/1956518
type gzipResponseWriter struct {
	io.Writer
	http.ResponseWriter
}

func (w gzipResponseWriter) Write(b []byte) (int, error) {
	// gzipping changes Content-Length.
	w.ResponseWriter.Header().Del("Content-Length")
	return w.Writer.Write(b)
}

func gzipMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if !checkExists(r.Header.Values("Accept-Encoding"), "gzip") {
			next(w, r)
			return
		}
		w.Header().Add("Content-Encoding", "gzip")
		gz := gzip.NewWriter(w)
		defer gz.Close()
		next(gzipResponseWriter{Writer: gz, ResponseWriter: w}, r)
	}
}

type chunkWriter struct {
	chunkSize int
	http.ResponseWriter
}

func (w chunkWriter) Write(b []byte) (int, error) {
	// Setting Content-Length causes the inbuilt server to disable chunking.
	w.ResponseWriter.Header().Del("Content-Length")
	total := 0
	for i := 0; i < len(b); i += w.chunkSize {
		end := i + w.chunkSize
		if end > len(b) {
			end = len(b)
		}
		c, err := w.ResponseWriter.Write(b[i:end])
		total += c
		if err != nil {
			return total, err
		}
		flusher, ok := w.ResponseWriter.(http.Flusher)
		if !ok {
			return total, fmt.Errorf("response writer is not a flusher")
		}
		flusher.Flush()
	}
	return total, nil
}

func chunkMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Body == nil {
			http.Error(w, "missing form body", http.StatusBadRequest)
			return
		}
		b1, b2, err := duplicateReadCloser(r.Body)
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		r.Body = b1
		params := getDefaultParams()
		err = json.NewDecoder(b2).Decode(params)
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		if params.DisableChunking {
			next(w, r)
			return
		}
		w.Header().Add("Transfer-Encoding", "chunked")
		next(chunkWriter{chunkSize: params.ChunkSize, ResponseWriter: w}, r)
	}
}

func basicHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "expected post request", http.StatusMethodNotAllowed)
		return
	}
	if r.Header.Get("Content-Type") != "application/json" {
		http.Error(w, "only supports json post data", http.StatusUnsupportedMediaType)
		return
	}
	b1, b2, err := duplicateReadCloser(r.Body)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	r.Body = b1
	params := getDefaultParams()
	err = json.NewDecoder(b2).Decode(params)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("X-Px-Ack-Seq-Id", r.Header.Get("X-Px-Seq-Id"))
	for i := 0; i < params.NumHeaders; i++ {
		w.Header().Add(
			fmt.Sprintf("X-Custom-Header-%d", i), string(util.RandPrintable(params.HeaderSize)),
		)
	}

	w.Header().Set("Content-Type", "text/plain")
	w.Header().Add("Content-Length", fmt.Sprintf("%d", params.BodySize))

	_, _ = w.Write(util.RandPrintable(params.BodySize))
}

func setupHTTPSServer(tlsConfig *tls.Config, port string) {
	ln, err := tls.Listen("tcp", fmt.Sprintf(":%s", port), tlsConfig)
	if err != nil {
		panic(fmt.Sprintf("HTTP TLS listen failed: %v", err))
	}
	fmt.Println("Serving HTTPS")
	if err = http.Serve(ln, nil); err != nil {
		panic(fmt.Sprintf("HTTP TLS serve failed: %v", err))
	}
}

func setupHTTPServer(port string) {
	fmt.Println("Serving HTTP")
	if err := http.ListenAndServe(fmt.Sprintf(":%s", port), nil); err != nil {
		panic(fmt.Sprintf("HTTP server failed: %v", err))
	}
}

// RunHTTPServers sets up and runs the SSL and non-SSL HTTP server with the provided parameters.
func RunHTTPServers(tlsConfig *tls.Config, port, sslPort string) {
	r := prometheus.NewRegistry()
	r.MustRegister(collectors.NewProcessCollector(collectors.ProcessCollectorOpts{}))
	http.Handle("/metrics", promhttp.HandlerFor(r, promhttp.HandlerOpts{}))

	http.HandleFunc("/", chunkMiddleware(gzipMiddleware(basicHandler)))
	// SSL port is optional
	if sslPort != "" && tlsConfig != nil {
		go setupHTTPSServer(tlsConfig, sslPort)
	}
	setupHTTPServer(port)
}
