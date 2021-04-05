package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"strconv"
)

type helloReply struct {
	Greeter string `json:"greeter"`
}

func sayHello(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "json")
	name := "world"
	nameArgs, ok := r.URL.Query()["name"]
	if ok {
		name = nameArgs[0]
	}
	reply := helloReply{Greeter: "Hello " + name + "!"}
	err := json.NewEncoder(w).Encode(reply)
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	port := flag.Int("port", 0, "The port number to serve.")
	flag.Parse()

	listener, err := net.Listen("tcp", ":"+strconv.Itoa(*port))
	if err != nil {
		log.Fatal(err)
	}

	fmt.Print(listener.Addr().(*net.TCPAddr).Port)

	http.HandleFunc("/sayhello", sayHello)
	err = http.Serve(listener, nil)
	if err != nil {
		log.Fatal(err)
	}
}
