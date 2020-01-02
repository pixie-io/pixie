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
)

type helloReply struct {
	Greeter string `json:"greeter"`
}

func main() {
	address := flag.String("address", "localhost:50050", "Server end point.")
	name := flag.String("name", "world", "The name to greet.")

	flag.Parse()

	resp, err := http.Get("http://" + *address + "/sayhello?name=" + url.QueryEscape(*name))
	if err != nil {
		panic(err)
	}
	defer resp.Body.Close()

	body, readErr := ioutil.ReadAll(resp.Body)
	if readErr != nil {
		log.Fatal(readErr)
	}

	reply := helloReply{}
	jsonErr := json.Unmarshal(body, &reply)
	if jsonErr != nil {
		log.Fatal(jsonErr)
	}

	fmt.Println(reply.Greeter)
}
