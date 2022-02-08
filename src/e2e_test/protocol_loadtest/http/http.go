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
	"crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"io"
	"log"
	"math/big"
	"net/http"
	"os"
	"strings"
	"time"
)

// Gzip handling adapted from https://gist.github.com/the42/1956518
type gzipResponseWriter struct {
	io.Writer
	http.ResponseWriter
}

func (w gzipResponseWriter) Write(b []byte) (int, error) {
	return w.Writer.Write(b)
}

func optionallyGzipMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if !strings.Contains(r.Header.Get("Accept-Encoding"), "gzip") {
			next(w, r)
			return
		}
		w.Header().Set("Content-Encoding", "gzip")
		w.Header().Set("Content-Type", "text/plain")
		gz := gzip.NewWriter(w)
		defer gz.Close()
		gzr := gzipResponseWriter{Writer: gz, ResponseWriter: w}
		next(gzr, r)
	}
}

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
		bytesWritten, err := fmt.Fprint(w, content.body)
		w.Header().Set("Content-Length", fmt.Sprintf("%d", bytesWritten))
		if err != nil {
			log.Println("error")
		}
	}
}

// Chunked+GZip not currently supported.
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

const (
	bitsize  = 4096
	certFile = "server.crt"
	keyFile  = "server.key"
)

var x509Name = pkix.Name{
	Organization: []string{"Pixie Labs Inc."},
	Country:      []string{"US"},
	Province:     []string{"California"},
	Locality:     []string{"San Francisco"},
}

func generateCertFiles(dnsNames []string) (string, string, error) {
	ca := &x509.Certificate{
		SerialNumber:          big.NewInt(1653),
		Subject:               x509Name,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(10, 0, 0),
		IsCA:                  true,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature | x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
	}
	caKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		return "", "", err
	}
	cert := &x509.Certificate{
		SerialNumber:          big.NewInt(1658),
		Subject:               x509Name,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(10, 0, 0),
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		DNSNames:              dnsNames,
	}
	privateKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		return "", "", err
	}
	certBytes, err := x509.CreateCertificate(rand.Reader, cert, ca, &privateKey.PublicKey, caKey)

	if err != nil {
		return "", "", err
	}

	certData := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certBytes})
	if err != nil {
		return "", "", err
	}
	if err = os.WriteFile(certFile, certData, 0666); err != nil {
		return "", "", err
	}

	keyData := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(privateKey)})
	if err != nil {
		return "", "", err
	}
	if err = os.WriteFile(keyFile, keyData, 0666); err != nil {
		return "", "", err
	}

	return certFile, keyFile, nil
}

func setupHTTPServer(ssl bool, port string, numBytesHeaders, numBytesBody int) {
	if ssl {
		certFile, keyFile, err := generateCertFiles([]string{"localhost"})
		if err != nil {
			panic(fmt.Sprintf("Could not create cert files: %s", err.Error()))
		}
		if err := http.ListenAndServeTLS(fmt.Sprintf(":%s", port), certFile, keyFile, nil); err != nil {
			panic(fmt.Sprintf("HTTP TLS server failed: %s", err.Error()))
		}
	} else {
		if err := http.ListenAndServe(fmt.Sprintf(":%s", port), nil); err != nil {
			panic(fmt.Sprintf("HTTP server failed: %s", err.Error()))
		}
	}
}

// RunHTTPServers sets up and runs the SSL and non-SSL HTTP server with the provided parameters.
// TODO(nserrino):  PP-3238  Remove numBytesHeaders/numBytesBody and make it a parameter passed
// in by the HTTP request so that we don't have to redeploy.
func RunHTTPServers(port, sslPort string, numBytesHeaders, numBytesBody int) {
	http.HandleFunc("/", optionallyGzipMiddleware(makeSimpleServeFunc(numBytesHeaders, numBytesBody)))
	http.HandleFunc("/chunked", makeChunkedServeFunc(numBytesHeaders, numBytesBody, 10))
	// SSL port is optional
	if sslPort != "" {
		go setupHTTPServer(true, sslPort, numBytesHeaders, numBytesBody)
	}
	setupHTTPServer(false, port, numBytesHeaders, numBytesBody)
}
