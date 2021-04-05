package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
)

func BasicHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Add("Content-Type", "application/json")
	_, err := io.WriteString(w, `{"status":"ok"}`)
	if err != nil {
		log.Fatal(err)
	}
}

func ListenAndServeTLS(port int, certFile, keyFile string) {
	log.Printf("Starting HTTPS service on Port %d", port)
	err := http.ListenAndServeTLS(fmt.Sprintf(":%d", port), certFile, keyFile, nil)
	if err != nil {
		log.Fatal(err)
	}
}

func ListenAndServe(port int) {
	log.Printf("Starting HTTP service on Port %d", port)
	err := http.ListenAndServe(fmt.Sprintf(":%d", port), nil)
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	http_port := 50100
	https_port := 50101
	http.HandleFunc("/", BasicHandler)

	var cert = flag.String("cert", "", "Path to the .crt file.")
	var key = flag.String("key", "", "Path to the .key file.")
	const keyPairBase = "demos/client_server_apps/go_https/server"

	certFile := keyPairBase + "/https-server.crt"
	if len(*cert) > 0 {
		certFile = *cert
	}
	keyFile := keyPairBase + "/https-server.key"
	if len(*key) > 0 {
		keyFile = *key
	}

	fmt.Printf("cert: %s\n", certFile)
	fmt.Printf("key: %s\n", certFile)

	go ListenAndServeTLS(https_port, certFile, keyFile)
	ListenAndServe(http_port)
}
