package main

import (
	"fmt"
	"io"
	"log"
	"net/http"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

const (
	httpPort  = 50100
	httpsPort = 50101
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
	pflag.String("cert", "", "Path to the .crt file.")
	pflag.String("key", "", "Path to the .key file.")
	pflag.Parse()

	viper.BindPFlags(pflag.CommandLine)

	http.HandleFunc("/", BasicHandler)

	go ListenAndServeTLS(httpsPort, viper.GetString("cert"), viper.GetString("key"))
	ListenAndServe(httpPort)
}
