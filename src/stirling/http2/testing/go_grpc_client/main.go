package main

import (
	"flag"
	"fmt"
	"log"
	"time"

	"golang.org/x/net/context"
	"google.golang.org/grpc"
	pb "pixielabs.ai/pixielabs/src/stirling/http2/testing/proto"
)

const (
	address     = "localhost:50051"
	defaultName = "world"
)

func connectAndGreet(name string) {
	// Set up a connection to the server.
	conn, err := grpc.Dial(address, grpc.WithInsecure())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := pb.NewGreeterClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	r, err := c.SayHello(ctx, &pb.HelloRequest{Name: name})
	if err != nil {
		log.Fatalf("could not greet: %v", err)
	} else {
		log.Printf("Greeting: %s", r.Message)
	}
}

func schedule(what func(), delay time.Duration) chan bool {
	stop := make(chan bool)

	go func() {
		for {
			what()
			select {
			case <-time.After(delay):
			case <-stop:
				return
			}
		}
	}()

	return stop
}

func main() {
	once := flag.Bool("once", false, "If true, send one request and wait for response and exit.")
	name := flag.String("name", "world", "The name to greet.")

	flag.Parse()

	if *once {
		connectAndGreet(*name)
		return
	}

	stop := schedule(func() { connectAndGreet(*name) }, 500*time.Millisecond)
	time.Sleep(60 * time.Second)
	stop <- true
	time.Sleep(1 * time.Second)
	fmt.Println("Test Done")
}
