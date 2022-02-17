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
	"compress/gzip"
	"crypto/tls"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"

	"px.dev/pixie/src/e2e_test/util"
)

const (
	headerSizeParam      = "header_size"
	numHeadersParam      = "num_headers"
	bodySizeParam        = "body_size"
	chunkSizeParam       = "chunk_size"
	disableChunkingParam = "disable_chunking"

	defaultNumHeaders  = 8
	defaultHeadersSize = 128
	defaultBodySize    = 1024
	defaultChunkSize   = 128
)

func getQueryParamAsInt(values url.Values, param string) int {
	val, err := strconv.Atoi(values.Get(param))
	if err != nil {
		switch param {
		case headerSizeParam:
			val = defaultHeadersSize
		case bodySizeParam:
			val = defaultBodySize
		case chunkSizeParam:
			val = defaultChunkSize
		case numHeadersParam:
			val = defaultNumHeaders
		}
	}
	return val
}

func checkExists(haystack []string, needle string) bool {
	for _, it := range haystack {
		if needle == it {
			return true
		}
	}
	return false
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
		if _, ok := r.URL.Query()[disableChunkingParam]; ok {
			next(w, r)
			return
		}
		w.Header().Add("Transfer-Encoding", "chunked")
		chunkSize := getQueryParamAsInt(r.URL.Query(), chunkSizeParam)
		next(chunkWriter{chunkSize: chunkSize, ResponseWriter: w}, r)
	}
}

func basicHandler(w http.ResponseWriter, r *http.Request) {
	headerSize := getQueryParamAsInt(r.URL.Query(), headerSizeParam)
	bodySize := getQueryParamAsInt(r.URL.Query(), bodySizeParam)
	numHeaders := getQueryParamAsInt(r.URL.Query(), numHeadersParam)

	w.Header().Set("x-px-ack-seq-id", r.Header.Get("x-px-seq-id"))
	for i := 0; i < numHeaders; i++ {
		w.Header().Add(fmt.Sprintf("X-Custom-Header-%d", i), string(util.RandPrintable(headerSize)))
	}

	w.Header().Set("Content-Type", "text/plain")
	w.Header().Add("Content-Length", fmt.Sprintf("%d", bodySize))

	_, err := w.Write(util.RandPrintable(bodySize))
	if err != nil {
		http.Error(w, "write failed", http.StatusInternalServerError)
	}
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
	http.HandleFunc("/", chunkMiddleware(gzipMiddleware(basicHandler)))
	// SSL port is optional
	if sslPort != "" && tlsConfig != nil {
		go setupHTTPSServer(tlsConfig, sslPort)
	}
	setupHTTPServer(port)
}
