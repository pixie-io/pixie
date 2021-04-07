package main

import (
	"crypto/tls"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"runtime"
	"time"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

const (
	address = "https://127.0.0.1:50101"
)

// Use this HTTPS client with https_server.go, which listens on HTTPS port 50101.

func main() {
	pflag.Int("max_procs", 1, "The maximum number of OS threads created by the golang runtime.")
	pflag.Int("iters", 1000, "Number of iterations.")
	pflag.Int("sub_iters", 1000, "Number of sub-iterations with same TLS config.")
	pflag.Parse()

	viper.BindPFlags(pflag.CommandLine)

	runtime.GOMAXPROCS(viper.GetInt("max_procs"))

	for i := 0; i < viper.GetInt("iters"); i++ {
		tr := &http.Transport{
			TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		}
		client := &http.Client{Transport: tr}

		for j := 0; j < viper.GetInt("sub_iters"); j++ {
			resp, err := client.Get(address)
			if err != nil {
				fmt.Println(err)
			}

			body, err := ioutil.ReadAll(resp.Body)
			if err != nil {
				log.Fatalln(err)
			} else {
				log.Println(string(body))
			}
			time.Sleep(time.Second)
		}
	}
}
