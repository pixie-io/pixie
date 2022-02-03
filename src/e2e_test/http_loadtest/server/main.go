/*
 * Copyright © 2018- Pixie Labs Inc.
 * Copyright © 2020- New Relic, Inc.
 * All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of New Relic Inc. and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Pixie Labs Inc. and its suppliers and
 * may be covered by U.S. and Foreign Patents, patents in process,
 * and are protected by trade secret or copyright law. Dissemination
 * of this information or reproduction of this material is strictly
 * forbidden unless prior written permission is obtained from
 * New Relic, Inc.
 *
 * SPDX-License-Identifier: Proprietary
 */

package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
)

type httpContent struct {
	headers map[string]string
	body    string
}

func buildHTTPContent(numBytesHeaders int, numBytesBody int, char string) *httpContent {
	headers := make(map[string]string)
	// TODO(james): add random headers.
	return &httpContent{
		body:    strings.Repeat(char, numBytesBody),
		headers: headers,
	}
}

func makeSimpleServeFunc(numBytesHeaders int, numBytesBody int) http.HandlerFunc {
	content := buildHTTPContent(numBytesHeaders, numBytesBody, "s")
	return func(w http.ResponseWriter, r *http.Request) {
		// Force content to not be chunked.
		w.Header().Set("Content-Length", fmt.Sprintf("%d", len(content.body)))
		_, err := fmt.Fprint(w, content.body)
		if err != nil {
			log.Println("error")
		}
	}
}

func makeChunkedServeFunc(numBytesHeaders int, numBytesBody int, numChunks int) http.HandlerFunc {
	content := buildHTTPContent(numBytesHeaders, numBytesBody, "c")
	chunkedBody := make([]string, numChunks)
	chunkSize := len(content.body) / numChunks
	for i := 0; i < numChunks-1; i++ {
		chunkedBody[i] = content.body[i*chunkSize : (i+1)*chunkSize]
	}
	chunkedBody[numChunks-1] = content.body[(numChunks-1)*chunkSize:]

	return func(w http.ResponseWriter, r *http.Request) {
		flusher, ok := w.(http.Flusher)
		if !ok {
			panic("http.ResponseWriter should be an http.Flusher")
		}
		for _, chunk := range chunkedBody {
			_, err := fmt.Fprint(w, chunk)
			if err != nil {
				log.Println("error")
			}
			flusher.Flush()
		}
	}
}

func main() {
	numBytesHeaders, err := strconv.Atoi(os.Getenv("NUM_BYTES_HEADERS"))
	if err != nil {
		log.Fatalln("Must specify valid integer NUM_BYTES_HEADERS in environment")
	}
	numBytesBody, err := strconv.Atoi(os.Getenv("NUM_BYTES_BODY"))
	if err != nil {
		log.Fatalln("Must specify valid integer NUM_BYTES_BODY in environment")
	}

	http.HandleFunc("/", makeSimpleServeFunc(numBytesHeaders, numBytesBody))
	http.HandleFunc("/chunked", makeChunkedServeFunc(numBytesHeaders, numBytesBody, 10))

	port := os.Getenv("PORT")
	if err := http.ListenAndServe(fmt.Sprintf(":%s", port), nil); err != nil {
		panic("HTTP server failed")
	}
}
