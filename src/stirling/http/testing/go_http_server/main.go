package main

import (
	"encoding/json"
	"flag"
	"log"
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
	json.NewEncoder(w).Encode(reply)
}

func main() {
	port := flag.Int("port", 50050, "The port number to serve.")
	http.HandleFunc("/sayhello", sayHello)
	err := http.ListenAndServe(":"+strconv.Itoa(*port), nil)
	if err != nil {
		log.Fatal(err)
	}
}
