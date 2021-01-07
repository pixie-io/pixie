// The intention to have matching http client in Go, is to mimic the typical setup of calling a restful service in Go.
// Obviously, we can use 'curl' to send a http request. But that would not be how typical Go client works.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"net/url"
	"time"
)

type helloReply struct {
	Greeter string `json:"greeter"`
}

func main() {
	address := flag.String("address", "localhost:50050", "Server end point.")
	name := flag.String("name", "world", "The name to greet.")
	count := flag.Int("count", 1, "The count of requests to make.")

	flag.Parse()

	for i := 0; i < *count; i++ {
		resp, err := http.Get("http://" + *address + "/sayhello?name=" + url.QueryEscape(*name))
		if err != nil {
			panic(err)
		}

		body, readErr := ioutil.ReadAll(resp.Body)
		resp.Body.Close()
		if readErr != nil {
			log.Fatal(readErr)
		}

		reply := helloReply{}
		jsonErr := json.Unmarshal(body, &reply)
		if jsonErr != nil {
			log.Fatal(jsonErr)
		}

		fmt.Println(reply.Greeter)
		time.Sleep(time.Second)
	}
}
