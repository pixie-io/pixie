package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"time"

	"google.golang.org/grpc"
	pb "pixielabs.ai/pixielabs/demos/applications/hipster_shop/proto"
)

// This is a placeholder for generating ad-hoc RPC calls to Hipster Shop services.
// Take a look at demos/applications/hipster_shop/proto/demo.proto for all available services.
// The service you want to call also needs to expose an external IP, which can be done with kubectl.

func main() {
	address := flag.String("address", "", "The address of the service.")

	flag.Parse()

	if len(*address) == 0 {
		log.Fatalf("-address must be provided.")
	}

	conn, err := grpc.Dial(*address, grpc.WithInsecure())
	if err != nil {
		log.Fatalf("Failed to connect %s, error message: %v", *address, err)
	}
	defer conn.Close()

	c := pb.NewShippingServiceClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	r, err := c.GetQuote(ctx, &pb.GetQuoteRequest{
		Address: &pb.Address{
			StreetAddress: "300 ABC",
			City:          "SF",
			State:         "CA",
			Country:       "USA",
			ZipCode:       12345,
		},
	})
	if err != nil {
		log.Fatalf("Failed to call PlaceOrder, error message: %v", err)
	}
	fmt.Println(r)
}
