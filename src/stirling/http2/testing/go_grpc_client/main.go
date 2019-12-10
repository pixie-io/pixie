package main

import (
	"crypto/tls"
	"flag"
	"fmt"
	"log"
	"time"

	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	pb "pixielabs.ai/pixielabs/src/stirling/http2/testing/proto"
)

func connectAndGreet(address string, https bool, name string) {
	// Set up a connection to the server.
	var conn *grpc.ClientConn
	var err error
	if https {
		tlsConfig := &tls.Config{InsecureSkipVerify: true}
		creds := credentials.NewTLS(tlsConfig)
		conn, err = grpc.Dial(address, grpc.WithTransportCredentials(creds))
	} else {
		conn, err = grpc.Dial(address, grpc.WithInsecure())
	}
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
	address := flag.String("address", "localhost:50051", "Server end point.")
	once := flag.Bool("once", false, "If true, send one request and wait for response and exit.")
	name := flag.String("name", "world", "The name to greet.")
	https := flag.Bool("https", false, "If true, uses https.")

	flag.Parse()

	if *once {
		connectAndGreet(*address, *https, *name)
		return
	}

	stop := schedule(func() { connectAndGreet(*address, *https, *name) }, 500*time.Millisecond)
	time.Sleep(60 * time.Second)
	stop <- true
	time.Sleep(1 * time.Second)
	fmt.Println("Test Done")
}
