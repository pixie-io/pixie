package main

import (
	"fmt"
	"log"
	"os"
	"time"

	"golang.org/x/net/context"
	"google.golang.org/grpc"
	pb "pixielabs.ai/pixielabs/src/stirling/testing/proto"
)

const (
	address     = "localhost:50051"
	defaultName = "world"
)

func connectAndGreet() {
	// Set up a connection to the server.
	conn, err := grpc.Dial(address, grpc.WithInsecure())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := pb.NewGreeterClient(conn)

	// Contact the server and print out its response.
	name := defaultName
	if len(os.Args) > 1 {
		name = os.Args[1]
	}
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
	stop := schedule(connectAndGreet, 500*time.Millisecond)
	time.Sleep(60 * time.Second)
	stop <- true
	time.Sleep(1 * time.Second)
	fmt.Println("Test Done")
}
